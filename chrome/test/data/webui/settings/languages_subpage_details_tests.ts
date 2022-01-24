// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LanguageHelper, LanguagesBrowserProxyImpl, SettingsAddLanguagesDialogElement, SettingsLanguagesSubpageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, fakeDataBind} from 'chrome://webui-test/test_util.js';

import {FakeLanguageSettingsPrivate, getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {FakeSettingsPrivate} from './fake_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';

// clang-format on

const languages_subpage_details_tests = {
  TestNames: {
    AlwaysTranslateDialog: 'always translate dialog',
    NeverTranslateDialog: 'never translate dialog',
  },
};

Object.assign(window, {languages_subpage_details_tests});

suite('languages subpage detailed settings', function() {
  let languageHelper: LanguageHelper;
  let languagesSubpage: SettingsLanguagesSubpageElement;
  let browserProxy: TestLanguagesBrowserProxy;

  // Always Translate language pref name for the platform.
  const alwaysTranslatePref = 'translate_allowlists';
  const neverTranslatePref = 'translate_blocked_languages';

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableDesktopDetailedLanguageSettings: true,
    });
    document.body.innerHTML = '';
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(function() {
    const settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
    settingsPrefs.initialize(
        settingsPrivate as unknown as typeof chrome.settingsPrivate);
    document.body.appendChild(settingsPrefs);
    return CrSettingsPrefs.initialized.then(function() {
      // Set up test browser proxy.
      browserProxy = new TestLanguagesBrowserProxy();
      LanguagesBrowserProxyImpl.setInstance(browserProxy);

      // Set up fake languageSettingsPrivate API.
      const languageSettingsPrivate =
          browserProxy.getLanguageSettingsPrivate() as unknown as
          FakeLanguageSettingsPrivate;
      languageSettingsPrivate.setSettingsPrefs(settingsPrefs);

      const settingsLanguages = document.createElement('settings-languages');
      settingsLanguages.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
      document.body.appendChild(settingsLanguages);

      languagesSubpage = document.createElement('settings-languages-subpage');

      languagesSubpage.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, languagesSubpage, 'prefs');

      languagesSubpage.languageHelper = settingsLanguages.languageHelper;
      fakeDataBind(settingsLanguages, languagesSubpage, 'language-helper');

      languagesSubpage.languages = settingsLanguages.languages;
      fakeDataBind(settingsLanguages, languagesSubpage, 'languages');

      document.body.appendChild(languagesSubpage);
      flush();

      languageHelper = languagesSubpage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  teardown(function() {
    document.body.innerHTML = '';
  });

  suite(
      languages_subpage_details_tests.TestNames.AlwaysTranslateDialog,
      function() {
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
                languagesSubpage.shadowRoot!.querySelector(
                    'settings-add-languages-dialog'));
            observer.disconnect();
            assertTrue(!!dialogClosedResolver);
            dialogClosedResolver.resolve();
          }
        }

        setup(function() {
          const addLanguagesButton =
              languagesSubpage.shadowRoot!.querySelector<HTMLElement>(
                  '#addAlwaysTranslate');
          const whenDialogOpen =
              eventToPromise('cr-dialog-open', languagesSubpage);
          assertTrue(!!addLanguagesButton);
          addLanguagesButton.click();

          // The page stamps the dialog, registers listeners, and populates the
          // iron-list asynchronously at microtask timing, so wait for a new
          // task.
          return whenDialogOpen.then(() => {
            dialog = languagesSubpage.shadowRoot!.querySelector(
                'settings-add-languages-dialog')!;
            assertTrue(!!dialog);
            assertEquals(dialog.id, 'alwaysTranslateDialog');

            // Observe the removal of the dialog via MutationObserver since the
            // HTMLDialogElement 'close' event fires at an unpredictable time.
            dialogClosedResolver = new PromiseResolver();
            dialogClosedObserver = new MutationObserver(onMutation);
            dialogClosedObserver.observe(
                languagesSubpage.shadowRoot!, {childList: true});

            flush();
          });
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
              languagesSubpage.getPref(alwaysTranslatePref).value);

          return dialogClosedResolver.promise;
        });
      });

  suite(
      languages_subpage_details_tests.TestNames.NeverTranslateDialog,
      function() {
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
                languagesSubpage.shadowRoot!.querySelector(
                    'settings-add-languages-dialog'));
            observer.disconnect();
            assertTrue(!!dialogClosedResolver);
            dialogClosedResolver.resolve();
          }
        }

        setup(function() {
          const addLanguagesButton =
              languagesSubpage.shadowRoot!.querySelector<HTMLElement>(
                  '#addNeverTranslate');
          const whenDialogOpen =
              eventToPromise('cr-dialog-open', languagesSubpage);
          assertTrue(!!addLanguagesButton);
          addLanguagesButton.click();

          // The page stamps the dialog, registers listeners, and populates the
          // iron-list asynchronously at microtask timing, so wait for a new
          // task.
          return whenDialogOpen.then(() => {
            dialog = languagesSubpage.shadowRoot!.querySelector(
                'settings-add-languages-dialog')!;
            assertTrue(!!dialog);
            assertEquals(dialog.id, 'neverTranslateDialog');

            // Observe the removal of the dialog via MutationObserver since the
            // HTMLDialogElement 'close' event fires at an unpredictable time.
            dialogClosedResolver = new PromiseResolver();
            dialogClosedObserver = new MutationObserver(onMutation);
            dialogClosedObserver.observe(
                languagesSubpage.shadowRoot!, {childList: true});

            flush();
          });
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
              languagesSubpage.getPref(neverTranslatePref).value);

          return dialogClosedResolver.promise;
        });
      });
});
