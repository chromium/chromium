// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrIconButtonElement, LanguageHelper, SettingsAddLanguagesDialogElement, SettingsTranslatePageElement} from 'chrome://settings/lazy_load.js';
import {LanguageHelperImpl, LanguagesBrowserProxyImpl, getLanguageHelperInstance} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
// clang-format on

suite('TranslatePage', function() {
  let languageHelper: LanguageHelper;
  let translatePage: SettingsTranslatePageElement;
  let browserProxy: TestLanguagesBrowserProxy;

  const translateTarget = 'translate_recent_target';
  // Always Translate language pref name for the platform.
  const alwaysTranslatePref = 'translate_allowlists';
  const neverTranslatePref = 'translate_blocked_languages';

  suiteSetup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getFakeLanguagePrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    // Set up test browser proxy.
    browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstance(browserProxy);

    LanguageHelperImpl.resetInstanceForTesting();
    languageHelper = getLanguageHelperInstance();
    await languageHelper.whenReady();

    translatePage = document.createElement('settings-translate-page');
    translatePage.languages = languageHelper.languages;
    languageHelper.addEventListener('languages-changed', (e: Event) => {
      translatePage.languages = (e as CustomEvent).detail;
    });

    document.body.appendChild(translatePage);
    flush();
  });

  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  suite('TranslateSettings', function() {
    test('change target language', function() {
      const targetLanguageSelector =
          translatePage.shadowRoot!.querySelector<HTMLSelectElement>(
              '#targetLanguage');
      assertTrue(!!targetLanguageSelector);

      assertEquals(
          targetLanguageSelector.value,
          PrefService.getInstance().getPref(translateTarget).value);

      targetLanguageSelector.value = 'sw';
      targetLanguageSelector.dispatchEvent(new CustomEvent('change'));

      assertEquals(
          PrefService.getInstance().getPref(translateTarget).value, 'sw');
    });

    test('never translate display', function() {
      // Disable a language not in fake_language_settings_private. The language
      // should not be shown in the never translate list.
      languageHelper.disableTranslateLanguage('eo');
      flush();

      const neverTranslateDiv =
          translatePage.shadowRoot!.querySelector<HTMLElement>(
              '#neverTranslateList');
      assertTrue(!!neverTranslateDiv);

      // Only one language should be shown in the UI.
      let listItems =
          neverTranslateDiv.querySelectorAll<HTMLElement>('.list-item');
      assertEquals(1, listItems.length);

      // But two should be in the preference (since en-US is the default).
      assertDeepEquals(
          ['en-US', 'eo'],
          PrefService.getInstance().getPref(neverTranslatePref).value);

      // Disable a language that is in fake_language_settings_private. The
      // language should be shown in the never translate list.
      languageHelper.disableTranslateLanguage('nb');
      flush();

      // Two items should now be shown.
      listItems = neverTranslateDiv.querySelectorAll<HTMLElement>('.list-item');
      assertEquals(2, listItems.length);

      // But three should be on the never translate list
      assertDeepEquals(
          ['en-US', 'eo', 'nb'],
          PrefService.getInstance().getPref(neverTranslatePref).value);
    });

    test('always translate display', function() {
      // Add a language not in fake_language_settings_private. The language
      // should not be shown in the always translate list.
      languageHelper.setLanguageAlwaysTranslateState('eo', true);
      flush();

      const alwaysTranslateDiv =
          translatePage.shadowRoot!.querySelector<HTMLElement>(
              '#alwaysTranslateList');
      assertTrue(!!alwaysTranslateDiv);

      // No languages should be shown on the UI.
      let listItems =
          alwaysTranslateDiv.querySelectorAll<HTMLElement>('.list-item');
      assertEquals(0, listItems.length);

      // But one should be on the always translate list
      assertDeepEquals(
          ['eo'],
          Object.keys(PrefService.getInstance()
                          .getPref<Record<string, unknown>>(alwaysTranslatePref)
                          .value));

      // Add a language that is in fake_language_settings_private. The
      // language should be shown in the always translate list.
      languageHelper.setLanguageAlwaysTranslateState('nb', true);
      flush();

      // // There should now be only one item shown.
      listItems =
          alwaysTranslateDiv.querySelectorAll<HTMLElement>('.list-item');
      assertEquals(1, listItems.length);

      // But two should be on the always translate list
      assertDeepEquals(
          ['eo', 'nb'],
          Object.keys(PrefService.getInstance()
                          .getPref<Record<string, unknown>>(alwaysTranslatePref)
                          .value));
    });

    test('never translate remove icon enabled state', function() {
      // The icon should be disabled if there is only one element on the list
      // and enabled if there are more than one.
      const neverTranslateDiv =
          translatePage.shadowRoot!.querySelector<HTMLElement>(
              '#neverTranslateList');
      assertTrue(!!neverTranslateDiv);

      // Initially only one disabled icon
      let deleteIcons = neverTranslateDiv.querySelectorAll<CrIconButtonElement>(
          '.icon-delete-gray');
      assertEquals(1, deleteIcons.length);
      assertTrue(deleteIcons[0]!.disabled);

      // Add another language to never translate.
      languageHelper.disableTranslateLanguage('sw');
      flush();

      // All icons should be enabled now.
      deleteIcons = neverTranslateDiv.querySelectorAll<CrIconButtonElement>(
          '.icon-delete-gray');
      assertEquals(2, deleteIcons.length);
      for (const icon of deleteIcons) {
        assertFalse(icon.disabled);
      }

      // Remove language and icon should be disabled again.
      languageHelper.enableTranslateLanguage('sw');
      flush();

      // All icons should be enabled now.
      deleteIcons = neverTranslateDiv.querySelectorAll<CrIconButtonElement>(
          '.icon-delete-gray');
      assertEquals(1, deleteIcons.length);
      assertTrue(deleteIcons[0]!.disabled);
    });

    test('translate.enable toggle', function() {
      const settingsToggle =
          translatePage.shadowRoot!.querySelector<HTMLElement>(
              '#offerTranslateOtherLanguages');
      assertTrue(!!settingsToggle);

      // Clicking on the toggle switches it to false.
      settingsToggle.click();
      let newToggleValue =
          PrefService.getInstance().getPref<boolean>('translate.enabled').value;
      assertFalse(newToggleValue);

      // Clicking on the toggle switches it to true again.
      settingsToggle.click();
      newToggleValue =
          PrefService.getInstance().getPref<boolean>('translate.enabled').value;
      assertTrue(newToggleValue);
    });
  });

  suite('AlwaysTranslateDialog', function() {
    let dialog: SettingsAddLanguagesDialogElement;
    let dialogClosedResolver: PromiseResolver<void>;
    let dialogClosedObserver: MutationObserver;

    // Resolves the PromiseResolver if the mutation includes removal of the
    // settings-add-languages-dialog.
    // TODO(michaelpg): Extract into a common method similar to
    // whenAttributeIs for use elsewhere.
    function onMutation(
        mutations: MutationRecord[], observer: MutationObserver) {
      if (mutations.some(function(mutation) {
            return mutation.type === 'childList' &&
                Array.from(mutation.removedNodes).includes(dialog);
          })) {
        // Sanity check: the dialog should no longer be in the DOM.
        assertEquals(
            null,
            translatePage.shadowRoot!.querySelector(
                'settings-add-languages-dialog'));
        observer.disconnect();
        assertTrue(!!dialogClosedResolver);
        dialogClosedResolver.resolve();
      }
    }

    setup(async function() {
      const addLanguagesButton =
          translatePage.shadowRoot!.querySelector<HTMLElement>(
              '#addAlwaysTranslate');
      const whenDialogOpen = eventToPromise('cr-dialog-open', translatePage);
      assertTrue(!!addLanguagesButton);
      addLanguagesButton.click();

      // The page stamps the dialog, registers listeners, and populates the
      // iron-list asynchronously at microtask timing, so wait for a new
      // task.
      await whenDialogOpen;

      dialog = translatePage.shadowRoot!.querySelector(
          'settings-add-languages-dialog')!;
      assertTrue(!!dialog);
      assertEquals(dialog.id, 'alwaysTranslateDialog');

      // Observe the removal of the dialog via MutationObserver since the
      // HTMLDialogElement 'close' event fires at an unpredictable time.
      dialogClosedResolver = new PromiseResolver();
      dialogClosedObserver = new MutationObserver(onMutation);
      dialogClosedObserver.observe(
          translatePage.shadowRoot!.querySelector('settings-section')!,
          {childList: true});

      flush();
    });

    teardown(function() {
      dialogClosedObserver.disconnect();
    });

    test('add languages and confirm', function() {
      dialog.dispatchEvent(
          new CustomEvent('languages-added', {detail: ['en', 'no']}));
      dialog.$.dialog.close();
      assertDeepEquals(
          ['en', 'no'],
          Object.keys(PrefService.getInstance()
                          .getPref<Record<string, unknown>>(alwaysTranslatePref)
                          .value));

      return dialogClosedResolver.promise;
    });
  });

  suite('NeverTranslateDialog', function() {
    let dialog: SettingsAddLanguagesDialogElement;
    let dialogClosedResolver: PromiseResolver<void>;
    let dialogClosedObserver: MutationObserver;

    // Resolves the PromiseResolver if the mutation includes removal of the
    // settings-add-languages-dialog.
    // TODO(michaelpg): Extract into a common method similar to
    // whenAttributeIs for use elsewhere.
    function onMutation(
        mutations: MutationRecord[], observer: MutationObserver) {
      if (mutations.some(function(mutation) {
            return mutation.type === 'childList' &&
                Array.from(mutation.removedNodes).includes(dialog);
          })) {
        // Sanity check: the dialog should no longer be in the DOM.
        assertEquals(
            null,
            translatePage.shadowRoot!.querySelector(
                'settings-add-languages-dialog'));
        observer.disconnect();
        assertTrue(!!dialogClosedResolver);
        dialogClosedResolver.resolve();
      }
    }

    setup(async function() {
      const addLanguagesButton =
          translatePage.shadowRoot!.querySelector<HTMLElement>(
              '#addNeverTranslate');
      const whenDialogOpen = eventToPromise('cr-dialog-open', translatePage);
      assertTrue(!!addLanguagesButton);
      addLanguagesButton.click();

      // The page stamps the dialog, registers listeners, and populates the
      // iron-list asynchronously at microtask timing, so wait for a new
      // task.
      await whenDialogOpen;

      dialog = translatePage.shadowRoot!.querySelector(
          'settings-add-languages-dialog')!;
      assertTrue(!!dialog);
      assertEquals(dialog.id, 'neverTranslateDialog');

      // Observe the removal of the dialog via MutationObserver since the
      // HTMLDialogElement 'close' event fires at an unpredictable time.
      dialogClosedResolver = new PromiseResolver();
      dialogClosedObserver = new MutationObserver(onMutation);
      dialogClosedObserver.observe(
          translatePage.shadowRoot!.querySelector('settings-section')!,
          {childList: true});

      flush();
    });

    teardown(function() {
      dialogClosedObserver.disconnect();
    });

    test('add languages and confirm', function() {
      dialog.dispatchEvent(
          new CustomEvent('languages-added', {detail: ['sw', 'no']}));
      dialog.$.dialog.close();
      assertDeepEquals(
          ['en-US', 'sw', 'no'],
          PrefService.getInstance().getPref(neverTranslatePref).value);

      return dialogClosedResolver.promise;
    });
  });
});
