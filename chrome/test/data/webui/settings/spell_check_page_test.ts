// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {LanguageHelper, SettingsSpellCheckPageElement} from 'chrome://settings/lazy_load.js';
import {LanguageHelperImpl, LanguagesBrowserProxyImpl, getLanguageHelperInstance} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import type {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
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
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

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

  suiteSetup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async function() {
    prefsBrowserProxy = new TestPrefsBrowserProxy(getFakeLanguagePrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    // Set up test browser proxy.
    browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstance(browserProxy);

    LanguageHelperImpl.resetInstanceForTesting();
    languageHelper = getLanguageHelperInstance();
    await languageHelper.whenReady();

    spellcheckPage = document.createElement('settings-spell-check-page');
    document.body.appendChild(spellcheckPage);
    flush();
  });

  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  suite('AllBuilds', function() {
    // <if expr="is_macosx">
    test('structure', function() {
      const spellCheckLanguagesList =
          spellcheckPage.shadowRoot!.querySelector('#spellCheckLanguagesList');
      assertFalse(!!spellCheckLanguagesList);

      const editDictionaryTrigger =
          spellcheckPage.shadowRoot!.querySelector('#spellCheckSubpageTrigger');
      assertFalse(!!editDictionaryTrigger);

      // <if expr="not _google_chrome">
      const spellCheckCollapse =
          spellcheckPage.shadowRoot!.querySelector('#spellCheckCollapse');
      assertFalse(!!spellCheckCollapse);
      // </if>

      const toggle =
          spellcheckPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#enableSpellcheckingToggle');
      assertTrue(!!toggle);
      assertTrue(toggle.checked);
    });
    // </if>

    // <if expr="not is_macosx">
    test('structure', async function() {
      function getListItems() {
        return spellcheckPage.shadowRoot!.querySelectorAll(
            '#spellCheckCollapse .list-item');
      }

      let listItems = getListItems();
      const triggerRow = spellcheckPage.shadowRoot!.querySelector(
          '#enableSpellcheckingToggle')!;
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
          PrefService.getInstance()
              .getPref<string[]>('spellcheck.dictionaries')
              .value.length);

      // Force-enable a language via policy.
      PrefService.getInstance().setPrefValue(
          'spellcheck.forced_dictionaries', ['nb']);
      flush();
      listItems = getListItems();
      assertEquals(3, listItems.length);
      const forceEnabledNbLanguageRow = listItems[2]!;
      assertTrue(forceEnabledNbLanguageRow.querySelector('cr-toggle')!.checked);
      assertTrue(!!forceEnabledNbLanguageRow.querySelector(
          'cr-policy-pref-indicator'));

      // Add the same language to spellcheck.dictionaries, but don't enable it.
      PrefService.getInstance().setPrefValue(
          'spellcheck.forced_dictionaries', []);
      PrefService.getInstance().setPrefValue('spellcheck.dictionaries', ['nb']);
      flush();
      listItems = getListItems();
      assertEquals(3, listItems.length);
      const prefEnabledNbLanguageRow = listItems[2]!;
      assertTrue(prefEnabledNbLanguageRow.querySelector('cr-toggle')!.checked);

      // Disable the language.
      prefEnabledNbLanguageRow.querySelector('cr-toggle')!.click();
      await prefEnabledNbLanguageRow.querySelector('cr-toggle')!.updateComplete;
      flush();
      assertEquals(2, getListItems().length);

      // Force-disable the same language via policy.
      PrefService.getInstance().setPrefValue(
          'spellcheck.blocked_dictionaries', ['nb']);
      languageHelper.enableLanguage('nb');
      flush();
      listItems = getListItems();
      assertEquals(3, listItems.length);
      const forceDisabledNbLanguageRow = listItems[2]!;
      assertFalse(
          forceDisabledNbLanguageRow.querySelector('cr-toggle')!.checked);
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
      flush();

      // The policy indicator should be present.
      assertTrue(
          !!triggerRow.shadowRoot!.querySelector('cr-policy-pref-indicator'));

      // Force-enable spellchecking via policy, and ensure that the policy
      // indicator is not present. |enable_spellchecking| can be forced to
      // true by policy, but no indicator should be shown in that case.
      setEnableSpellcheckingViaPolicy(true);
      flush();
      assertFalse(!!triggerRow.querySelector('cr-policy-pref-indicator'));

      const spellCheckLanguagesCount = getListItems().length;
      // Enabling a language without spellcheck support should not add it to
      // the list
      languageHelper.enableLanguage('tk');
      flush();
      assertEquals(getListItems().length, spellCheckLanguagesCount);
    });

    test('only 1 supported language', async () => {
      const list = spellcheckPage.shadowRoot!.querySelector<HTMLElement>(
          '#spellCheckLanguagesList')!;
      assertFalse(list.hidden);
      const toggle =
          spellcheckPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#enableSpellcheckingToggle');
      assertTrue(!!toggle);
      assertTrue(toggle.checked);
      assertDeepEquals(
          ['en-US'],
          PrefService.getInstance()
              .getPref<string[]>('spellcheck.dictionaries')
              .value);

      // Update supported languages to just 1 language should hide list.
      languageHelper.disableLanguage('sw');
      await flushTasks();
      assertTrue(list.hidden);

      // Disable spell check should keep list hidden and remove the single
      // language from dictionaries.
      toggle.click();
      await flushTasks();

      assertTrue(list.hidden);
      assertFalse(toggle.checked);
      assertDeepEquals(
          [],
          PrefService.getInstance()
              .getPref<string[]>('spellcheck.dictionaries')
              .value);

      // Enable spell check should keep list hidden and add the single language
      // to dictionaries.
      toggle.click();
      await flushTasks();

      assertTrue(list.hidden);
      assertTrue(toggle.checked);
      assertDeepEquals(
          ['en-US'],
          PrefService.getInstance()
              .getPref<string[]>('spellcheck.dictionaries')
              .value);

      // When a single language has a dictionary download error, the list should
      // not be hidden so the user can see the error and retry.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      (languageSettingsPrivate.onSpellcheckDictionariesChanged as
       FakeChromeEvent)
          .callListeners([
            {languageCode: 'en-US', isReady: false, downloadFailed: true},
          ]);
      await flushTasks();
      assertFalse(list.hidden);

      // When the dictionary download succeeds, the list is hidden again.
      (languageSettingsPrivate.onSpellcheckDictionariesChanged as
       FakeChromeEvent)
          .callListeners([
            {languageCode: 'en-US', isReady: true, downloadFailed: false},
          ]);
      await flushTasks();
      assertTrue(list.hidden);

      // When a single language is managed by policy, the list is not hidden.
      PrefService.getInstance().setPrefValue(
          'spellcheck.forced_dictionaries', ['en-US']);
      await flushTasks();
      assertFalse(list.hidden);
    });

    test('no supported languages', () => {
      loadTimeData.overrideValues({
        spellCheckDisabledReason: 'no languages!',
      });

      const toggle =
          spellcheckPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#enableSpellcheckingToggle');
      assertTrue(!!toggle);

      assertFalse(toggle.disabled);
      assertTrue(PrefService.getInstance()
                     .getPref<boolean>('browser.enable_spellchecking')
                     .value);
      assertEquals(toggle.subLabel, undefined);

      // Empty out supported languages
      languageHelper.dispatchEvent(new CustomEvent('languages-changed', {
        detail: Object.assign({}, languageHelper.languages, {
          enabled: [],
          spellCheckOnLanguages: [],
        }),
      }));
      assertTrue(toggle.disabled);
      assertFalse(PrefService.getInstance()
                      .getPref<boolean>('browser.enable_spellchecking')
                      .value);
      assertEquals(toggle.subLabel, 'no languages!');
    });

    test('error handling', function() {
      function checkAllHidden(nodes: HTMLElement[]) {
        assertTrue(nodes.every(node => node.hidden));
      }

      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      const spellCheckCollapse =
          spellcheckPage.shadowRoot!.querySelector('#spellCheckCollapse')!;
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

      flush();
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
      flush();
      assertTrue(moreInfo.hidden);

      retryButtons[0]!.click();
      flush();
      assertFalse(moreInfo.hidden);
    });
    // </if>
  });

  // <if expr="_google_chrome">
  suite('OfficialBuild', function() {
    test('enabling and disabling the spelling service', async () => {
      const previousValue =
          PrefService.getInstance()
              .getPref<boolean>('spellcheck.use_spelling_service')
              .value;
      spellcheckPage.shadowRoot!
          .querySelector<HTMLElement>('#spellingServiceEnable')!.click();
      flush();
      await microtasksFinished();
      assertNotEquals(
          previousValue,
          PrefService.getInstance()
              .getPref<boolean>('spellcheck.use_spelling_service')
              .value);
    });
  });
  // </if>
});
