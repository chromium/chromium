// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LanguagesBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {eventToPromise, fakeDataBind} from 'chrome://webui-test/test_util.js';

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {FakeSettingsPrivate} from './fake_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';

// clang-format on

window.languages_subpage_details_tests = {};

/** @enum {string} */
window.languages_subpage_details_tests.TestNames = {
  AlwaysTranslateDialog: 'always translate dialog',
  NeverTranslateDialog: 'never translate dialog',
};

suite('languages subpage detailed settings', function() {
  /** @type {?LanguageHelper} */
  let languageHelper = null;
  /** @type {?SettingsLanguagesPageElement} */
  let languagesSubpage = null;
  /** @type {?CrActionMenuElement} */
  let actionMenu = null;
  /** @type {?LanguagesBrowserProxy} */
  let browserProxy = null;

  // Always Translate language pref name for the platform.
  const alwaysTranslatePref = 'translate_allowlists';
  const neverTranslatePref = 'translate_blocked_languages';

  suiteSetup(function() {
    // TODO(crbug/1109431): Update this test once migration is completed.
    loadTimeData.overrideValues({
      isChromeOSLanguagesSettingsUpdate: false,
      enableDesktopDetailedLanguageSettings: true,
    });
    testing.Test.disableAnimationsAndTransitions();
    PolymerTest.clearBody();
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(function() {
    const settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
    settingsPrefs.initialize(settingsPrivate);
    document.body.appendChild(settingsPrefs);
    return CrSettingsPrefs.initialized.then(function() {
      // Set up test browser proxy.
      browserProxy = new TestLanguagesBrowserProxy();
      LanguagesBrowserProxyImpl.setInstance(browserProxy);

      // Set up fake languageSettingsPrivate API.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
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
      actionMenu = languagesSubpage.shadowRoot.querySelector('#menu').get();

      languageHelper = languagesSubpage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  teardown(function() {
    PolymerTest.clearBody();
  });

  suite(
      languages_subpage_details_tests.TestNames.AlwaysTranslateDialog,
      function() {
        let dialog;
        let dialogItems;
        let cancelButton;
        let actionButton;
        let dialogClosedResolver;
        let dialogClosedObserver;

        // Resolves the PromiseResolver if the mutation includes removal of the
        // settings-add-languages-dialog.
        // TODO(michaelpg): Extract into a common method similar to
        // whenAttributeIs for use elsewhere.
        const onMutation = function(mutations, observer) {
          if (mutations.some(function(mutation) {
                return mutation.type === 'childList' &&
                    Array.from(mutation.removedNodes).includes(dialog);
              })) {
            // Sanity check: the dialog should no longer be in the DOM.
            assertEquals(
                null,
                languagesSubpage.shadowRoot.querySelector(
                    'settings-add-languages-dialog'));
            observer.disconnect();
            assertTrue(!!dialogClosedResolver);
            dialogClosedResolver.resolve();
          }
        };

        setup(function() {
          const addLanguagesButton =
              languagesSubpage.shadowRoot.querySelector('#addAlwaysTranslate');
          const whenDialogOpen =
              eventToPromise('cr-dialog-open', languagesSubpage);
          addLanguagesButton.click();

          // The page stamps the dialog, registers listeners, and populates the
          // iron-list asynchronously at microtask timing, so wait for a new
          // task.
          return whenDialogOpen.then(() => {
            dialog = languagesSubpage.shadowRoot.querySelector(
                'settings-add-languages-dialog');
            assertTrue(!!dialog);
            assertEquals(dialog.id, 'alwaysTranslateDialog');

            // Observe the removal of the dialog via MutationObserver since the
            // HTMLDialogElement 'close' event fires at an unpredictable time.
            dialogClosedResolver = new PromiseResolver();
            dialogClosedObserver = new MutationObserver(onMutation);
            dialogClosedObserver.observe(
                languagesSubpage.root, {childList: true});

            actionButton = dialog.shadowRoot.querySelector('.action-button');
            cancelButton = dialog.shadowRoot.querySelector('.cancel-button');
            flush();

            // The fixed-height dialog's iron-list should stamp far fewer than
            // 50 items.
            dialogItems =
                dialog.$.dialog.querySelectorAll('.list-item:not([hidden])');
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
              ['en', 'no'], languageHelper.getPref(alwaysTranslatePref).value);

          return dialogClosedResolver.promise;
        });
      });

  suite(
      languages_subpage_details_tests.TestNames.NeverTranslateDialog,
      function() {
        let dialog;
        let dialogItems;
        let cancelButton;
        let actionButton;
        let dialogClosedResolver;
        let dialogClosedObserver;

        // Resolves the PromiseResolver if the mutation includes removal of the
        // settings-add-languages-dialog.
        // TODO(michaelpg): Extract into a common method similar to
        // whenAttributeIs for use elsewhere.
        const onMutation = function(mutations, observer) {
          if (mutations.some(function(mutation) {
                return mutation.type === 'childList' &&
                    Array.from(mutation.removedNodes).includes(dialog);
              })) {
            // Sanity check: the dialog should no longer be in the DOM.
            assertEquals(
                null,
                languagesSubpage.shadowRoot.querySelector(
                    'settings-add-languages-dialog'));
            observer.disconnect();
            assertTrue(!!dialogClosedResolver);
            dialogClosedResolver.resolve();
          }
        };

        setup(function() {
          const addLanguagesButton =
              languagesSubpage.shadowRoot.querySelector('#addNeverTranslate');
          const whenDialogOpen =
              eventToPromise('cr-dialog-open', languagesSubpage);
          addLanguagesButton.click();

          // The page stamps the dialog, registers listeners, and populates the
          // iron-list asynchronously at microtask timing, so wait for a new
          // task.
          return whenDialogOpen.then(() => {
            dialog = languagesSubpage.shadowRoot.querySelector(
                'settings-add-languages-dialog');
            assertTrue(!!dialog);
            assertEquals(dialog.id, 'neverTranslateDialog');

            // Observe the removal of the dialog via MutationObserver since the
            // HTMLDialogElement 'close' event fires at an unpredictable time.
            dialogClosedResolver = new PromiseResolver();
            dialogClosedObserver = new MutationObserver(onMutation);
            dialogClosedObserver.observe(
                languagesSubpage.root, {childList: true});

            actionButton = dialog.shadowRoot.querySelector('.action-button');
            cancelButton = dialog.shadowRoot.querySelector('.cancel-button');
            flush();

            // The fixed-height dialog's iron-list should stamp far fewer than
            // 50 items.
            dialogItems =
                dialog.$.dialog.querySelectorAll('.list-item:not([hidden])');
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
              languageHelper.getPref(neverTranslatePref).value);

          return dialogClosedResolver.promise;
        });
      });
});
