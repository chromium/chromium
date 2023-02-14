// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputsShortcutReminderState, LanguagesBrowserProxyImpl, LanguagesMetricsProxyImpl, LanguagesPageInteraction} from 'chrome://os-settings/chromeos/lazy_load.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {fakeDataBind, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {FakeSettingsPrivate} from './fake_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_os_languages_browser_proxy.js';
import {TestLanguagesMetricsProxy} from './test_os_languages_metrics_proxy.js';

suite('input page', () => {
  /** @type {!SettingsInputPageElement} */
  let inputPage;
  /** @type {!LanguagesMetricsProxy} */
  let metricsProxy;
  /** @type {!LanguagesBrowserProxy} */
  let browserProxy;
  /** @type {!LanguagesHelper} */
  let languageHelper;

  suiteSetup(() => {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(() => {
    document.body.innerHTML = '';
    const prefElement = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
    prefElement.initialize(settingsPrivate);

    /**
     * Prefs listener to emulate SpellcheckService listeners.
     * As we use a mocked prefs object in tests, we also need to mock the
     * behavior of SpellcheckService as it relies on a C++ PrefChangeRegistrar
     * to listen to pref changes - which do not work when the prefs are mocked.
     * @param {!Array<!chrome.settingsPrivate.PrefObject>} prefs
     */
    function spellCheckServiceListener(prefs) {
      for (const pref of prefs) {
        switch (pref.key) {
          case 'spellcheck.dictionaries':
            // Emulate SpellcheckService::OnSpellCheckDictionariesChanged:
            // If there are no dictionaries, set browser.enable_spellchecking
            // to false.
            if (pref.value.length === 0) {
              settingsPrivate.setPref(
                  'browser.enable_spellchecking', false, '', () => {});
            }
            break;

          case 'intl.accept_languages':
            // Emulate SpellcheckService::OnAcceptLanguagesChanged:
            // Filter spellcheck.dictionaries and remove all dictionaries not
            // in intl.accept_languages. We won't "normalize" it here as it is
            // extremely difficult to do in JavaScript, and should not matter
            // for tests.
            // Disabled for LSV2 Update 2.
            if (inputPage.languageSettingsV2Update2Enabled_) {
              break;
            }

            // Normally, getting prefs is an asynchronous action with callbacks,
            // but we can cheat in tests using FakeSettingsPrivate.
            const dictionaries =
                settingsPrivate.prefs['spellcheck.dictionaries'].value;
            const acceptLanguages = new Set(pref.value.split(','));

            const filteredDictionaries = dictionaries.filter(
                dictionary => acceptLanguages.has(dictionary));
            settingsPrivate.setPref(
                'spellcheck.dictionaries', filteredDictionaries, '', () => {});
            break;
        }
      }
    }

    // Listen to prefs changes using settingsPrivate.onPrefsChanged.
    // While prefElement (<settings-prefs>) is normally a synchronous wrapper
    // around the asynchronous settingsPrivate, the two's prefs are always
    // synchronously kept in sync both ways in tests.
    // However, it's possible that a settingsPrivate.onPrefsChanged listener
    // receives a change before prefElement does if the change is made by
    // settingsPrivate, so prefer to use settingsPrivate getters/setters
    // whenever possible.
    settingsPrivate.onPrefsChanged.addListener(spellCheckServiceListener);

    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized.then(() => {
      // Set up test browser proxy.
      browserProxy = new TestLanguagesBrowserProxy();
      LanguagesBrowserProxyImpl.setInstanceForTesting(browserProxy);

      // Sets up test metrics proxy.
      metricsProxy = new TestLanguagesMetricsProxy();
      LanguagesMetricsProxyImpl.setInstanceForTesting(metricsProxy);

      // Set up fake languageSettingsPrivate API.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      languageSettingsPrivate.setSettingsPrefs(prefElement);

      // Instantiate the data model with data bindings for prefs.
      const settingsLanguages = document.createElement('settings-languages');
      settingsLanguages.prefs = prefElement.prefs;
      fakeDataBind(prefElement, settingsLanguages, 'prefs');
      document.body.appendChild(settingsLanguages);

      // Create page with data bindings for prefs and data model.
      inputPage = document.createElement('os-settings-input-page');
      inputPage.prefs = prefElement.prefs;
      fakeDataBind(prefElement, inputPage, 'prefs');
      inputPage.languages = settingsLanguages.languages;
      fakeDataBind(settingsLanguages, inputPage, 'languages');
      inputPage.languageHelper = settingsLanguages.languageHelper;
      fakeDataBind(settingsLanguages, inputPage, 'language-helper');
      languageHelper = inputPage.languageHelper;
      document.body.appendChild(inputPage);
    });
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  suite('language pack notice', () => {
    test('is shown when needed', () => {
      inputPage.shouldShowLanguagePacksNotice_ = true;
      loadTimeData.overrideValues({languagePacksHandwritingEnabled: true});
      flush();

      assertTrue(isVisible(
          inputPage.shadowRoot.querySelector('#languagePacksNotice')));
    });

    test('is hidden when needed', () => {
      inputPage.shouldShowLanguagePacksNotice_ = false;
      loadTimeData.overrideValues({languagePacksHandwritingEnabled: false});
      flush();

      assertFalse(isVisible(
          inputPage.shadowRoot.querySelector('#languagePacksNotice')));
    });
  });

  suite('input method list', () => {
    test('displays correctly', () => {
      const inputMethodsList =
          inputPage.shadowRoot.querySelector('#inputMethodsList');
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
      const router = Router.getInstance();
      assertEquals(
          router.currentRoute.getAbsolutePath(),
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
      flush();

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
      flush();

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
          inputPage.shadowRoot.querySelector('#inputMethodsManagedbyPolicy');
      assertFalse(!!inputMethodsManagedbyPolicy);

      inputPage.setPrefValue(
          'settings.language.allowed_input_methods', ['xkb:us::eng']);
      flush();

      assertTrue(
          !!inputPage.shadowRoot.querySelector('#inputMethodsManagedbyPolicy'));
    });
  });

  suite('input page', () => {
    test('Deep link to spell check', async () => {
      const params = new URLSearchParams();
      params.append('settingId', '1207');
      Router.getInstance().navigateTo(routes.OS_LANGUAGES_INPUT, params);

      flush();

      const deepLinkElement =
          inputPage.shadowRoot.querySelector('#enableSpellcheckingToggle')
              .shadowRoot.querySelector('cr-toggle');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Spell check toggle should be focused for settingId=1207.');
    });
  });

  suite('add input methods dialog', () => {
    let dialog;
    let suggestedList;
    let allImesList;
    let cancelButton;
    let actionButton;

    setup(() => {
      assertFalse(!!inputPage.shadowRoot.querySelector(
          'os-settings-add-input-methods-dialog'));
      inputPage.shadowRoot.querySelector('#addInputMethod').click();
      flush();

      dialog = inputPage.shadowRoot
                   .querySelector('os-settings-add-input-methods-dialog')
                   .shadowRoot.querySelector('os-settings-add-items-dialog');
      assertTrue(!!dialog);

      actionButton = dialog.shadowRoot.querySelector('.action-button');
      assertTrue(!!actionButton);
      cancelButton = dialog.shadowRoot.querySelector('.cancel-button');
      assertTrue(!!cancelButton);

      suggestedList = dialog.shadowRoot.querySelector('#suggested-items-list');
      assertTrue(!!suggestedList);

      allImesList = dialog.shadowRoot.querySelector('#filtered-items-list');
      assertTrue(!!allImesList);

      // No input methods has been selected, so the action button is disabled.
      assertTrue(actionButton.disabled);
      assertFalse(cancelButton.disabled);
    });

    test('has action button working correctly', () => {
      const listItems = suggestedList.querySelectorAll('.list-item');
      // selecting a language enables action button
      listItems[0].click();
      assertFalse(actionButton.disabled);

      // selecting the same language again disables action button
      listItems[0].click();
      assertTrue(actionButton.disabled);
    });

    test('has correct structure and adds input methods', () => {
      const suggestedItems = suggestedList.querySelectorAll('.list-item');
      // input methods are based on and ordered by enabled languages
      // only allowed input methods are shown.
      assertEquals(2, suggestedItems.length);
      assertEquals('US Swahili keyboard', suggestedItems[0].textContent.trim());
      assertEquals('Swahili keyboard', suggestedItems[1].textContent.trim());
      // selecting Swahili keyboard.
      suggestedItems[1].click();

      const allItems = allImesList.querySelectorAll('.list-item');
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
            allItems[i].shadowRoot.querySelector('cr-checkbox').disabled,
            `expect ${expectedItems[i].name}'s checkbox disabled state to be ${
                expectedItems[i].checkboxDisabled}`);
        assertEquals(
            expectedItems[i].checkboxChecked,
            allItems[i].shadowRoot.querySelector('cr-checkbox').checked,
            `expect ${expectedItems[i].name}'s checkbox checked state to be ${
                expectedItems[i].checkboxChecked}`);
        assertEquals(
            expectedItems[i].policyIcon,
            !!allItems[i].shadowRoot.querySelector('iron-icon'),
            `expect ${expectedItems[i].name}'s policy icon presence to be ${
                expectedItems[i].policyIcon}`);
      }

      // selecting Vietnamese keyboard
      allItems[3].shadowRoot.querySelector('cr-checkbox').click();

      actionButton.click();

      assertTrue(languageHelper.isInputMethodEnabled(
          '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:sw:sw'));
      assertFalse(languageHelper.isInputMethodEnabled(
          'ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:sw'));
      assertTrue(languageHelper.isInputMethodEnabled(
          '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:vi:vi'));
    });

    test('suggested input methods hidden when no languages is enabled', () => {
      languageHelper.setPrefValue('intl.accept_languages', '');
      languageHelper.setPrefValue('settings.language.preferred_languages', '');
      flush();

      suggestedList = dialog.shadowRoot.querySelector('#suggestedInputMethods');
      // suggested input methods is rendered previously.
      assertFalse(isVisible(suggestedList));
    });

    test('suggested input methods hidden when no input methods left', () => {
      const languageCode = 'sw';
      languageHelper.setPrefValue('intl.accept_languages', languageCode);
      languageHelper.setPrefValue(
          'settings.language.preferred_languages', languageCode);
      languageHelper.getInputMethodsForLanguage(languageCode)
          .forEach(inputMethod => {
            languageHelper.addInputMethod(inputMethod.id);
          });
      flush();

      suggestedList = dialog.shadowRoot.querySelector('#suggestedInputMethods');
      assertFalse(isVisible(suggestedList));
    });

    test('searches input methods correctly', () => {
      const searchInput = dialog.shadowRoot.querySelector('cr-search-field');
      const getItems = function() {
        return allImesList.querySelectorAll('.list-item:not([hidden])');
      };

      assertTrue(
          isVisible(dialog.shadowRoot.querySelector('#filtered-items-label')));
      assertTrue(isVisible(suggestedList));

      // Expecting a few languages to be displayed when no query exists.
      assertGE(getItems().length, 1);

      // Search hides the suggested list and the label for all IMEs.
      searchInput.setValue('v');
      flush();
      assertFalse(
          isVisible(dialog.shadowRoot.querySelector('#filtered-items-label')));
      assertFalse(isVisible(suggestedList));

      // Search input methods name
      searchInput.setValue('vietnamese');
      flush();
      assertEquals(1, getItems().length);
      assertTrue(getItems()[0].textContent.includes('Vietnamese'));

      // Search input methods' language
      searchInput.setValue('Turkmen');
      flush();
      assertEquals(1, getItems().length);
      assertTrue(getItems()[0].textContent.includes('Swahili keyboard'));
    });

    test('has escape key behavior working correctly', function() {
      const searchInput = dialog.shadowRoot.querySelector('cr-search-field');
      searchInput.setValue('dummyquery');

      // Test that dialog is not closed if 'Escape' is pressed on the input
      // and a search query exists.
      keyDownOn(searchInput, 19, [], 'Escape');
      assertTrue(dialog.$.dialog.open);

      // Test that dialog is closed if 'Escape' is pressed on the input and no
      // search query exists.
      searchInput.setValue('');
      keyDownOn(searchInput, 19, [], 'Escape');
      assertFalse(dialog.$.dialog.open);
    });
  });

  suite('records metrics', () => {
    test('when deactivating show ime menu', async () => {
      inputPage.setPrefValue('settings.language.ime_menu_activated', true);
      inputPage.shadowRoot.querySelector('#showImeMenu').click();
      flush();

      assertFalse(
          await metricsProxy.whenCalled('recordToggleShowInputOptionsOnShelf'));
    });

    test('when activating show ime menu', async () => {
      inputPage.setPrefValue('settings.language.ime_menu_activated', false);
      inputPage.shadowRoot.querySelector('#showImeMenu').click();
      flush();

      assertTrue(
          await metricsProxy.whenCalled('recordToggleShowInputOptionsOnShelf'));
    });

    test('when adding input methods', async () => {
      inputPage.shadowRoot.querySelector('#addInputMethod').click();
      flush();

      await metricsProxy.whenCalled('recordAddInputMethod');
    });

    test('when switch input method', async () => {
      const inputMethodsList =
          inputPage.shadowRoot.querySelector('#inputMethodsList');
      assertTrue(!!inputMethodsList);

      // The test input methods should appear.
      const items = inputMethodsList.querySelectorAll('.list-item');
      items[0].click();
      assertEquals(
          LanguagesPageInteraction.SWITCH_INPUT_METHOD,
          await metricsProxy.whenCalled('recordInteraction'));
    });

    test('when dismissing shortcut reminder', async () => {
      // Enable Update 2.
      inputPage.languageSettingsV2Update2Enabled_ = true;
      loadTimeData.overrideValues({enableLanguageSettingsV2Update2: true});
      flush();

      // Default shortcut reminder with two elements should show "last used IME"
      // reminder.
      inputPage.shadowRoot.querySelector('keyboard-shortcut-banner')
          .$.dismiss.click();
      assertEquals(
          InputsShortcutReminderState.LAST_USED_IME,
          await metricsProxy.whenCalled('recordShortcutReminderDismissed'));
      metricsProxy.resetResolver('recordShortcutReminderDismissed');

      // Add US Swahili keyboard, a third party IME.
      languageHelper.addInputMethod(
          'ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:sw');
      flush();

      // Shortcut reminder should show "next IME" shortcut.
      inputPage.shadowRoot.querySelector('keyboard-shortcut-banner')
          .$.dismiss.click();
      assertEquals(
          InputsShortcutReminderState.NEXT_IME,
          await metricsProxy.whenCalled('recordShortcutReminderDismissed'));
      metricsProxy.resetResolver('recordShortcutReminderDismissed');

      // Reset shortcut reminder dismissals to display both shortcuts.
      inputPage.setPrefValue(
          'ash.shortcut_reminders.last_used_ime_dismissed', false);
      inputPage.setPrefValue(
          'ash.shortcut_reminders.next_ime_dismissed', false);
      flush();

      // Shortcut reminder should show both shortcuts.
      inputPage.shadowRoot.querySelector('keyboard-shortcut-banner')
          .$.dismiss.click();
      assertEquals(
          InputsShortcutReminderState.LAST_USED_IME_AND_NEXT_IME,
          await metricsProxy.whenCalled('recordShortcutReminderDismissed'));
    });

    test('when clicking on "learn more" about language packs', async () => {
      inputPage.shouldShowLanguagePacksNotice_ = true;
      loadTimeData.overrideValues({languagePacksHandwritingEnabled: true});
      flush();

      const anchor = inputPage.shadowRoot.querySelector('#languagePacksNotice')
                         .shadowRoot.querySelector('a');
      // The below would normally create a new window, which would change the
      // focus from this test to the new window.
      // Prevent this from happening by adding an event listener on the anchor
      // element which stops the default behaviour (of opening a new window).
      anchor.addEventListener('click', (e) => e.preventDefault());
      anchor.click();
      assertEquals(
          await metricsProxy.whenCalled('recordInteraction'),
          LanguagesPageInteraction.OPEN_LANGUAGE_PACKS_LEARN_MORE);
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
      // However, we should still set loadTimeData as some other code may use
      // it (such as languages.js).
      loadTimeData.overrideValues({
        enableLanguageSettingsV2Update2: false,
        onDeviceGrammarCheckEnabled: false,
      });

      flush();
      // spell check is initially on
      spellCheckToggle =
          inputPage.shadowRoot.querySelector('#enableSpellcheckingToggle');
      assertTrue(!!spellCheckToggle);
      assertTrue(spellCheckToggle.checked);

      spellCheckListContainer =
          inputPage.shadowRoot.querySelector('#spellCheckLanguagesList');
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

      // The spell check toggle should be off now.
      assertFalse(spellCheckToggle.checked);
      spellCheckToggle.click();
      assertTrue(spellCheckToggle.checked);

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
          flush();

          assertFalse(spellCheckLanguageToggle.checked);
          assertDeepEquals(
              [], languageHelper.prefs.spellcheck.dictionaries.value);

          // The spell check toggle should be off now.
          assertFalse(spellCheckToggle.checked);
          spellCheckToggle.click();
          assertTrue(spellCheckToggle.checked);

          // toggle on by clicking name
          spellCheckList[0].querySelector('.name-with-error').click();
          flush();

          assertTrue(spellCheckLanguageToggle.checked);
          assertDeepEquals(
              ['en-US'], languageHelper.prefs.spellcheck.dictionaries.value);
        });

    test('shows force-on existing spell check language', () => {
      // Force-enable an existing language via policy.
      languageHelper.setPrefValue('spellcheck.forced_dictionaries', ['sw']);
      flush();

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
      flush();

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
      flush();

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
      flush();
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
          flush();
          const newSpellCheckList =
              spellCheckListContainer.querySelectorAll('.list-item');
          assertEquals(2, newSpellCheckList.length);
        });

    test('shows force-off spell check when language is enabled', () => {
      // Force-disable a language via policy.
      languageHelper.setPrefValue('spellcheck.blocked_dictionaries', ['nb']);
      languageHelper.enableLanguage('nb');
      flush();

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
      flush();
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

      flush();
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

      flush();
      assertTrue(errorDivs[0].hidden);
      assertTrue(retryButtons[0].hidden);
    });

    test('toggle off disables enhanced spell check', () => {
      const enhancedSpellCheckToggle =
          inputPage.shadowRoot.querySelector('#enhancedSpellCheckToggle');
      assertFalse(enhancedSpellCheckToggle.disabled);
      spellCheckToggle.click();

      assertTrue(enhancedSpellCheckToggle.disabled);
    });

    test('toggle off disables edit dictionary', () => {
      const editDictionarySubpageTrigger =
          inputPage.shadowRoot.querySelector('#editDictionarySubpageTrigger');
      assertFalse(editDictionarySubpageTrigger.disabled);
      spellCheckToggle.click();

      assertTrue(editDictionarySubpageTrigger.disabled);
    });

    test('opens edit dictionary page', () => {
      const editDictionarySubpageTrigger =
          inputPage.shadowRoot.querySelector('#editDictionarySubpageTrigger');
      editDictionarySubpageTrigger.click();
      const router = Router.getInstance();
      assertEquals(
          router.currentRoute.getAbsolutePath(),
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
      // However, we should still set loadTimeData as some other code may use
      // it (such as languages.js).
      loadTimeData.overrideValues({
        enableLanguageSettingsV2Update2: true,
        onDeviceGrammarCheckEnabled: true,
      });
      flush();

      // Spell check is initially on.
      spellCheckToggle =
          inputPage.shadowRoot.querySelector('#enableSpellcheckingToggle');
      assertTrue(!!spellCheckToggle);
      assertTrue(spellCheckToggle.checked);

      spellCheckListContainer =
          inputPage.shadowRoot.querySelector('#spellCheckLanguagesListV2');
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
      flush();

      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      // The spell check list should just have "add languages".
      assertEquals(0 + 1, newSpellCheckList.length);

      assertDeepEquals([], languageHelper.prefs.spellcheck.dictionaries.value);
    });

    test('can remove non-enabled language from spell check list', () => {
      // Add a new non-enabled language to spellcheck.dictionaries.
      languageHelper.setPrefValue('spellcheck.dictionaries', ['en-US', 'nb']);
      flush();

      let newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should have en-US, nb and "add languages".
      assertEquals(2 + 1, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));
      assertTrue(newSpellCheckList[1].textContent.includes('Norwegian Bokmål'));

      // Remove nb.
      newSpellCheckList[1].querySelector('cr-icon-button').click();
      flush();
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
      flush();

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
      flush();

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
      flush();

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
      flush();

      // The spell check list should just have "add languages".
      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(0 + 1, newSpellCheckList.length);
    });

    test('does not show force-off enabled spell check language', () => {
      // Force-disable an enabled language via policy.
      languageHelper.setPrefValue('spellcheck.blocked_dictionaries', ['sw']);
      flush();

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
      flush();

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
      flush();
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

    test('removing all languages, then adding enabled language works', () => {
      // See https://crbug.com/1197386 for more information.
      // Remove en-US so there are no spell check languages.
      const spellCheckLanguageToggle =
          spellCheckList[0].querySelector('cr-icon-button');
      spellCheckLanguageToggle.click();
      flush();

      let newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should just have "add languages".
      assertEquals(0 + 1, newSpellCheckList.length);
      // The "enable spellchecking" toggle should be off as well.
      assertFalse(spellCheckToggle.checked);

      // Enable spell checking again.
      spellCheckToggle.click();
      newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      // The spell check list shouldn't have changed...
      assertEquals(0 + 1, newSpellCheckList.length);
      // ...but the "enable spellchecking" toggle should be checked.
      assertTrue(spellCheckToggle.checked);

      // Add an enabled language (en-US).
      languageHelper.toggleSpellCheck('en-US', true);
      flush();

      newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      // The spell check list should now have en-US.
      assertEquals(1 + 1, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));
      // Spell check should still be enabled.
      assertTrue(spellCheckToggle.checked);
    });

    test('changing Accept-Language does not change spellcheck', () => {
      // Remove en-US from Accept-Language, which is also an enabled spell check
      // language.
      languageHelper.disableLanguage('en-US');
      flush();

      // en-US should still be there.
      let newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(1 + 1, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));

      // Add a spell check language not in Accept-Language.
      languageHelper.toggleSpellCheck('nb', true);
      flush();

      // The spell check list should now have en-US, nb and "add languages".
      newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(2 + 1, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));
      assertTrue(newSpellCheckList[1].textContent.includes('Norwegian Bokmål'));

      // Add an arbitrary language to Accept-Language.
      languageHelper.enableLanguage('tk');
      flush();

      // The spell check list should remain the same.
      newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(2 + 1, newSpellCheckList.length);
      assertTrue(
          newSpellCheckList[0].textContent.includes('English (United States)'));
      assertTrue(newSpellCheckList[1].textContent.includes('Norwegian Bokmål'));
    });

    // TODO(crbug.com/1201540): Add test to ensure that it is impossible to
    //     enable spell check without a spell check language added (i.e. the
    //     "add spell check languages" dialog appears when turning it on).

    // TODO(crbug.com/1201540): Add a test for the "automatically determining
    //     spell check language" behaviour when the user has no spell check
    //     languages.

    // TODO(crbug.com/1201540): Add a test for the shortcut reminder.

    test('error handling', () => {
      // Enable Swahili so we have two languages for testing.
      languageHelper.setPrefValue('spellcheck.dictionaries', ['en-US', 'sw']);
      flush();
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

      flush();
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

      flush();
      assertTrue(errorDivs[0].hidden);
      assertTrue(retryButtons[0].hidden);
    });

    test('toggle off disables edit dictionary', () => {
      const editDictionarySubpageTrigger =
          inputPage.shadowRoot.querySelector('#editDictionarySubpageTrigger');
      assertFalse(editDictionarySubpageTrigger.disabled);
      spellCheckToggle.click();

      assertTrue(editDictionarySubpageTrigger.disabled);
    });

    test('opens edit dictionary page', () => {
      const editDictionarySubpageTrigger =
          inputPage.shadowRoot.querySelector('#editDictionarySubpageTrigger');
      editDictionarySubpageTrigger.click();
      const router = Router.getInstance();
      assertEquals(
          router.currentRoute.getAbsolutePath(),
          'chrome://os-settings/osLanguages/editDictionary');
    });
  });

  suite('add spell check languages dialog', () => {
    let dialog;
    let suggestedList;
    let allLangsList;
    let cancelButton;
    let actionButton;

    /**
     * Returns the list items in the dialog.
     * @return {!Array<!Element>}
     */
    function getAllLanguagesCheckboxWithPolicies() {
      // If an element (the <iron-list> in this case) is hidden in Polymer,
      // Polymer will intelligently not update the DOM of the hidden element
      // to prevent DOM updates that the user can't see. However, this means
      // that when the <iron-list> is hidden (due to no results), the list
      // items still exist in the DOM.
      // This function should return the *visible* items that the user can
      // select, so if the <iron-list> is hidden we should return an empty
      // list instead.
      if (!isVisible(allLangsList)) {
        return [];
      }
      return [
        ...allLangsList.querySelectorAll('cr-checkbox-with-policy'),
      ].filter(checkbox => isVisible(checkbox));
    }

    /**
     * Returns the internal cr-checkboxes in allLanguages.
     * @return {!Array<!Element>}
     */
    function getAllLanguagesCheckboxes() {
      const checkboxWithPolicies = getAllLanguagesCheckboxWithPolicies();
      return checkboxWithPolicies.map(
          checkboxWithPolicy => checkboxWithPolicy.$.checkbox);
    }

    setup(() => {
      // Enable Update 2.
      // We use the property directly instead of loadTimeData, as overriding
      // loadTimeData does not work as the property is set using a value().
      inputPage.languageSettingsV2Update2Enabled_ = true;
      // However, we should still set loadTimeData as some other code may use
      // it (such as languages.js).
      loadTimeData.overrideValues({enableLanguageSettingsV2Update2: true});
      flush();

      assertFalse(!!inputPage.shadowRoot.querySelector(
          'os-settings-add-spellcheck-languages-dialog'));
      inputPage.shadowRoot.querySelector('#addSpellcheckLanguages').click();
      flush();

      dialog = inputPage.shadowRoot
                   .querySelector('os-settings-add-spellcheck-languages-dialog')
                   .shadowRoot.querySelector('os-settings-add-items-dialog');
      assertTrue(!!dialog);
      assertTrue(dialog.$.dialog.open);

      suggestedList = dialog.shadowRoot.querySelector('#suggested-items-list');
      assertTrue(!!suggestedList);
      allLangsList = dialog.shadowRoot.querySelector('#filtered-items-list');
      assertTrue(!!allLangsList);

      actionButton = dialog.shadowRoot.querySelector('.action-button');
      assertTrue(!!actionButton);
      cancelButton = dialog.shadowRoot.querySelector('.cancel-button');
      assertTrue(!!cancelButton);
    });

    test('action button is enabled and disabled when necessary', () => {
      // Mimic $$, but with a querySelectorAll instead of querySelector.
      const checkboxes = getAllLanguagesCheckboxes();
      assertTrue(checkboxes.length > 0);

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
      // As Swahili is an enabled language, it should be shown as a suggested
      // language.
      const suggestedItems = suggestedList.querySelectorAll('cr-checkbox');
      assertEquals(suggestedItems.length, 1);
      assertTrue(suggestedItems[0].textContent.includes('Swahili'));

      // There are four languages with spell check enabled in
      // fake_language_settings_private.js: en-US, en-CA, sw, nb.
      // en-US shouldn't be displayed as it is already enabled.
      const allItems = getAllLanguagesCheckboxWithPolicies();
      assertEquals(allItems.length, 3);
      assertTrue(allItems[0].textContent.includes('English (Canada)'));
      assertTrue(allItems[1].textContent.includes('Swahili'));
      assertTrue(allItems[2].textContent.includes('Norwegian Bokmål'));

      // By default, all checkboxes should not be disabled, and should not be
      // checked.
      const checkboxes = [...suggestedItems, ...getAllLanguagesCheckboxes()];
      assertTrue(checkboxes.every(checkbox => !checkbox.disabled));
      assertTrue(checkboxes.every(checkbox => !checkbox.checked));

      // There should be a label for both sections.
      const suggestedLabel =
          dialog.shadowRoot.querySelector('#suggested-items-label');
      assertTrue(!!suggestedLabel);
      assertTrue(isVisible(suggestedLabel));

      const allLangsLabel =
          dialog.shadowRoot.querySelector('#filtered-items-label');
      assertTrue(!!allLangsLabel);
      assertTrue(isVisible(allLangsLabel));
    });

    test('can add single language and uncheck language', () => {
      const checkboxes = getAllLanguagesCheckboxes();
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
      const checkboxes = getAllLanguagesCheckboxes();

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
      flush();

      const swCheckboxWithPolicy = getAllLanguagesCheckboxWithPolicies()[1];
      const swCheckbox =
          swCheckboxWithPolicy.shadowRoot.querySelector('cr-checkbox');
      const swPolicyIcon =
          swCheckboxWithPolicy.shadowRoot.querySelector('iron-icon');

      assertTrue(swCheckbox.disabled);
      assertFalse(swCheckbox.checked);
      assertTrue(!!swPolicyIcon);
    });

    test('labels do not appear if there are no suggested languages', () => {
      // Disable sw, the only default suggested language, as a web language.
      languageHelper.disableLanguage('sw');
      flush();

      // Suggested languages should not show up whatsoever.
      assertFalse(isVisible(suggestedList));
      // The label for all languages should not appear either.
      assertFalse(isVisible(allLangsList.querySelector('.label')));
    });

    test('input method languages appear as suggested languages', () => {
      // Remove en-US from the dictionary list AND the enabled languages list.
      languageHelper.setPrefValue('spellcheck.dictionaries', []);
      languageHelper.disableLanguage('en-US');
      flush();

      // Both Swahili (as it is an enabled language) and English (US) (as it is
      // enabled as an input method) should appear in the list.
      const suggestedListItems = suggestedList.querySelectorAll('.list-item');
      assertEquals(suggestedListItems.length, 2);
      assertTrue(suggestedListItems[0].textContent.includes(
          'English (United States)'));
      assertTrue(suggestedListItems[1].textContent.includes('Swahili'));

      // en-US should also appear in the all languages list now.
      assertEquals(allLangsList.querySelectorAll('.list-item').length, 4);
    });

    test('searches languages on display name', () => {
      const searchInput = dialog.shadowRoot.querySelector('cr-search-field');

      // Expecting a few languages to be displayed when no query exists.
      assertGE(getAllLanguagesCheckboxWithPolicies().length, 1);

      // Issue query that matches the |displayedName| in lowercase.
      searchInput.setValue('norwegian');
      flush();
      assertEquals(getAllLanguagesCheckboxWithPolicies().length, 1);
      assertTrue(getAllLanguagesCheckboxWithPolicies()[0].textContent.includes(
          'Norwegian Bokmål'));

      // Issue query that matches the |nativeDisplayedName|.
      searchInput.setValue('norsk');
      flush();
      assertEquals(getAllLanguagesCheckboxWithPolicies().length, 1);

      // Issue query that does not match any language.
      searchInput.setValue('egaugnal');
      flush();
      assertEquals(getAllLanguagesCheckboxWithPolicies().length, 0);
      assertTrue(
          isVisible(dialog.shadowRoot.querySelector('#no-search-results')));
    });

    test('has escape key behavior working correctly', function() {
      const searchInput = dialog.shadowRoot.querySelector('cr-search-field');
      searchInput.setValue('dummyquery');

      // Test that dialog is not closed if 'Escape' is pressed on the input
      // and a search query exists.
      keyDownOn(searchInput, 19, [], 'Escape');
      assertTrue(dialog.$.dialog.open);

      // Test that dialog is closed if 'Escape' is pressed on the input and no
      // search query exists.
      searchInput.setValue('');
      keyDownOn(searchInput, 19, [], 'Escape');
      assertFalse(dialog.$.dialog.open);
    });
  });
});
