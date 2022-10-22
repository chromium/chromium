// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LanguageHelper, LanguagesBrowserProxyImpl, SettingsSpellCheckPageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
// <if expr="not is_macosx">
import {loadTimeData, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertDeepEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
// </if>

import {assertFalse} from 'chrome://webui-test/chai_assert.js';
// <if expr="_google_chrome">
import {assertNotEquals} from 'chrome://webui-test/chai_assert.js';
// </if>

// <if expr="not is_macosx">
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
// </if>

import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import {FakeLanguageSettingsPrivate, getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {FakeSettingsPrivate} from './fake_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';

// clang-format on

const spell_check_page_tests = {
  TestNames: {
    Spellcheck: 'spellcheck_all',
    // <if expr="_google_chrome">
    SpellcheckOfficialBuild: 'spellcheck_official',
    // </if>
  },
};

Object.assign(window, {spell_check_page_tests});

suite('spell check page', function() {
  let languageHelper: LanguageHelper;
  let spellcheckPage: SettingsSpellCheckPageElement;
  let browserProxy: TestLanguagesBrowserProxy;

  suiteSetup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

      spellcheckPage = document.createElement('settings-spell-check-page');

      // Prefs would normally be data-bound to settings-spell-check-page.
      spellcheckPage.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, spellcheckPage, 'prefs');

      spellcheckPage.languageHelper = settingsLanguages.languageHelper;
      fakeDataBind(settingsLanguages, spellcheckPage, 'language-helper');

      spellcheckPage.languages = settingsLanguages.languages;
      fakeDataBind(settingsLanguages, spellcheckPage, 'languages');

      document.body.appendChild(spellcheckPage);
      flush();
      languageHelper = spellcheckPage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  suite(spell_check_page_tests.TestNames.Spellcheck, function() {
    // <if expr="is_macosx">
    test('structure', function() {
      const spellCheckCollapse =
          spellcheckPage.shadowRoot!.querySelector('#spellCheckCollapse');
      assertFalse(!!spellCheckCollapse);
    });
    // </if>

    // <if expr="not is_macosx">
    test('structure', function() {
      const spellCheckCollapse =
          spellcheckPage.shadowRoot!.querySelector('#spellCheckCollapse');
      assertTrue(!!spellCheckCollapse);

      const triggerRow = spellcheckPage.shadowRoot!.querySelector(
          '#enableSpellcheckingToggle')!;

      // Disable spellcheck for en-US.
      const spellcheckLanguageRow =
          spellCheckCollapse.querySelector('.list-item')!;
      const spellcheckLanguageToggle =
          spellcheckLanguageRow.querySelector('cr-toggle');
      assertTrue(!!spellcheckLanguageToggle);
      spellcheckLanguageToggle.click();
      assertFalse(spellcheckLanguageToggle.checked);
      assertEquals(
          0, spellcheckPage.getPref('spellcheck.dictionaries').value.length);

      // Force-enable a language via policy.
      spellcheckPage.setPrefValue('spellcheck.forced_dictionaries', ['nb']);
      flush();
      const forceEnabledNbLanguageRow =
          spellCheckCollapse.querySelectorAll('.list-item')[2];
      assertTrue(!!forceEnabledNbLanguageRow);
      assertTrue(forceEnabledNbLanguageRow.querySelector('cr-toggle')!.checked);
      assertTrue(!!forceEnabledNbLanguageRow.querySelector(
          'cr-policy-pref-indicator'));

      // Add the same language to spellcheck.dictionaries, but don't enable it.
      spellcheckPage.setPrefValue('spellcheck.forced_dictionaries', []);
      spellcheckPage.setPrefValue('spellcheck.dictionaries', ['nb']);
      flush();

      const prefEnabledNbLanguageRow =
          spellCheckCollapse.querySelectorAll('.list-item')[2];
      assertTrue(!!prefEnabledNbLanguageRow);
      assertTrue(prefEnabledNbLanguageRow.querySelector('cr-toggle')!.checked);

      // Disable the language.
      prefEnabledNbLanguageRow.querySelector('cr-toggle')!.click();
      flush();
      assertEquals(2, spellCheckCollapse.querySelectorAll('.list-item').length);

      // Force-disable the same language via policy.
      spellcheckPage.setPrefValue('spellcheck.blocked_dictionaries', ['nb']);
      languageHelper.enableLanguage('nb');
      flush();
      const forceDisabledNbLanguageRow =
          spellCheckCollapse.querySelectorAll('.list-item')[2]!;
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

        // First set the prefValue, then override the actual preference
        // object in spellcheckPage. This is necessary, to avoid a mismatch
        // between the settings state and |spellcheckPage.prefs|, which would
        // cause the value to be reset in |spellcheckPage.prefs|.
        spellcheckPage.setPrefValue('browser.enable_spellchecking', value);
        spellcheckPage.set('prefs.browser.enable_spellchecking', newPrefValue);
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

      const spellCheckLanguagesCount =
          spellCheckCollapse.querySelectorAll('.list-item').length;
      // Enabling a language without spellcheck support should not add it to
      // the list
      languageHelper.enableLanguage('tk');
      flush();
      assertEquals(
          spellCheckCollapse.querySelectorAll('.list-item').length,
          spellCheckLanguagesCount);
    });

    test('only 1 supported language', () => {
      const list = spellcheckPage.shadowRoot!.querySelector<HTMLElement>(
          '#spellCheckLanguagesList')!;
      assertFalse(list.hidden);
      const toggle =
          spellcheckPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#enableSpellcheckingToggle');
      assertTrue(!!toggle);
      assertTrue(toggle.checked);
      assertDeepEquals(
          ['en-US'], spellcheckPage.getPref('spellcheck.dictionaries').value);

      // Update supported languages to just 1 language should hide list.
      spellcheckPage.setPrefValue('intl.accept_languages', 'en-US');
      flush();
      assertTrue(list.hidden);

      // Disable spell check should keep list hidden and remove the single
      // language from dictionaries.
      toggle.click();
      flush();

      assertTrue(list.hidden);
      assertFalse(toggle.checked);
      assertDeepEquals(
          [], spellcheckPage.getPref('spellcheck.dictionaries').value);

      // Enable spell check should keep list hidden and add the single language
      // to dictionaries.
      toggle.click();
      flush();

      assertTrue(list.hidden);
      assertTrue(toggle.checked);
      assertDeepEquals(
          ['en-US'], spellcheckPage.getPref('spellcheck.dictionaries').value);
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
      assertTrue(spellcheckPage.getPref('browser.enable_spellchecking').value);
      assertEquals(toggle.subLabel, undefined);

      // Empty out supported languages
      for (const lang of languageHelper.languages!.enabled) {
        languageHelper.disableLanguage(lang.language.code);
      }
      assertTrue(toggle.disabled);
      assertFalse(spellcheckPage.getPref('browser.enable_spellchecking').value);
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

      const languageCode =
          spellcheckPage.get('languages.enabled.0.language.code');
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
          spellcheckPage.get('languages.enabled.0.downloadDictionaryStatus');
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
  suite(spell_check_page_tests.TestNames.SpellcheckOfficialBuild, function() {
    test('enabling and disabling the spelling service', () => {
      const previousValue =
          spellcheckPage.prefs.spellcheck.use_spelling_service.value;
      spellcheckPage.shadowRoot!
          .querySelector<HTMLElement>('#spellingServiceEnable')!.click();
      flush();
      assertNotEquals(
          previousValue,
          spellcheckPage.prefs.spellcheck.use_spelling_service.value);
    });
  });
  // </if>
});
