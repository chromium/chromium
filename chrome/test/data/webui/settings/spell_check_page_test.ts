// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {LanguageHelper, SettingsSpellCheckPageElement} from 'chrome://settings/lazy_load.js';
import {LanguageHelperImpl, LanguagesBrowserProxyImpl, getLanguageHelperInstance} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
// <if expr="not is_macosx">
import {loadTimeData} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
// </if>

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
// <if expr="_google_chrome">
import {assertNotEquals} from 'chrome://webui-test/chai_assert.js';
// </if>

// <if expr="_google_chrome or not is_macosx">
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
// </if>
// <if expr="not is_macosx">
import type {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';

// </if>

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

// clang-format on

suite('SpellCheck', function() {
  let languageHelper: LanguageHelper;
  let spellcheckPage: SettingsSpellCheckPageElement;
  let browserProxy: TestLanguagesBrowserProxy;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

  suiteSetup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async function() {
    prefsBrowserProxy = new TestPrefsBrowserProxy(getFakeLanguagePrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    // Set up test browser proxy.
    browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstance(browserProxy);

    LanguageHelperImpl.resetInstanceForTesting();
    languageHelper = getLanguageHelperInstance();
    await languageHelper.whenReady();

    spellcheckPage = document.createElement('settings-spell-check-page');
    document.body.appendChild(spellcheckPage);
  });

  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  suite('AllBuilds', function() {
    // <if expr="is_macosx">
    test('structure', function() {
      const spellCheckLanguagesList =
          spellcheckPage.shadowRoot.querySelector('#spellCheckLanguagesList');
      assertFalse(!!spellCheckLanguagesList);

      const editDictionaryTrigger =
          spellcheckPage.shadowRoot.querySelector('#spellCheckSubpageTrigger');
      assertFalse(!!editDictionaryTrigger);

      // <if expr="not _google_chrome">
      const spellCheckCollapse =
          spellcheckPage.shadowRoot.querySelector('#spellCheckCollapse');
      assertFalse(!!spellCheckCollapse);
      // </if>

      const toggle = spellcheckPage.$.enableSpellcheckingToggle;
      assertTrue(toggle.checked);
    });
    // </if>

    // <if expr="not is_macosx">
    test('structure', async function() {
      function getListItems() {
        return spellcheckPage.shadowRoot.querySelectorAll(
            '#spellCheckCollapse .list-item');
      }

      let listItems = getListItems();
      const triggerRow = spellcheckPage.$.enableSpellcheckingToggle;
      assertEquals(2, listItems.length);

      // Disable spellcheck for en-US.
      const spellcheckLanguageRow = listItems[0]!;
      const spellcheckLanguageToggle =
          spellcheckLanguageRow.querySelector('cr-toggle');
      assertTrue(!!spellcheckLanguageToggle);
      spellcheckLanguageToggle.click();
      await microtasksFinished();
      assertFalse(spellcheckLanguageToggle.checked);
      assertEquals(
          0,
          prefService.getPref<string[]>('spellcheck.dictionaries')
              .value.length);

      // Force-enable a language via policy.
      prefService.setPrefValue('spellcheck.forced_dictionaries', ['nb']);
      await microtasksFinished();
      listItems = getListItems();
      assertEquals(3, listItems.length);
      const forceEnabledNbLanguageRow = listItems[2]!;
      const forceEnabledNbLanguageToggle =
          forceEnabledNbLanguageRow.querySelector('cr-toggle');
      assertTrue(!!forceEnabledNbLanguageToggle);
      assertTrue(forceEnabledNbLanguageToggle.checked);
      assertTrue(!!forceEnabledNbLanguageRow.querySelector(
          'cr-policy-pref-indicator'));

      // Add the same language to spellcheck.dictionaries, but don't enable it.
      prefService.setPrefValue('spellcheck.forced_dictionaries', []);
      prefService.setPrefValue('spellcheck.dictionaries', ['nb']);
      await microtasksFinished();
      listItems = getListItems();
      assertEquals(3, listItems.length);
      const prefEnabledNbLanguageRow = listItems[2]!;
      const prefEnabledNbLanguageToggle =
          prefEnabledNbLanguageRow.querySelector('cr-toggle');
      assertTrue(!!prefEnabledNbLanguageToggle);
      assertTrue(prefEnabledNbLanguageToggle.checked);

      // Disable the language.
      prefEnabledNbLanguageToggle.click();
      await microtasksFinished();
      assertEquals(2, getListItems().length);

      // Force-disable the same language via policy.
      prefService.setPrefValue('spellcheck.blocked_dictionaries', ['nb']);
      languageHelper.enableLanguage('nb');
      await microtasksFinished();
      listItems = getListItems();
      assertEquals(3, listItems.length);
      const forceDisabledNbLanguageRow = listItems[2]!;
      const forceDisabledNbLanguageToggle =
          forceDisabledNbLanguageRow.querySelector('cr-toggle');
      assertTrue(!!forceDisabledNbLanguageToggle);
      assertFalse(forceDisabledNbLanguageToggle.checked);
      assertTrue(!!forceDisabledNbLanguageRow.querySelector(
          'cr-policy-pref-indicator'));

      // Sets |browser.enable_spellchecking| to |value| as if it was set by
      // policy.
      function setEnableSpellcheckingViaPolicy(value: boolean) {
        const newPrefValue = {
          key: 'browser.enable_spellchecking',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: value,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
        };

        prefsBrowserProxy.fakeApi.sendPrefChanges([newPrefValue]);
      }

      // Force-disable spellchecking via policy.
      setEnableSpellcheckingViaPolicy(false);
      await microtasksFinished();

      // The policy indicator should be present.
      assertTrue(
          !!triggerRow.shadowRoot!.querySelector('cr-policy-pref-indicator'));

      // Force-enable spellchecking via policy, and ensure that the policy
      // indicator is not present. |enable_spellchecking| can be forced to
      // true by policy, but no indicator should be shown in that case.
      setEnableSpellcheckingViaPolicy(true);
      await microtasksFinished();
      assertFalse(!!triggerRow.querySelector('cr-policy-pref-indicator'));

      const spellCheckLanguagesCount = getListItems().length;
      // Enabling a language without spellcheck support should not add it to
      // the list
      languageHelper.enableLanguage('tk');
      await microtasksFinished();
      assertEquals(getListItems().length, spellCheckLanguagesCount);
    });

    test('only 1 supported language', async () => {
      const list = spellcheckPage.$.spellCheckLanguagesList;
      assertFalse(list.hidden);
      const toggle = spellcheckPage.$.enableSpellcheckingToggle;
      assertTrue(toggle.checked);
      assertDeepEquals(
          ['en-US'],
          prefService.getPref<string[]>('spellcheck.dictionaries').value);

      // Update supported languages to just 1 language should hide list.
      languageHelper.disableLanguage('sw');
      await microtasksFinished();
      assertTrue(list.hidden);

      // Disable spell check should keep list hidden and remove the single
      // language from dictionaries.
      toggle.click();
      await microtasksFinished();

      assertTrue(list.hidden);
      assertFalse(toggle.checked);
      assertDeepEquals(
          [], prefService.getPref<string[]>('spellcheck.dictionaries').value);

      // Enable spell check should keep list hidden and add the single language
      // to dictionaries.
      toggle.click();
      await microtasksFinished();

      assertTrue(list.hidden);
      assertTrue(toggle.checked);
      assertDeepEquals(
          ['en-US'],
          prefService.getPref<string[]>('spellcheck.dictionaries').value);

      // When a single language has a dictionary download error, the list should
      // not be hidden so the user can see the error and retry.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      (languageSettingsPrivate.onSpellcheckDictionariesChanged as
       FakeChromeEvent)
          .callListeners([
            {languageCode: 'en-US', isReady: false, downloadFailed: true},
          ]);
      await microtasksFinished();
      assertFalse(list.hidden);

      // When the dictionary download succeeds, the list is hidden again.
      (languageSettingsPrivate.onSpellcheckDictionariesChanged as
       FakeChromeEvent)
          .callListeners([
            {languageCode: 'en-US', isReady: true, downloadFailed: false},
          ]);
      await microtasksFinished();
      assertTrue(list.hidden);

      // When a single language is managed by policy, the list is not hidden.
      prefService.setPrefValue('spellcheck.forced_dictionaries', ['en-US']);
      await microtasksFinished();
      assertFalse(list.hidden);
    });

    test('no supported languages', async () => {
      loadTimeData.overrideValues({
        spellCheckDisabledReason: 'no languages!',
      });

      const toggle = spellcheckPage.$.enableSpellcheckingToggle;
      assertFalse(toggle.disabled);
      assertTrue(
          prefService.getPref<boolean>('browser.enable_spellchecking').value);
      assertEquals(toggle.subLabel, '');

      // Empty out supported languages
      languageHelper.dispatchEvent(new CustomEvent('languages-changed', {
        detail: Object.assign({}, languageHelper.languages, {
          enabled: [],
          spellCheckOnLanguages: [],
        }),
      }));
      await microtasksFinished();
      assertTrue(toggle.disabled);
      assertFalse(
          prefService.getPref<boolean>('browser.enable_spellchecking').value);
      assertEquals(toggle.subLabel, 'no languages!');
    });

    test('error handling', async function() {
      function checkAllHidden(nodes: HTMLElement[]) {
        assertTrue(nodes.every(node => node.hidden));
      }

      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      const spellCheckCollapse = spellcheckPage.$.spellCheckCollapse;
      const errorDivs =
          Array.from(spellCheckCollapse.querySelectorAll<HTMLElement>(
              '.name-with-error-list div'));
      assertEquals(4, errorDivs.length);
      checkAllHidden(errorDivs);

      const retryButtons =
          Array.from(spellCheckCollapse.querySelectorAll('cr-button'));
      assertEquals(2, retryButtons.length);
      checkAllHidden(retryButtons);

      const languageCode = languageHelper.languages!.enabled[0]!.language.code;
      (languageSettingsPrivate.onSpellcheckDictionariesChanged as
       FakeChromeEvent)
          .callListeners([
            {languageCode, isReady: false, downloadFailed: true},
          ]);

      await microtasksFinished();
      assertFalse(errorDivs[0]!.hidden);
      checkAllHidden(errorDivs.slice(1));
      assertFalse(retryButtons[0]!.hidden);
      assertTrue(retryButtons[1]!.hidden);

      // Check that more information is provided when subsequent downloads
      // fail.
      const moreInfo = errorDivs[1]!;
      assertTrue(moreInfo.hidden);
      // No change when status is the same as last update.
      const currentStatus =
          languageHelper.languages!.enabled[0]!.downloadDictionaryStatus;
      (languageSettingsPrivate.onSpellcheckDictionariesChanged as
       FakeChromeEvent)
          .callListeners([currentStatus]);
      await microtasksFinished();
      assertTrue(moreInfo.hidden);

      retryButtons[0]!.click();
      await microtasksFinished();
      assertFalse(moreInfo.hidden);
    });
    // </if>
  });

  // <if expr="_google_chrome">
  suite('OfficialBuild', function() {
    test('enabling and disabling the spelling service', async () => {
      const previousValue =
          prefService.getPref<boolean>('spellcheck.use_spelling_service').value;
      spellcheckPage.$.spellingServiceEnable.click();
      await microtasksFinished();
      assertNotEquals(
          previousValue,
          prefService.getPref<boolean>('spellcheck.use_spelling_service')
              .value);
    });
  });
  // </if>
});
