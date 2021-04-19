// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isChromeOS, isMac, isWindows} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LanguagesBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://settings/settings.js';
import {getFakeLanguagePrefs} from 'chrome://test/settings/fake_language_settings_private.js';
import {FakeSettingsPrivate} from 'chrome://test/settings/fake_settings_private.js';
import {TestLanguagesBrowserProxy} from 'chrome://test/settings/test_languages_browser_proxy.js';
import {fakeDataBind, isChildVisible} from 'chrome://test/test_util.m.js';

// clang-format on

window.languages_page_tests = {};

/** @enum {string} */
window.languages_page_tests.TestNames = {
  Spellcheck: 'spellcheck_all',
  SpellcheckOfficialBuild: 'spellcheck_official',
  ChromeOSLanguagesSettingsUpdate: 'chromeos settings update',
  RestructuredLanguageSettings: 'restructured language settings',
};

suite('languages page', function() {
  /** @type {?LanguageHelper} */
  let languageHelper = null;
  /** @type {?SettingsLanguagesPageElement} */
  let languagesPage = null;
  /** @type {?IronCollapseElement} */
  let languagesCollapse = null;
  /** @type {?CrActionMenuElement} */
  let actionMenu = null;
  /** @type {?LanguagesBrowserProxy} */
  let browserProxy = null;

  // Enabled language pref name for the platform.
  const languagesPref = isChromeOS ? 'settings.language.preferred_languages' :
                                     'intl.accept_languages';

  suiteSetup(function() {
    // TODO(crbug/1109431): Update this test once migration is completed.
    loadTimeData.overrideValues({
      isChromeOSLanguagesSettingsUpdate: false,
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
      LanguagesBrowserProxyImpl.instance_ = browserProxy;

      // Set up fake languageSettingsPrivate API.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      languageSettingsPrivate.setSettingsPrefs(settingsPrefs);

      languagesPage = document.createElement('settings-languages-page');

      // Prefs would normally be data-bound to settings-languages-page.
      languagesPage.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, languagesPage, 'prefs');

      document.body.appendChild(languagesPage);
      flush();
      languagesCollapse = languagesPage.$$('#languagesCollapse');
      languagesCollapse.opened = true;
      actionMenu =
          languagesPage.$$('settings-languages-subpage').$$('#menu').get();

      languageHelper = languagesPage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  teardown(function() {
    PolymerTest.clearBody();
  });

  suite(languages_page_tests.TestNames.Spellcheck, function() {
    test('structure', function() {
      const spellCheckCollapse = languagesPage.$$('#spellCheckCollapse');
      const spellCheckSettingsExist = !!spellCheckCollapse;
      if (isMac) {
        assertFalse(spellCheckSettingsExist);
        return;
      }

      assertTrue(spellCheckSettingsExist);

      const triggerRow = languagesPage.$$('#enableSpellcheckingToggle');

      // Disable spellcheck for en-US.
      const spellcheckLanguageRow =
          spellCheckCollapse.querySelector('.list-item');
      const spellcheckLanguageToggle =
          spellcheckLanguageRow.querySelector('cr-toggle');
      assertTrue(!!spellcheckLanguageToggle);
      spellcheckLanguageToggle.click();
      assertFalse(spellcheckLanguageToggle.checked);
      assertEquals(
          0, languageHelper.prefs.spellcheck.dictionaries.value.length);

      // Force-enable a language via policy.
      languageHelper.setPrefValue('spellcheck.forced_dictionaries', ['nb']);
      flush();
      const forceEnabledNbLanguageRow =
          spellCheckCollapse.querySelectorAll('.list-item')[2];
      assertTrue(!!forceEnabledNbLanguageRow);
      assertTrue(forceEnabledNbLanguageRow.querySelector('cr-toggle').checked);
      assertTrue(!!forceEnabledNbLanguageRow.querySelector(
          'cr-policy-pref-indicator'));

      // Add the same language to spellcheck.dictionaries, but don't enable it.
      languageHelper.setPrefValue('spellcheck.forced_dictionaries', []);
      languageHelper.setPrefValue('spellcheck.dictionaries', ['nb']);
      flush();
      debugger;
      const prefEnabledNbLanguageRow =
          spellCheckCollapse.querySelectorAll('.list-item')[2];
      assertTrue(!!prefEnabledNbLanguageRow);
      assertTrue(prefEnabledNbLanguageRow.querySelector('cr-toggle').checked);

      // Disable the language.
      prefEnabledNbLanguageRow.querySelector('cr-toggle').click();
      flush();
      assertEquals(2, spellCheckCollapse.querySelectorAll('.list-item').length);

      // Force-disable the same language via policy.
      languageHelper.setPrefValue('spellcheck.blocked_dictionaries', ['nb']);
      languageHelper.enableLanguage('nb');
      flush();
      const forceDisabledNbLanguageRow =
          spellCheckCollapse.querySelectorAll('.list-item')[2];
      assertFalse(
          forceDisabledNbLanguageRow.querySelector('cr-toggle').checked);
      assertTrue(!!forceDisabledNbLanguageRow.querySelector(
          'cr-policy-pref-indicator'));

      // Sets |browser.enable_spellchecking| to |value| as if it was set by
      // policy.
      const setEnableSpellcheckingViaPolicy = function(value) {
        const newPrefValue = {
          key: 'browser.enable_spellchecking',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: value,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY
        };

        // First set the prefValue, then override the actual preference
        // object in languagesPage. This is necessary, to avoid a mismatch
        // between the settings state and |languagesPage.prefs|, which would
        // cause the value to be reset in |languagesPage.prefs|.
        languageHelper.setPrefValue('browser.enable_spellchecking', value);
        languagesPage.set('prefs.browser.enable_spellchecking', newPrefValue);
      };

      // Force-disable spellchecking via policy.
      setEnableSpellcheckingViaPolicy(false);
      flush();

      // The policy indicator should be present.
      assertTrue(!!triggerRow.$$('cr-policy-pref-indicator'));

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
      if (isMac) {
        return;
      }

      const list = languagesPage.$$('#spellCheckLanguagesList');
      assertFalse(list.hidden);
      assertTrue(languagesPage.$$('#enableSpellcheckingToggle').checked);
      assertDeepEquals(
          ['en-US'], languageHelper.getPref('spellcheck.dictionaries').value);

      // Update supported languages to just 1 language should hide list.
      languageHelper.setPrefValue(languagesPref, 'en-US');
      flush();
      assertTrue(list.hidden);

      // Disable spell check should keep list hidden and remove the single
      // language from dictionaries.
      languagesPage.$$('#enableSpellcheckingToggle').click();
      flush();

      assertTrue(list.hidden);
      assertFalse(languagesPage.$$('#enableSpellcheckingToggle').checked);
      assertDeepEquals(
          [], languageHelper.getPref('spellcheck.dictionaries').value);

      // Enable spell check should keep list hidden and add the single language
      // to dictionaries.
      languagesPage.$$('#enableSpellcheckingToggle').click();
      flush();

      assertTrue(list.hidden);
      assertTrue(languagesPage.$$('#enableSpellcheckingToggle').checked);
      assertDeepEquals(
          ['en-US'], languageHelper.getPref('spellcheck.dictionaries').value);
    });

    test('no supported languages', () => {
      if (isMac) {
        return;
      }

      loadTimeData.overrideValues({
        spellCheckDisabledReason: 'no languages!',
      });

      assertFalse(languagesPage.$$('#enableSpellcheckingToggle').disabled);
      assertTrue(languageHelper.getPref('browser.enable_spellchecking').value);
      assertEquals(
          languagesPage.$$('#enableSpellcheckingToggle').subLabel, undefined);

      // Empty out supported languages
      for (const lang of languageHelper.languages.enabled) {
        languageHelper.disableLanguage(lang.language.code);
      }
      assertTrue(languagesPage.$$('#enableSpellcheckingToggle').disabled);
      assertFalse(languageHelper.getPref('browser.enable_spellchecking').value);
      assertEquals(
          languagesPage.$$('#enableSpellcheckingToggle').subLabel,
          'no languages!');
    });

    test('error handling', function() {
      if (isMac) {
        return;
      }

      const checkAllHidden = nodes => {
        assertTrue(nodes.every(node => node.hidden));
      };

      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      const spellCheckCollapse = languagesPage.$$('#spellCheckCollapse');
      const errorDivs = Array.from(
          spellCheckCollapse.querySelectorAll('.name-with-error-list div'));
      assertEquals(4, errorDivs.length);
      checkAllHidden(errorDivs);

      const retryButtons =
          Array.from(spellCheckCollapse.querySelectorAll('cr-button'));
      assertEquals(2, retryButtons.length);
      checkAllHidden(retryButtons);

      const languageCode =
          languagesPage.get('languages.enabled.0.language.code');
      languageSettingsPrivate.onSpellcheckDictionariesChanged.callListeners([
        {languageCode, isReady: false, downloadFailed: true},
      ]);

      flush();
      assertFalse(errorDivs[0].hidden);
      checkAllHidden(errorDivs.slice(1));
      assertFalse(retryButtons[0].hidden);
      assertTrue(retryButtons[1].hidden);

      // Check that more information is provided when subsequent downloads
      // fail.
      const moreInfo = errorDivs[1];
      assertTrue(moreInfo.hidden);
      // No change when status is the same as last update.
      const currentStatus =
          languagesPage.get('languages.enabled.0.downloadDictionaryStatus');
      languageSettingsPrivate.onSpellcheckDictionariesChanged.callListeners(
          [currentStatus]);
      flush();
      assertTrue(moreInfo.hidden);

      retryButtons[0].click();
      flush();
      assertFalse(moreInfo.hidden);
    });
  });

  suite(languages_page_tests.TestNames.SpellcheckOfficialBuild, function() {
    test('enabling and disabling the spelling service', () => {
      const previousValue =
          languagesPage.prefs.spellcheck.use_spelling_service.value;
      languagesPage.$$('#spellingServiceEnable').click();
      flush();
      assertNotEquals(
          previousValue,
          languagesPage.prefs.spellcheck.use_spelling_service.value);
    });
  });
});

suite(languages_page_tests.TestNames.RestructuredLanguageSettings, function() {
  /** @type {?LanguageHelper} */
  let languageHelper = null;
  /** @type {?SettingsLanguagesPageElement} */
  let languagesPage = null;
  /** @type {?LanguagesBrowserProxy} */
  let browserProxy = null;

  suiteSetup(function() {
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
      LanguagesBrowserProxyImpl.instance_ = browserProxy;

      // Set up fake languageSettingsPrivate API.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      languageSettingsPrivate.setSettingsPrefs(settingsPrefs);

      languagesPage = document.createElement('settings-languages-page');

      // Prefs would normally be data-bound to settings-languages-page.
      languagesPage.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, languagesPage, 'prefs');

      document.body.appendChild(languagesPage);
      flush();

      languageHelper = languagesPage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  teardown(function() {
    PolymerTest.clearBody();
  });

  test('languageSubpageTriggerVisible', function() {
    assertFalse(isChildVisible(languagesPage, '#languagesCollapse'));
    assertTrue(isChildVisible(languagesPage, '#languagesSubpageTrigger'));
  });

  test('languageSubpageTriggerClicked', function() {
    languagesPage.$$('#languagesSubpageTrigger').click();
    assertEquals(
        Router.getInstance().getCurrentRoute(), routes.LANGUAGE_SETTINGS);
  });

});

suite(
    languages_page_tests.TestNames.ChromeOSLanguagesSettingsUpdate, function() {
      test('shows correct structure if update is true', () => {
        loadTimeData.overrideValues({
          isChromeOSLanguagesSettingsUpdate: true,
        });
        const page = document.createElement('settings-languages-page');
        document.body.appendChild(page);
        flush();

        assertTrue(!!page.$$('#openChromeOSLanguagesSettings'));
        assertFalse(!!page.$$('cr-expand-button'));
        assertFalse(!!page.$$('#languagesCollapse'));
        assertFalse(!!page.$$('#enableSpellcheckingToggle'));
        assertFalse(!!page.$$('#spellCheckCollapse'));
      });

      test('shows correct structure if update is false', () => {
        loadTimeData.overrideValues({
          isChromeOSLanguagesSettingsUpdate: false,
        });
        const page = document.createElement('settings-languages-page');
        document.body.appendChild(page);
        flush();

        assertFalse(!!page.$$('#openChromeOSLanguagesSettings'));
        assertTrue(!!page.$$('cr-expand-button'));
        assertTrue(!!page.$$('#languagesCollapse'));
        assertTrue(!!page.$$('#enableSpellcheckingToggle'));
        assertTrue(!!page.$$('#spellCheckCollapse'));
      });
    });
