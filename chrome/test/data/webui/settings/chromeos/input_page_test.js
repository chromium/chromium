// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {LanguagesBrowserProxyImpl, LanguagesMetricsProxyImpl, LanguagesPageInteraction} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getFakeLanguagePrefs} from '../fake_language_settings_private.js'
// #import {FakeSettingsPrivate} from '../fake_settings_private.js';
// #import {TestLanguagesBrowserProxy} from './test_os_languages_browser_proxy.m.js';
// #import {TestLanguagesMetricsProxy} from './test_os_languages_metrics_proxy.m.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {fakeDataBind} from '../../test_util.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

suite('input page', () => {
  /** @type {!SettingsInputPageElement} */
  let inputPage;
  /** @type {!settings.LanguagesMetricsProxy} */
  let metricsProxy;
  /** @type {!settings.LanguagesBrowserProxy} */
  let browserProxy;
  /** @type {!LanguagesHelper} */
  let languageHelper;

  suiteSetup(() => {
    CrSettingsPrefs.deferInitialization = true;
    loadTimeData.overrideValues({imeOptionsInSettings: true});
  });

  setup(() => {
    document.body.innerHTML = '';
    const prefElement = document.createElement('settings-prefs');
    const settingsPrivate =
        new settings.FakeSettingsPrivate(settings.getFakeLanguagePrefs());
    prefElement.initialize(settingsPrivate);
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized.then(() => {
      // Set up test browser proxy.
      browserProxy = new settings.TestLanguagesBrowserProxy();
      settings.LanguagesBrowserProxyImpl.instance_ = browserProxy;

      // Sets up test metrics proxy.
      metricsProxy = new settings.TestLanguagesMetricsProxy();
      settings.LanguagesMetricsProxyImpl.instance_ = metricsProxy;

      // Set up fake languageSettingsPrivate API.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      languageSettingsPrivate.setSettingsPrefs(prefElement);

      // Instantiate the data model with data bindings for prefs.
      const settingsLanguages = document.createElement('settings-languages');
      settingsLanguages.prefs = prefElement.prefs;
      test_util.fakeDataBind(prefElement, settingsLanguages, 'prefs');
      document.body.appendChild(settingsLanguages);

      // Create page with data bindings for prefs and data model.
      inputPage = document.createElement('os-settings-input-page');
      inputPage.prefs = prefElement.prefs;
      test_util.fakeDataBind(prefElement, inputPage, 'prefs');
      inputPage.languages = settingsLanguages.languages;
      test_util.fakeDataBind(settingsLanguages, inputPage, 'languages');
      inputPage.languageHelper = settingsLanguages.languageHelper;
      test_util.fakeDataBind(settingsLanguages, inputPage, 'language-helper');
      languageHelper = inputPage.languageHelper;
      document.body.appendChild(inputPage);
    });
  });

  teardown(function() {
    settings.Router.getInstance().resetRouteForTesting();
  });

  suite('input method list', () => {
    test('displays correctly', () => {
      const inputMethodsList = inputPage.$$('#inputMethodsList');
      assertTrue(!!inputMethodsList);

      // The test input methods should appear.
      const items = inputMethodsList.querySelectorAll('.list-item');
      // Two items for input methods and one item for add input methods.
      assertEquals(3, items.length);
      assertEquals(
          'US keyboard',
          items[0].querySelector('.display-name').textContent.trim());
      assertTrue(!!items[0].querySelector('.internal-wrapper'));
      assertFalse(!!items[0].querySelector('.external-wrapper'));
      assertFalse(!!items[0].querySelector('.icon-clear').disabled);
      assertEquals(
          'US Dvorak keyboard',
          items[1].querySelector('.display-name').textContent.trim());
      assertTrue(!!items[1].querySelector('.external-wrapper'));
      assertFalse(!!items[1].querySelector('.internal-wrapper'));
      assertFalse(!!items[1].querySelector('.icon-clear').disabled);
    });

    test('navigates to input method options page', () => {
      const inputMethodsList = inputPage.$.inputMethodsList;
      const items = inputMethodsList.querySelectorAll('.list-item');
      items[0].querySelector('.subpage-arrow').click();
      const router = settings.Router.getInstance();
      assertEquals(
          router.getCurrentRoute().getAbsolutePath(),
          'chrome://os-settings/osLanguages/inputMethodOptions');
      assertEquals(
          router.getQueryParameters().get('id'),
          '_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:us::eng');
    });

    test('removes an input method', () => {
      const inputMethodName = 'US keyboard';

      let inputMethodsList = inputPage.$.inputMethodsList;
      let items = inputMethodsList.querySelectorAll('.list-item');
      assertEquals(3, items.length);
      assertEquals(
          inputMethodName,
          items[0].querySelector('.display-name').textContent.trim());

      // clicks remove input method button.
      items[0].querySelector('.icon-clear').click();
      Polymer.dom.flush();

      inputMethodsList = inputPage.$.inputMethodsList;
      items = inputMethodsList.querySelectorAll('.list-item');
      assertEquals(2, items.length);
      assertTrue(
          items[0].querySelector('.display-name').textContent.trim() !==
          inputMethodName);
    });

    test('disables remove input method option', async () => {
      // Add US Swahili keyboard, a third party IME
      languageHelper.addInputMethod(
          'ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:sw');
      // Remove US Dvorak keyboard, so there is only 1 component IME left.
      languageHelper.removeInputMethod(
          '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us:dvorak:eng');
      Polymer.dom.flush();

      const inputMethodsList = inputPage.$.inputMethodsList;
      const items = inputMethodsList.querySelectorAll('.list-item');
      assertEquals(3, items.length);
      assertEquals(
          'US keyboard',
          items[0].querySelector('.display-name').textContent.trim());
      assertTrue(!!items[0].querySelector('.icon-clear').disabled);
      assertEquals(
          'US Swahili keyboard',
          items[1].querySelector('.display-name').textContent.trim());
      assertFalse(!!items[1].querySelector('.icon-clear').disabled);
    });

    test('shows managed input methods label', () => {
      const inputMethodsManagedbyPolicy =
          inputPage.$$('#inputMethodsManagedbyPolicy');
      assertFalse(!!inputMethodsManagedbyPolicy);

      inputPage.setPrefValue(
          'settings.language.allowed_input_methods', ['xkb:us::eng']);
      Polymer.dom.flush();

      assertTrue(!!inputPage.$$('#inputMethodsManagedbyPolicy'));
    });
  });

  suite('input page', () => {
    test('Deep link to spell check', async () => {
      loadTimeData.overrideValues({
        isDeepLinkingEnabled: true,
      });

      const params = new URLSearchParams;
      params.append('settingId', '1207');
      settings.Router.getInstance().navigateTo(
          settings.routes.OS_LANGUAGES_INPUT, params);

      Polymer.dom.flush();

      const deepLinkElement =
          inputPage.$$('#enableSpellcheckingToggle').$$('cr-toggle');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Spell check toggle should be focused for settingId=1207.');
    });
  });

  suite('add input methods dialog', () => {
    let dialog;
    let suggestedInputMethods;
    let allInputMethods;
    let cancelButton;
    let actionButton;

    setup(() => {
      assertFalse(!!inputPage.$$('os-settings-add-input-methods-dialog'));
      inputPage.$$('#addInputMethod').click();
      Polymer.dom.flush();

      dialog = inputPage.$$('os-settings-add-input-methods-dialog');
      assertTrue(!!dialog);

      actionButton = dialog.$$('.action-button');
      assertTrue(!!actionButton);
      cancelButton = dialog.$$('.cancel-button');
      assertTrue(!!cancelButton);

      suggestedInputMethods = dialog.$$('#suggestedInputMethods');
      assertTrue(!!suggestedInputMethods);

      allInputMethods = dialog.$$('#allInputMethods');
      assertTrue(!!allInputMethods);

      // No input methods has been selected, so the action button is disabled.
      assertTrue(actionButton.disabled);
      assertFalse(cancelButton.disabled);
    });

    test('has action button working correctly', () => {
      const listItems = suggestedInputMethods.querySelectorAll('.list-item');
      // selecting a language enables action button
      listItems[0].click();
      assertFalse(actionButton.disabled);

      // selecting the same language again disables action button
      listItems[0].click();
      assertTrue(actionButton.disabled);
    });

    test('has correct structure and adds input methods', () => {
      const suggestedItems =
          suggestedInputMethods.querySelectorAll('.list-item');
      // input methods are based on and ordered by enabled languages
      // only allowed input methods are shown.
      assertEquals(2, suggestedItems.length);
      assertEquals('US Swahili keyboard', suggestedItems[0].textContent.trim());
      assertEquals('Swahili keyboard', suggestedItems[1].textContent.trim());
      // selecting Swahili keyboard.
      suggestedItems[1].click();

      const allItems = allInputMethods.querySelectorAll('.list-item');
      // All input methods should appear and ordered based on fake settings
      // data.
      assertEquals(4, allItems.length);

      const expectedItems = [
        {
          name: 'Swahili keyboard',
          checkboxDisabled: false,
          checkboxChecked: true,
          policyIcon: false,
        },
        {
          name: 'US Swahili keyboard',
          checkboxDisabled: false,
          checkboxChecked: false,
          policyIcon: false,
        },
        {
          name: 'US International keyboard',
          checkboxDisabled: true,
          checkboxChecked: false,
          policyIcon: true,
        },
        {
          name: 'Vietnamese keyboard',
          checkboxDisabled: false,
          checkboxChecked: false,
          policyIcon: false,
        },
      ];

      for (let i = 0; i < allItems.length; i++) {
        assertTrue(
            allItems[i].textContent.includes(expectedItems[i].name),
            `expect ${allItems[i].textContent} to include ${
                expectedItems[i].name}`);
        assertEquals(
            expectedItems[i].checkboxDisabled,
            allItems[i].querySelector('cr-checkbox').disabled,
            `expect ${expectedItems[i].name}'s checkbox disabled state to be ${
                expectedItems[i].checkboxDisabled}`);
        assertEquals(
            expectedItems[i].checkboxChecked,
            allItems[i].querySelector('cr-checkbox').checked,
            `expect ${expectedItems[i].name}'s checkbox checked state to be ${
                expectedItems[i].checkboxChecked}`);
        assertEquals(
            expectedItems[i].policyIcon,
            !!allItems[i].querySelector('iron-icon'),
            `expect ${expectedItems[i].name}'s policy icon presence to be ${
                expectedItems[i].policyIcon}`);
      }

      // selecting Vietnamese keyboard
      allItems[3].querySelector('cr-checkbox').click();

      actionButton.click();

      assertTrue(languageHelper.isInputMethodEnabled(
          '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:sw:sw'));
      assertFalse(languageHelper.isInputMethodEnabled(
          'ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:sw'));
      assertTrue(languageHelper.isInputMethodEnabled(
          '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:vi:vi'));
    });

    test('suggested input methods hidden when no languages is enabled', () => {
      languageHelper.setPrefValue('settings.language.preferred_languages', '');
      Polymer.dom.flush();

      suggestedInputMethods = dialog.$$('#suggestedInputMethods');
      // suggested input methods is rendered previously.
      assertTrue(!!suggestedInputMethods);
      assertEquals('none', getComputedStyle(suggestedInputMethods).display);
    });

    test('suggested input methods hidden when no input methods left', () => {
      const languageCode = 'sw';
      languageHelper.setPrefValue(
          'settings.language.preferred_languages', languageCode);
      languageHelper.getInputMethodsForLanguage(languageCode)
          .forEach(inputMethod => {
            languageHelper.addInputMethod(inputMethod.id);
          });
      Polymer.dom.flush();

      suggestedInputMethods = dialog.$$('#suggestedInputMethods');
      // suggested input methods is rendered previously.
      assertTrue(!!suggestedInputMethods);
      assertEquals('none', getComputedStyle(suggestedInputMethods).display);
    });

    test('searches input methods correctly', () => {
      const searchInput = dialog.$$('cr-search-field');
      const getItems = function() {
        return allInputMethods.querySelectorAll('.list-item:not([hidden])');
      };

      assertFalse(dialog.$$('#allInputMethodsLabel').hidden);
      assertEquals('block', getComputedStyle(suggestedInputMethods).display);

      // Expecting a few languages to be displayed when no query exists.
      assertGE(getItems().length, 1);

      // Search hides suggestedInputMethods and allInputMethodsLabel.
      searchInput.setValue('v');
      Polymer.dom.flush();
      assertTrue(dialog.$$('#allInputMethodsLabel').hidden);
      assertEquals('none', getComputedStyle(suggestedInputMethods).display);

      // Search input methods name
      searchInput.setValue('vietnamese');
      Polymer.dom.flush();
      assertEquals(1, getItems().length);
      assertTrue(getItems()[0].textContent.includes('Vietnamese'));

      // Search input methods' language
      searchInput.setValue('Turkmen');
      Polymer.dom.flush();
      assertEquals(1, getItems().length);
      assertTrue(getItems()[0].textContent.includes('Swahili keyboard'));
    });

    test('has escape key behavior working correctly', function() {
      const searchInput = dialog.$$('cr-search-field');
      searchInput.setValue('dummyquery');

      // Test that dialog is not closed if 'Escape' is pressed on the input
      // and a search query exists.
      MockInteractions.keyDownOn(searchInput, 19, [], 'Escape');
      assertTrue(dialog.$.dialog.open);

      // Test that dialog is closed if 'Escape' is pressed on the input and no
      // search query exists.
      searchInput.setValue('');
      MockInteractions.keyDownOn(searchInput, 19, [], 'Escape');
      assertFalse(dialog.$.dialog.open);
    });
  });

  suite('records metrics', () => {
    test('when deactivating show ime menu', async () => {
      inputPage.setPrefValue('settings.language.ime_menu_activated', true);
      inputPage.$$('#showImeMenu').click();
      Polymer.dom.flush();

      assertFalse(
          await metricsProxy.whenCalled('recordToggleShowInputOptionsOnShelf'));
    });

    test('when activating show ime menu', async () => {
      inputPage.setPrefValue('settings.language.ime_menu_activated', false);
      inputPage.$$('#showImeMenu').click();
      Polymer.dom.flush();

      assertTrue(
          await metricsProxy.whenCalled('recordToggleShowInputOptionsOnShelf'));
    });

    test('when adding input methods', async () => {
      inputPage.$$('#addInputMethod').click();
      Polymer.dom.flush();

      await metricsProxy.whenCalled('recordAddInputMethod');
    });

    test('when switch input method', async () => {
      const inputMethodsList = inputPage.$$('#inputMethodsList');
      assertTrue(!!inputMethodsList);

      // The test input methods should appear.
      const items = inputMethodsList.querySelectorAll('.list-item');
      items[0].click();
      assertEquals(
          settings.LanguagesPageInteraction.SWITCH_INPUT_METHOD,
          await metricsProxy.whenCalled('recordInteraction'));
    });
  });

  suite('spell check v1', () => {
    let spellCheckToggle;
    let spellCheckListContainer;
    let spellCheckList;

    setup(() => {
      // Disable Update 2.
      // We use the property directly instead of loadTimeData, as overriding
      // loadTimeData does not work as the property is set using a value().
      inputPage.languageSettingsV2Update2Enabled_ = false;
      Polymer.dom.flush();
      // spell check is initially on
      spellCheckToggle = inputPage.$.enableSpellcheckingToggle;
      assertTrue(!!spellCheckToggle);
      assertTrue(spellCheckToggle.checked);

      spellCheckListContainer = inputPage.$$('#spellCheckLanguagesList');
      assertTrue(!!spellCheckListContainer);

      // two languages are in the list, with en-US on and sw off.
      spellCheckList = spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(2, spellCheckList.length);
      assertTrue(
          spellCheckList[0].textContent.includes('English (United States)'));
      assertTrue(spellCheckList[0].querySelector('cr-toggle').checked);
      assertTrue(spellCheckList[1].textContent.includes('Swahili'));
      assertFalse(spellCheckList[1].querySelector('cr-toggle').checked);
    });

    test('toggles a spell check language add/remove it from dictionary', () => {
      assertDeepEquals(
          ['en-US'], languageHelper.prefs.spellcheck.dictionaries.value);
      // Get toggle for en-US.
      const spellCheckLanguageToggle =
          spellCheckList[0].querySelector('cr-toggle');

      // toggle off
      spellCheckLanguageToggle.click();

      assertFalse(spellCheckLanguageToggle.checked);
      assertDeepEquals([], languageHelper.prefs.spellcheck.dictionaries.value);

      // toggle on
      spellCheckLanguageToggle.click();

      assertTrue(spellCheckLanguageToggle.checked);
      assertDeepEquals(
          ['en-US'], languageHelper.prefs.spellcheck.dictionaries.value);
    });

    test(
        'clicks a spell check language name add/remove it from dictionary',
        () => {
          assertDeepEquals(
              ['en-US'], languageHelper.prefs.spellcheck.dictionaries.value);
          // Get toggle for en-US.
          const spellCheckLanguageToggle =
              spellCheckList[0].querySelector('cr-toggle');

          // toggle off by clicking name
          spellCheckList[0].querySelector('.name-with-error').click();
          Polymer.dom.flush();

          assertFalse(spellCheckLanguageToggle.checked);
          assertDeepEquals(
              [], languageHelper.prefs.spellcheck.dictionaries.value);

          // toggle on by clicking name
          spellCheckList[0].querySelector('.name-with-error').click();
          Polymer.dom.flush();

          assertTrue(spellCheckLanguageToggle.checked);
          assertDeepEquals(
              ['en-US'], languageHelper.prefs.spellcheck.dictionaries.value);
        });

    test('shows force-on existing spell check language', () => {
      // Force-enable an existing language via policy.
      languageHelper.setPrefValue('spellcheck.forced_dictionaries', ['sw']);
      Polymer.dom.flush();

      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(2, newSpellCheckList.length);
      const forceEnabledSwLanguageRow = newSpellCheckList[1];
      assertTrue(!!forceEnabledSwLanguageRow);
      assertTrue(!!forceEnabledSwLanguageRow.querySelector(
          'cr-policy-pref-indicator'));
      assertTrue(
          forceEnabledSwLanguageRow.querySelector('.managed-toggle').checked);
      assertTrue(
          forceEnabledSwLanguageRow.querySelector('.managed-toggle').disabled);
      assertEquals(
          getComputedStyle(
              forceEnabledSwLanguageRow.querySelector('.name-with-error'))
              .pointerEvents,
          'none');
    });

    test('shows force-on non-enabled spell check language', () => {
      // Force-enable a new language via policy.
      languageHelper.setPrefValue('spellcheck.forced_dictionaries', ['nb']);
      Polymer.dom.flush();

      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(3, newSpellCheckList.length);
      const forceEnabledNbLanguageRow = newSpellCheckList[2];
      assertTrue(!!forceEnabledNbLanguageRow);
      assertTrue(!!forceEnabledNbLanguageRow.querySelector(
          'cr-policy-pref-indicator'));
      assertTrue(
          forceEnabledNbLanguageRow.querySelector('.managed-toggle').checked);
      assertTrue(
          forceEnabledNbLanguageRow.querySelector('.managed-toggle').disabled);
      assertEquals(
          getComputedStyle(
              forceEnabledNbLanguageRow.querySelector('.name-with-error'))
              .pointerEvents,
          'none');
    });

    test('can disable non-enabled spell check language', () => {
      // Add a new language to spellcheck.dictionaries which isn't enabled.
      languageHelper.setPrefValue('spellcheck.dictionaries', ['en-US', 'nb']);
      Polymer.dom.flush();

      let newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      // The spell check list should have en-US (enabled), sw (disabled) and
      // nb (enabled).
      assertEquals(3, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));
      assertTrue(newSpellCheckList[0].querySelector('cr-toggle').checked);
      assertTrue(newSpellCheckList[1].textContent.includes('Swahili'));
      assertFalse(newSpellCheckList[1].querySelector('cr-toggle').checked);
      assertTrue(newSpellCheckList[2].textContent.includes('Norwegian Bokmål'));
      assertTrue(newSpellCheckList[2].querySelector('cr-toggle').checked);

      // Disable nb.
      newSpellCheckList[2].querySelector('cr-toggle').click();
      Polymer.dom.flush();
      newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should have en-US (enabled) and sw (disabled).
      assertEquals(2, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));
      assertTrue(newSpellCheckList[0].querySelector('cr-toggle').checked);
      assertTrue(newSpellCheckList[1].textContent.includes('Swahili'));
      assertFalse(newSpellCheckList[1].querySelector('cr-toggle').checked);

      assertDeepEquals(
          ['en-US'], languageHelper.prefs.spellcheck.dictionaries.value);
    });

    test(
        'does not show force-off spell check when language is not enabled',
        () => {
          // Force-disable a language via policy.
          languageHelper.setPrefValue(
              'spellcheck.blocked_dictionaries', ['nb']);
          Polymer.dom.flush();
          const newSpellCheckList =
              spellCheckListContainer.querySelectorAll('.list-item');
          assertEquals(2, newSpellCheckList.length);
        });

    test('shows force-off spell check when language is enabled', () => {
      // Force-disable a language via policy.
      languageHelper.setPrefValue('spellcheck.blocked_dictionaries', ['nb']);
      languageHelper.enableLanguage('nb');
      Polymer.dom.flush();

      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(3, newSpellCheckList.length);
      const forceDisabledNbLanguageRow = newSpellCheckList[2];
      assertTrue(!!forceDisabledNbLanguageRow.querySelector(
          'cr-policy-pref-indicator'));
      assertFalse(
          forceDisabledNbLanguageRow.querySelector('.managed-toggle').checked);
      assertTrue(
          forceDisabledNbLanguageRow.querySelector('.managed-toggle').disabled);
      assertEquals(
          getComputedStyle(
              forceDisabledNbLanguageRow.querySelector('.name-with-error'))
              .pointerEvents,
          'none');
    });

    test('toggle off disables toggle and click event', () => {
      // Initially, both toggles are enabled
      assertFalse(spellCheckList[0].querySelector('cr-toggle').disabled);
      assertFalse(spellCheckList[1].querySelector('cr-toggle').disabled);
      assertEquals(
          getComputedStyle(spellCheckList[0].querySelector('.name-with-error'))
              .pointerEvents,
          'auto');
      assertEquals(
          getComputedStyle(spellCheckList[1].querySelector('.name-with-error'))
              .pointerEvents,
          'auto');

      spellCheckToggle.click();

      assertFalse(spellCheckToggle.checked);
      assertTrue(spellCheckList[0].querySelector('cr-toggle').disabled);
      assertTrue(spellCheckList[1].querySelector('cr-toggle').disabled);
      assertEquals(
          getComputedStyle(spellCheckList[0].querySelector('.name-with-error'))
              .pointerEvents,
          'none');
      assertEquals(
          getComputedStyle(spellCheckList[1].querySelector('.name-with-error'))
              .pointerEvents,
          'none');
    });

    test('does not add a language without spellcheck support', () => {
      const spellCheckLanguagesCount = spellCheckList.length;
      // Enabling a language without spellcheck support should not add it to
      // the list
      languageHelper.enableLanguage('tk');
      Polymer.dom.flush();
      assertEquals(spellCheckList.length, spellCheckLanguagesCount);
    });

    test('toggle is disabled when there are no supported languages', () => {
      assertFalse(spellCheckToggle.disabled);

      // Empty out supported languages
      for (const lang of languageHelper.languages.enabled) {
        languageHelper.disableLanguage(lang.language.code);
      }

      assertTrue(spellCheckToggle.disabled);
      assertFalse(spellCheckToggle.checked);
    });

    test('error handling', () => {
      const checkAllHidden = nodes => {
        assertTrue(nodes.every(node => node.hidden));
      };

      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      const errorDivs = Array.from(
          spellCheckListContainer.querySelectorAll('.name-with-error div'));
      assertEquals(2, errorDivs.length);
      checkAllHidden(errorDivs);

      const retryButtons =
          Array.from(spellCheckListContainer.querySelectorAll('cr-button'));
      assertEquals(2, retryButtons.length);

      const languageCode = inputPage.get('languages.enabled.0.language.code');
      languageSettingsPrivate.onSpellcheckDictionariesChanged.callListeners([
        {languageCode, isReady: false, downloadFailed: true},
      ]);

      Polymer.dom.flush();
      assertFalse(errorDivs[0].hidden);
      assertFalse(retryButtons[0].hidden);
      assertFalse(retryButtons[0].disabled);

      // turns off spell check disable retry button.
      spellCheckToggle.click();
      assertTrue(retryButtons[0].disabled);

      // turns spell check back on and enable download.
      spellCheckToggle.click();
      languageSettingsPrivate.onSpellcheckDictionariesChanged.callListeners([
        {languageCode, isReady: true, downloadFailed: false},
      ]);

      Polymer.dom.flush();
      assertTrue(errorDivs[0].hidden);
      assertTrue(retryButtons[0].hidden);
    });

    test('toggle off disables enhanced spell check', () => {
      const enhancedSpellCheckToggle =
          inputPage.$$('#enhancedSpellCheckToggle');
      assertFalse(enhancedSpellCheckToggle.disabled);
      spellCheckToggle.click();

      assertTrue(enhancedSpellCheckToggle.disabled);
    });

    test('toggle off disables edit dictionary', () => {
      const editDictionarySubpageTrigger =
          inputPage.$$('#editDictionarySubpageTrigger');
      assertFalse(editDictionarySubpageTrigger.disabled);
      spellCheckToggle.click();

      assertTrue(editDictionarySubpageTrigger.disabled);
    });

    test('opens edit dictionary page', () => {
      const editDictionarySubpageTrigger =
          inputPage.$$('#editDictionarySubpageTrigger');
      editDictionarySubpageTrigger.click();
      const router = settings.Router.getInstance();
      assertEquals(
          router.getCurrentRoute().getAbsolutePath(),
          'chrome://os-settings/osLanguages/editDictionary');
    });
  });

  suite('spell check v2', () => {
    let spellCheckToggle;
    let spellCheckListContainer;
    // This list is not dynamically updated.
    let spellCheckList;

    setup(() => {
      // Enable Update 2.
      // We use the property directly instead of loadTimeData, as overriding
      // loadTimeData does not work as the property is set using a value().
      inputPage.languageSettingsV2Update2Enabled_ = true;
      Polymer.dom.flush();

      // Spell check is initially on.
      spellCheckToggle = inputPage.$.enableSpellcheckingToggle;
      assertTrue(!!spellCheckToggle);
      assertTrue(spellCheckToggle.checked);

      spellCheckListContainer = inputPage.$$('#spellCheckLanguagesListV2');
      assertTrue(!!spellCheckListContainer);

      // The spell check list should only have en-US (excluding the "add
      // languages" button).
      spellCheckList = spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(1 + 1, spellCheckList.length);
      assertTrue(
          spellCheckList[0].textContent.includes('English (United States)'));
      assertTrue(spellCheckList[1].textContent.includes('Add languages'));
    });

    test('can remove enabled language from spell check list', () => {
      assertDeepEquals(
          ['en-US'], languageHelper.prefs.spellcheck.dictionaries.value);
      // Get remove button for en-US.
      const spellCheckLanguageToggle =
          spellCheckList[0].querySelector('cr-icon-button');

      // Remove the language.
      spellCheckLanguageToggle.click();
      Polymer.dom.flush();

      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      // The spell check list should just have "add languages".
      assertEquals(0 + 1, newSpellCheckList.length);

      assertDeepEquals([], languageHelper.prefs.spellcheck.dictionaries.value);
    });

    test('can remove non-enabled language from spell check list', () => {
      // Add a new non-enabled language to spellcheck.dictionaries.
      languageHelper.setPrefValue('spellcheck.dictionaries', ['en-US', 'nb']);
      Polymer.dom.flush();

      let newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should have en-US, nb and "add languages".
      assertEquals(2 + 1, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));
      assertTrue(newSpellCheckList[1].textContent.includes('Norwegian Bokmål'));

      // Remove nb.
      newSpellCheckList[1].querySelector('cr-icon-button').click();
      Polymer.dom.flush();
      newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should have en-US and "add languages".
      assertEquals(1 + 1, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));

      assertDeepEquals(
          ['en-US'], languageHelper.prefs.spellcheck.dictionaries.value);
    });

    test('shows force-on spell check language turned on by user', () => {
      // Force-enable a spell check language originally set by the user.
      languageHelper.setPrefValue('spellcheck.forced_dictionaries', ['en-US']);
      Polymer.dom.flush();

      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should have en-US and "add languages".
      assertEquals(1 + 1, newSpellCheckList.length);

      const forceEnabledEnUSLanguageRow = newSpellCheckList[0];
      assertTrue(forceEnabledEnUSLanguageRow.textContent.includes(
          'English (United States)'));
      assertTrue(!!forceEnabledEnUSLanguageRow.querySelector(
          'cr-policy-pref-indicator'));
      // Polymer sometimes hides the old enabled element by using a
      // display: none, so we use the managed-button class to get a reference to
      // the new disabled button.
      const managedButton =
          forceEnabledEnUSLanguageRow.querySelector('.managed-button');
      assertTrue(managedButton.disabled);
    });

    test('shows force-on enabled spell check language', () => {
      // Force-enable an enabled language via policy.
      languageHelper.setPrefValue('spellcheck.forced_dictionaries', ['sw']);
      Polymer.dom.flush();

      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should have en-US, sw and "add languages".
      assertEquals(2 + 1, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));

      const forceEnabledSwLanguageRow = newSpellCheckList[1];
      assertTrue(forceEnabledSwLanguageRow.textContent.includes('Swahili'));
      assertTrue(!!forceEnabledSwLanguageRow.querySelector(
          'cr-policy-pref-indicator'));
      const managedButton =
          forceEnabledSwLanguageRow.querySelector('.managed-button');
      assertTrue(managedButton.disabled);
    });

    test('shows force-on non-enabled spell check language', () => {
      // Force-enable a non-enabled language via policy.
      languageHelper.setPrefValue('spellcheck.forced_dictionaries', ['nb']);
      Polymer.dom.flush();

      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should have en-US, nb and "add languages".
      assertEquals(2 + 1, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));

      const forceEnabledNbLanguageRow = newSpellCheckList[1];
      assertTrue(
          forceEnabledNbLanguageRow.textContent.includes('Norwegian Bokmål'));
      assertTrue(!!forceEnabledNbLanguageRow.querySelector(
          'cr-policy-pref-indicator'));
      const managedButton =
          forceEnabledNbLanguageRow.querySelector('.managed-button');
      assertTrue(managedButton.disabled);
    });

    test('does not show force-off spell check language enabled by user', () => {
      // Force-disable a spell check language originally set by the user.
      languageHelper.setPrefValue('spellcheck.blocked_dictionaries', ['en-US']);
      Polymer.dom.flush();

      // The spell check list should just have "add languages".
      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(0 + 1, newSpellCheckList.length);
    });

    test('does not show force-off enabled spell check language', () => {
      // Force-disable an enabled language via policy.
      languageHelper.setPrefValue('spellcheck.blocked_dictionaries', ['sw']);
      Polymer.dom.flush();

      // The spell check list should be the same (en-US, "add languages").
      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(1 + 1, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));
    });

    test('does not show force-off non-enabled spell check language', () => {
      // Force-disable a non-enabled language via policy.
      languageHelper.setPrefValue('spellcheck.blocked_dictionaries', ['nb']);
      Polymer.dom.flush();

      // The spell check list should be the same (en-US, "add languages").
      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(1 + 1, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));
    });

    test('toggle off disables buttons', () => {
      assertTrue(spellCheckToggle.checked);
      assertFalse(spellCheckList[0].querySelector('cr-icon-button').disabled);
      // "Add languages" uses a cr-button instead of a cr-icon-button.
      assertFalse(spellCheckList[1].querySelector('cr-button').disabled);

      spellCheckToggle.click();

      assertFalse(spellCheckToggle.checked);
      assertTrue(spellCheckList[0].querySelector('cr-icon-button').disabled);
      assertTrue(spellCheckList[1].querySelector('cr-button').disabled);
    });

    test('languages are in sorted order', () => {
      languageHelper.setPrefValue(
          'spellcheck.dictionaries', ['sw', 'en-US', 'nb', 'en-CA']);
      Polymer.dom.flush();
      // The spell check list should be sorted by display name:
      // English (Canada), English (United States), Norwegian Bokmål, then
      // Swahili.
      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(4 + 1, newSpellCheckList.length);
      assertTrue(newSpellCheckList[0].textContent.includes('English (Canada)'));
      assertTrue(
          newSpellCheckList[1].textContent.includes('English (United States)'));
      assertTrue(newSpellCheckList[2].textContent.includes('Norwegian Bokmål'));
      assertTrue(newSpellCheckList[3].textContent.includes('Swahili'));
    });

    test('error handling', () => {
      // Enable Swahili so we have two languages for testing.
      languageHelper.setPrefValue('spellcheck.dictionaries', ['en-US', 'sw']);
      Polymer.dom.flush();
      const checkAllHidden = nodes => {
        assertTrue(nodes.every(node => node.hidden));
      };

      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      const errorDivs = Array.from(
          spellCheckListContainer.querySelectorAll('.name-with-error div'));
      assertEquals(2, errorDivs.length);
      checkAllHidden(errorDivs);

      const retryButtons = Array.from(spellCheckListContainer.querySelectorAll(
          'cr-button:not(#addSpellcheckLanguages)'));
      assertEquals(2, retryButtons.length);

      const languageCode = inputPage.get('languages.enabled.0.language.code');
      languageSettingsPrivate.onSpellcheckDictionariesChanged.callListeners([
        {languageCode, isReady: false, downloadFailed: true},
      ]);

      Polymer.dom.flush();
      assertFalse(errorDivs[0].hidden);
      assertFalse(retryButtons[0].hidden);
      assertFalse(retryButtons[0].disabled);

      // turns off spell check disable retry button.
      spellCheckToggle.click();
      assertTrue(retryButtons[0].disabled);

      // turns spell check back on and enable download.
      spellCheckToggle.click();
      languageSettingsPrivate.onSpellcheckDictionariesChanged.callListeners([
        {languageCode, isReady: true, downloadFailed: false},
      ]);

      Polymer.dom.flush();
      assertTrue(errorDivs[0].hidden);
      assertTrue(retryButtons[0].hidden);
    });

    test('toggle off disables enhanced spell check', () => {
      const enhancedSpellCheckToggle =
          inputPage.$$('#enhancedSpellCheckToggle');
      assertFalse(enhancedSpellCheckToggle.disabled);
      spellCheckToggle.click();

      assertTrue(enhancedSpellCheckToggle.disabled);
    });

    test('toggle off disables edit dictionary', () => {
      const editDictionarySubpageTrigger =
          inputPage.$$('#editDictionarySubpageTrigger');
      assertFalse(editDictionarySubpageTrigger.disabled);
      spellCheckToggle.click();

      assertTrue(editDictionarySubpageTrigger.disabled);
    });

    test('opens edit dictionary page', () => {
      const editDictionarySubpageTrigger =
          inputPage.$$('#editDictionarySubpageTrigger');
      editDictionarySubpageTrigger.click();
      const router = settings.Router.getInstance();
      assertEquals(
          router.getCurrentRoute().getAbsolutePath(),
          'chrome://os-settings/osLanguages/editDictionary');
    });
  });

  suite('add spell check languages dialog', () => {
    let dialog;
    let cancelButton;
    let actionButton;

    setup(() => {
      // Enable Update 2.
      // We use the property directly instead of loadTimeData, as overriding
      // loadTimeData does not work as the property is set using a value().
      inputPage.languageSettingsV2Update2Enabled_ = true;
      Polymer.dom.flush();

      assertFalse(
          !!inputPage.$$('os-settings-add-spellcheck-languages-dialog'));
      inputPage.$$('#addSpellcheckLanguages').click();
      Polymer.dom.flush();

      dialog = inputPage.$$('os-settings-add-spellcheck-languages-dialog');
      assertTrue(!!dialog);
      assertTrue(dialog.$.dialog.open);

      actionButton = dialog.$$('.action-button');
      assertTrue(!!actionButton);
      cancelButton = dialog.$$('.cancel-button');
      assertTrue(!!cancelButton);
    });

    test('action button is enabled and disabled when necessary', () => {
      // Mimic $$, but with a querySelectorAll instead of querySelector.
      const checkboxes = dialog.root.querySelectorAll('cr-checkbox');

      // By default, no languages have been selected so the action button is
      // disabled.
      assertTrue(actionButton.disabled);

      // Selecting a language enables the action button.
      checkboxes[0].click();
      assertFalse(actionButton.disabled);

      // Selecting the same language again disables the action button.
      checkboxes[0].click();
      assertTrue(actionButton.disabled);
    });

    test('cancel button is never disabled', () => {
      assertFalse(cancelButton.disabled);
    });

    test('initial expected layout', () => {
      // There are four languages with spell check enabled in
      // fake_language_settings_private.js: en-US, en-CA, sw, nb.
      // en-US shouldn't be displayed as it is already enabled.
      const listItems = dialog.root.querySelectorAll('.list-item');
      assertTrue(listItems[0].textContent.includes('English (Canada)'));
      assertTrue(listItems[1].textContent.includes('Swahili'));
      assertTrue(listItems[2].textContent.includes('Norwegian Bokmål'));

      // By default, all checkboxes should not be disabled, and should not be
      // checked.
      const checkboxes =
          [...listItems].map(listItem => listItem.querySelector('cr-checkbox'));
      assertTrue(checkboxes.every(checkbox => !checkbox.disabled));
      assertTrue(checkboxes.every(checkbox => !checkbox.checked));
    });

    test('can add single language and uncheck language', () => {
      const checkboxes = dialog.root.querySelectorAll('cr-checkbox');
      const swCheckbox = checkboxes[1];
      const nbCheckbox = checkboxes[2];

      // By default, en-US should be the only enabled spell check dictionary.
      assertDeepEquals(
          ['en-US'], languageHelper.prefs.spellcheck.dictionaries.value);

      swCheckbox.click();
      assertTrue(swCheckbox.checked);

      // Check and uncheck nb to ensure that it gets "ignored".
      nbCheckbox.click();
      assertTrue(nbCheckbox.checked);

      nbCheckbox.click();
      assertFalse(nbCheckbox.checked);

      actionButton.click();
      assertDeepEquals(
          ['en-US', 'sw'], languageHelper.prefs.spellcheck.dictionaries.value);
      assertFalse(dialog.$.dialog.open);
    });

    test('can add multiple languages', () => {
      const checkboxes = dialog.root.querySelectorAll('cr-checkbox');

      assertDeepEquals(
          ['en-US'], languageHelper.prefs.spellcheck.dictionaries.value);

      // Click en-CA and nb.
      checkboxes[0].click();
      assertTrue(checkboxes[0].checked);
      checkboxes[2].click();
      assertTrue(checkboxes[2].checked);

      actionButton.click();
      // The two possible results are en-US, en-CA, nb OR en-US, nb, en-CA.
      // We do not care about the ordering of the last two, but the first one
      // should still be en-US.
      assertEquals(
          'en-US', languageHelper.prefs.spellcheck.dictionaries.value[0]);
      // Note that .sort() mutates the array, but as this is the end of the test
      // the prefs will be reset after this anyway.
      assertDeepEquals(
          ['en-CA', 'en-US', 'nb'],
          languageHelper.prefs.spellcheck.dictionaries.value.sort());
      assertFalse(dialog.$.dialog.open);
    });

    test('policy disabled languages cannot be selected and show icon', () => {
      // Force-disable sw.
      languageHelper.setPrefValue('spellcheck.blocked_dictionaries', ['sw']);
      Polymer.dom.flush();

      const listItems = dialog.root.querySelectorAll('.list-item');
      const swListItem = listItems[1];
      const swCheckbox = swListItem.querySelector('cr-checkbox');
      const swPolicyIcon = swListItem.querySelector('iron-icon');

      assertTrue(swCheckbox.disabled);
      assertFalse(swCheckbox.checked);
      assertTrue(!!swPolicyIcon);
    });
  });
});
