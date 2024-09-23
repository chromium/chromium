// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CrCheckboxWithPolicyElement, InputsShortcutReminderState, LanguageHelper, LanguagesBrowserProxyImpl, LanguagesMetricsProxyImpl, LanguagesPageInteraction, OsSettingsAddItemsDialogElement, OsSettingsInputPageElement, SettingsLanguagesElement} from 'chrome://os-settings/lazy_load.js';
import {AcceleratorAction, CrCheckboxElement, CrSettingsPrefs, IronListElement, Router, routes, settingMojom, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {StandardAcceleratorProperties} from 'chrome://resources/ash/common/shortcut_input_ui/accelerator_info.mojom-webui.js';
import {VKey} from 'chrome://resources/ash/common/shortcut_input_ui/accelerator_keys.mojom-webui.js';
import {FakeAcceleratorFetcher} from 'chrome://resources/ash/common/shortcut_input_ui/fake_accelerator_fetcher.js';
import {Modifier} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {AcceleratorKeyState} from 'chrome://resources/mojo/ui/base/accelerators/mojom/accelerator.mojom-webui.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGE, assertGT, assertNotEquals, assertNull, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {fakeDataBind, flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {FakeLanguageSettingsPrivate, getFakeLanguagePrefs} from '../fake_language_settings_private.js';
import {clearBody} from '../utils.js';

import {TestLanguagesBrowserProxy} from './test_os_languages_browser_proxy.js';
import {TestLanguagesMetricsProxy} from './test_os_languages_metrics_proxy.js';

suite('<os-settings-input-page>', () => {
  let inputPage: OsSettingsInputPageElement;
  let metricsProxy: TestLanguagesMetricsProxy;
  let browserProxy: TestLanguagesBrowserProxy;
  let languageHelper: LanguageHelper;
  let settingsLanguages: SettingsLanguagesElement;

  async function createInputPage(): Promise<void> {
    const prefElement: SettingsPrefsElement =
        document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());

    /**
     * Prefs listener to emulate SpellcheckService listeners.
     * As we use a mocked prefs object in tests, we also need to mock the
     * behavior of SpellcheckService as it relies on a C++ PrefChangeRegistrar
     * to listen to pref changes - which do not work when the prefs are mocked.
     */
    async function spellCheckServiceListener(
        prefs: chrome.settingsPrivate.PrefObject[]): Promise<void> {
      for (const pref of prefs) {
        switch (pref.key) {
          case 'spellcheck.dictionaries':
            // Emulate SpellcheckService::OnSpellCheckDictionariesChanged:
            // If there are no dictionaries, set browser.enable_spellchecking
            // to false.
            if (pref.value.length === 0) {
              settingsPrivate.setPref(
                  'browser.enable_spellchecking', false, '');
            }
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
    prefElement.initialize(settingsPrivate);
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;

    // Set up fake languageSettingsPrivate API.
    const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate() as
        FakeLanguageSettingsPrivate;
    languageSettingsPrivate.setSettingsPrefsForTesting(prefElement);

    // Instantiate the data model with data bindings for prefs.
    settingsLanguages = document.createElement('settings-languages');
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
    await flushTasks();
  }

  suiteSetup(() => {
    // Set up test browser proxy.
    browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstanceForTesting(browserProxy);

    // Sets up test metrics proxy.
    metricsProxy = new TestLanguagesMetricsProxy();
    LanguagesMetricsProxyImpl.setInstanceForTesting(metricsProxy);

    CrSettingsPrefs.deferInitialization = true;
  });

  setup(() => {
    clearBody();
    loadTimeData.overrideValues({
      allowEmojiSuggestion: true,
    });
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_INPUT);
  });

  teardown(() => {
    inputPage.remove();
    settingsLanguages.remove();
    Router.getInstance().resetRouteForTesting();
    browserProxy.reset();
    metricsProxy.reset();
  });

  suite('language pack notice', () => {
    test('is shown', async () => {
      await createInputPage();

      assertTrue(isVisible(
          inputPage.shadowRoot!.querySelector('#languagePacksNotice')));
    });
  });

  suite('input method list', () => {
    setup(async () => {
      await createInputPage();
    });

    test('displays correctly', () => {
      const inputMethodsList =
          inputPage.shadowRoot!.querySelector('#inputMethodsList');
      assertTrue(!!inputMethodsList);

      // The test input methods should appear.
      const items = inputMethodsList.querySelectorAll('.list-item');
      // Two items for input methods and one item for add input methods.
      assertEquals(3, items.length);
      let name = items[0]!.querySelector('.display-name');
      assertTrue(!!name);
      assertEquals('US keyboard', name.textContent?.trim());
      assertTrue(!!items[0]!.querySelector('.internal-wrapper'));
      assertNull(items[0]!.querySelector('.external-wrapper'));
      let icon = items[0]!.querySelector<HTMLButtonElement>('.icon-clear');
      assertTrue(!!icon);
      assertFalse(icon.disabled);
      name = items[1]!.querySelector('.display-name');
      assertTrue(!!name);
      assertEquals('US Dvorak keyboard', name.textContent?.trim());
      assertTrue(!!items[1]!.querySelector('.external-wrapper'));
      assertNull(items[1]!.querySelector('.internal-wrapper'));
      icon = items[1]!.querySelector<HTMLButtonElement>('.icon-clear');
      assertTrue(!!icon);
      assertFalse(icon.disabled);
    });

    test('navigates to input method options page', () => {
      const inputMethodsList =
          inputPage.shadowRoot!.querySelector('#inputMethodsList');
      assertTrue(!!inputMethodsList);
      const items = inputMethodsList.querySelectorAll('.list-item');
      const button =
          items[0]!.querySelector<HTMLButtonElement>('.subpage-arrow');
      assertTrue(!!button);
      button.click();
      const router = Router.getInstance();
      assertEquals(
          'chrome://os-settings/osLanguages/inputMethodOptions',
          router.currentRoute.getAbsolutePath());
      assertEquals(
          '_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:us::eng',
          router.getQueryParameters().get('id'));
    });

    test('removes an input method', () => {
      const inputMethodName = 'US keyboard';

      let inputMethodsList =
          inputPage.shadowRoot!.querySelector('#inputMethodsList');
      assertTrue(!!inputMethodsList);
      let items = inputMethodsList.querySelectorAll('.list-item');
      assertEquals(3, items.length);
      let name = items[0]!.querySelector('.display-name');
      assertTrue(!!name);
      assertEquals(inputMethodName, name.textContent?.trim());

      // clicks remove input method button.
      const icon = items[0]!.querySelector<HTMLButtonElement>('.icon-clear');
      assertTrue(!!icon);
      icon.click();
      flush();

      inputMethodsList =
          inputPage.shadowRoot!.querySelector('#inputMethodsList');
      assertTrue(!!inputMethodsList);
      items = inputMethodsList.querySelectorAll('.list-item');
      assertEquals(2, items.length);
      name = items[0]!.querySelector('.display-name');
      assertTrue(!!name);
      assertNotEquals(inputMethodName, name.textContent?.trim());
    });

    test('disables remove input method option', () => {
      // Add US Swahili keyboard, a third party IME
      languageHelper.addInputMethod(
          'ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:sw');
      // Remove US Dvorak keyboard, so there is only 1 component IME left.
      languageHelper.removeInputMethod(
          '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us:dvorak:eng');
      flush();

      const inputMethodsList =
          inputPage.shadowRoot!.querySelector('#inputMethodsList');
      assertTrue(!!inputMethodsList);
      const items = inputMethodsList.querySelectorAll('.list-item');
      assertEquals(3, items.length);
      let name = items[0]!.querySelector('.display-name');
      assertTrue(!!name);
      assertEquals('US keyboard', name.textContent?.trim());
      let icon = items[0]!.querySelector<HTMLButtonElement>('.icon-clear');
      assertTrue(!!icon);
      assertTrue(icon.disabled);
      name = items[1]!.querySelector('.display-name');
      assertTrue(!!name);
      assertEquals('US Swahili keyboard', name.textContent?.trim());
      icon = items[1]!.querySelector<HTMLButtonElement>('.icon-clear');
      assertTrue(!!icon);
      assertFalse(icon.disabled);
    });

    test('shows managed input methods label', () => {
      const inputMethodsManagedbyPolicy =
          inputPage.shadowRoot!.querySelector('#inputMethodsManagedbyPolicy');
      assertNull(inputMethodsManagedbyPolicy);

      inputPage.setPrefValue(
          'settings.language.allowed_input_methods', ['xkb:us::eng']);
      flush();

      assertTrue(!!inputPage.shadowRoot!.querySelector(
          '#inputMethodsManagedbyPolicy'));
    });
  });

  suite('input page', () => {
    test('Deep link to spell check', async () => {
      await createInputPage();

      const setting = settingMojom.Setting.kSpellCheckOnOff;
      const params = new URLSearchParams();
      params.append('settingId', setting.toString());
      Router.getInstance().navigateTo(routes.OS_LANGUAGES_INPUT, params);
      flush();

      const enableSpellcheckingToggle =
          inputPage.shadowRoot!.querySelector('#enableSpellcheckingToggle');
      assertTrue(!!enableSpellcheckingToggle);
      const deepLinkElement =
          enableSpellcheckingToggle.shadowRoot!.querySelector('cr-toggle');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Spell check toggle should be focused for settingId=${setting}.`);
    });

    test('Spellcheck row is focused after returning from subpage', async () => {
      await createInputPage();

      const triggerSelector = '#editDictionarySubpageTrigger';
      const subpageTrigger =
          inputPage.shadowRoot!.querySelector<HTMLElement>(triggerSelector);
      assertTrue(!!subpageTrigger);

      // Sub-page trigger navigates to spellcheck subpage
      subpageTrigger.click();
      assertEquals(
          routes.OS_LANGUAGES_EDIT_DICTIONARY,
          Router.getInstance().currentRoute);

      // Navigate back
      const popStateEventPromise = eventToPromise('popstate', window);
      Router.getInstance().navigateToPreviousRoute();
      await popStateEventPromise;
      await waitAfterNextRender(inputPage);

      assertEquals(
          subpageTrigger, inputPage.shadowRoot!.activeElement,
          `${triggerSelector} should be focused.`);
    });
  });

  suite('add input methods dialog', () => {
    let dialog: OsSettingsAddItemsDialogElement;
    let suggestedList: IronListElement;
    let allImesList: IronListElement;
    let cancelButton: HTMLButtonElement;
    let actionButton: HTMLButtonElement;

    setup(async () => {
      await createInputPage();

      let element = inputPage.shadowRoot!.querySelector(
          'os-settings-add-input-methods-dialog');
      assertNull(element);
      const addInputMethod =
          inputPage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#addInputMethod');
      assertTrue(!!addInputMethod);
      addInputMethod.click();
      flush();

      element = inputPage.shadowRoot!.querySelector(
          'os-settings-add-input-methods-dialog');
      assertTrue(!!element);
      const dialogElement =
          element.shadowRoot!.querySelector('os-settings-add-items-dialog');
      assertTrue(!!dialogElement);
      dialog = dialogElement;

      const button =
          dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
      assertTrue(!!button);
      actionButton = button;
      const cancel =
          dialog.shadowRoot!.querySelector<HTMLButtonElement>('.cancel-button');
      assertTrue(!!cancel);
      cancelButton = cancel;

      const list = dialog.shadowRoot!.querySelector<IronListElement>(
          '#suggested-items-list');
      assertTrue(!!list);
      suggestedList = list;

      const allList = dialog.shadowRoot!.querySelector<IronListElement>(
          '#filtered-items-list');
      assertTrue(!!allList);
      allImesList = allList;

      // No input methods has been selected, so the action button is disabled.
      assertTrue(actionButton.disabled);
      assertFalse(cancelButton.disabled);
    });

    test('has action button working correctly', () => {
      const listItems =
          suggestedList.querySelectorAll<HTMLButtonElement>('.list-item');
      assertTrue(!!listItems);
      // selecting a language enables action button
      listItems[0]!.click();
      assertFalse(actionButton.disabled);

      // selecting the same language again disables action button
      listItems[0]!.click();
      assertTrue(actionButton.disabled);
    });

    test('has correct structure and adds input methods', () => {
      const suggestedItems =
          suggestedList.querySelectorAll<HTMLElement>('.list-item');
      assertTrue(!!suggestedItems);
      // input methods are based on and ordered by enabled languages
      // only allowed input methods are shown.
      assertEquals(2, suggestedItems.length);
      assertEquals(
          'US Swahili keyboard', suggestedItems[0]!.textContent?.trim());
      assertEquals('Swahili keyboard', suggestedItems[1]!.textContent?.trim());
      // selecting Swahili keyboard.
      suggestedItems[1]!.click();

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
        const checkbox = allItems[i]!.shadowRoot!.querySelector('cr-checkbox');
        assertTrue(!!checkbox);
        assertStringContains(allItems[i]!.textContent!, expectedItems[i]!.name);
        assertEquals(
            expectedItems[i]!.checkboxDisabled, checkbox.disabled,
            `expect ${expectedItems[i]!.name}'s checkbox disabled state to be ${
                expectedItems[i]!.checkboxDisabled}`);
        assertEquals(
            expectedItems[i]!.checkboxChecked, checkbox.checked,
            `expect ${expectedItems[i]!.name}'s checkbox checked state to be ${
                expectedItems[i]!.checkboxChecked}`);
        assertEquals(
            expectedItems[i]!.policyIcon,
            !!allItems[i]!.shadowRoot!.querySelector('iron-icon'),
            `expect ${expectedItems[i]!.name}'s policy icon presence to be ${
                expectedItems[i]!.policyIcon}`);
      }

      // selecting Vietnamese keyboard
      const checkbox = allItems[3]!.shadowRoot!.querySelector('cr-checkbox');
      assertTrue(!!checkbox);
      checkbox.click();

      actionButton.click();

      assertTrue(languageHelper.isInputMethodEnabled(
          '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:sw:sw'));
      assertFalse(languageHelper.isInputMethodEnabled(
          'ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:sw'));
      assertTrue(languageHelper.isInputMethodEnabled(
          '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:vi:vi'));
    });

    test('suggested input methods hidden when no languages is enabled', () => {
      inputPage.setPrefValue('intl.accept_languages', '');
      inputPage.setPrefValue('settings.language.preferred_languages', '');
      flush();

      const suggestedMethods =
          dialog.shadowRoot!.querySelector('#suggestedInputMethods');
      // suggested input methods is rendered previously.
      assertFalse(isVisible(suggestedMethods));
    });

    test('suggested input methods hidden when no input methods left', () => {
      const languageCode = 'sw';
      inputPage.setPrefValue('intl.accept_languages', languageCode);
      inputPage.setPrefValue(
          'settings.language.preferred_languages', languageCode);
      languageHelper.getInputMethodsForLanguage(languageCode)
          .forEach(
              (inputMethod: chrome.languageSettingsPrivate.InputMethod) => {
                languageHelper.addInputMethod(inputMethod.id);
              });
      flush();

      const suggestedMethods =
          dialog.shadowRoot!.querySelector('#suggestedInputMethods');
      assertFalse(isVisible(suggestedMethods));
    });

    test('searches input methods correctly', () => {
      const searchInput = dialog.shadowRoot!.querySelector('cr-search-field');
      assertTrue(!!searchInput);
      const getItems = () => {
        return allImesList.querySelectorAll('.list-item:not([hidden])');
      };

      assertTrue(
          isVisible(dialog.shadowRoot!.querySelector('#filtered-items-label')));
      assertTrue(isVisible(suggestedList));

      // Expecting a few languages to be displayed when no query exists.
      assertGE(getItems().length, 1);

      // Search hides the suggested list and the label for all IMEs.
      searchInput.setValue('v');
      flush();
      assertFalse(
          isVisible(dialog.shadowRoot!.querySelector('#filtered-items-label')));
      assertFalse(isVisible(suggestedList));

      // Search input methods name
      searchInput.setValue('vietnamese');
      flush();
      assertEquals(1, getItems().length);
      assertStringContains(getItems()[0]!.textContent!, 'Vietnamese');

      // Search input methods' language
      searchInput.setValue('Turkmen');
      flush();
      assertEquals(1, getItems().length);
      assertStringContains(getItems()[0]!.textContent!, 'Swahili keyboard');
    });

    test('has escape key behavior working correctly', () => {
      const searchInput = dialog.shadowRoot!.querySelector('cr-search-field');
      assertTrue(!!searchInput);
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
    setup(async () => {
      await createInputPage();
    });

    test('when deactivating show ime menu', async () => {
      inputPage.setPrefValue('settings.language.ime_menu_activated', true);
      const showImeMenu =
          inputPage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#showImeMenu');
      assertTrue(!!showImeMenu);
      showImeMenu.click();
      flush();

      assertFalse(
          await metricsProxy.whenCalled('recordToggleShowInputOptionsOnShelf'));
    });

    test('when activating show ime menu', async () => {
      inputPage.setPrefValue('settings.language.ime_menu_activated', false);
      const showImeMenu =
          inputPage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#showImeMenu');
      assertTrue(!!showImeMenu);
      showImeMenu.click();
      flush();

      assertTrue(
          await metricsProxy.whenCalled('recordToggleShowInputOptionsOnShelf'));
    });

    test('when adding input methods', async () => {
      const addInputMethod =
          inputPage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#addInputMethod');
      assertTrue(!!addInputMethod);
      addInputMethod.click();
      flush();

      await metricsProxy.whenCalled('recordAddInputMethod');
    });

    test('when switch input method', async () => {
      const inputMethodsList =
          inputPage.shadowRoot!.querySelector('#inputMethodsList');
      assertTrue(!!inputMethodsList);

      // The test input methods should appear.
      const items =
          inputMethodsList.querySelectorAll<HTMLButtonElement>('.list-item');
      items[0]!.click();
      assertEquals(
          LanguagesPageInteraction.SWITCH_INPUT_METHOD,
          await metricsProxy.whenCalled('recordInteraction'));
    });


    test('dismissing shortcut reminder with accelerator provider', async () => {
      const expectedLastUsedImeAccelerator: StandardAcceleratorProperties = {
        keyDisplay: stringToMojoString16('m'),
        accelerator: {
          modifiers: Modifier.CONTROL,
          keyCode: VKey.kKeyM,
          keyState: AcceleratorKeyState.PRESSED,
          timeStamp: {
            internalValue: 0n,
          },
        },
        originalAccelerator: null,
      };

      const acceleratorProvider = new FakeAcceleratorFetcher();
      inputPage.acceleratorFetcher = acceleratorProvider;
      await flushTasks();

      acceleratorProvider.observeAcceleratorChanges(
          [
            AcceleratorAction.kSwitchToLastUsedIme,
            AcceleratorAction.kSwitchToNextIme,
          ],
          inputPage);
      assertTrue(!!inputPage.acceleratorFetcher);

      // Set an updated lastUsedImeAccelerator, the shortcut reminder should
      // show "ctrl + m" as last used IME.
      acceleratorProvider.mockAcceleratorsUpdated(
          AcceleratorAction.kSwitchToLastUsedIme,
          [expectedLastUsedImeAccelerator]);
      await flushTasks();

      assertTrue(!!inputPage.get('lastUsedImeAccelerator_'));
      assertEquals(
          inputPage.get('lastUsedImeAccelerator_').keyDisplay,
          expectedLastUsedImeAccelerator.keyDisplay);

      const updatedLastUsedImeAccelerator: StandardAcceleratorProperties = {
        keyDisplay: stringToMojoString16('k'),
        accelerator: {
          modifiers: Modifier.CONTROL + Modifier.SHIFT,
          keyCode: VKey.kKeyK,
          keyState: AcceleratorKeyState.PRESSED,
          timeStamp: {
            internalValue: 0n,
          },
        },
        originalAccelerator: null,
      };

      // Update the last used IME with a new accelerator, the shortcut reminder
      // should show "ctrl + shift + k" as last used IME.
      acceleratorProvider.mockAcceleratorsUpdated(
          AcceleratorAction.kSwitchToLastUsedIme,
          [updatedLastUsedImeAccelerator]);
      await flushTasks();
      assertTrue(!!inputPage.get('lastUsedImeAccelerator_'));
      assertEquals(
          (inputPage.get('lastUsedImeAccelerator_'))!.keyDisplay,
          updatedLastUsedImeAccelerator!.keyDisplay);

      let element =
          inputPage.shadowRoot!.querySelector('keyboard-shortcut-banner');
      assertTrue(!!element);

      let dismissButton =
          element.shadowRoot!.querySelector<HTMLButtonElement>('#dismiss');
      assertTrue(!!dismissButton);
      dismissButton.click();
      assertEquals(
          InputsShortcutReminderState.LAST_USED_IME,
          await metricsProxy.whenCalled('recordShortcutReminderDismissed'));
      metricsProxy.resetResolver('recordShortcutReminderDismissed');

      // Add US Swahili keyboard, a third party IME.
      languageHelper.addInputMethod(
          'ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:sw');
      flush();

      // Shortcut reminder should show "next IME" shortcut.
      element = inputPage.shadowRoot!.querySelector('keyboard-shortcut-banner');
      assertTrue(!!element);
      dismissButton =
          element.shadowRoot!.querySelector<HTMLButtonElement>('#dismiss');
      assertTrue(!!dismissButton);
      dismissButton.click();
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
      element = inputPage.shadowRoot!.querySelector('keyboard-shortcut-banner');
      assertTrue(!!element);
      dismissButton =
          element.shadowRoot!.querySelector<HTMLButtonElement>('#dismiss');
      assertTrue(!!dismissButton);
      dismissButton.click();
      assertEquals(
          InputsShortcutReminderState.LAST_USED_IME_AND_NEXT_IME,
          await metricsProxy.whenCalled('recordShortcutReminderDismissed'));
    });

    test(
        'when dismissing shortcut reminder without shortcut provider',
        async () => {
          // Default shortcut reminder with two elements should show "last used
          // IME" reminder.
          let element =
              inputPage.shadowRoot!.querySelector('keyboard-shortcut-banner');
          assertTrue(!!element);
          let dismissButton =
              element.shadowRoot!.querySelector<HTMLButtonElement>('#dismiss');
          assertTrue(!!dismissButton);
          dismissButton.click();
          assertEquals(
              InputsShortcutReminderState.LAST_USED_IME,
              await metricsProxy.whenCalled('recordShortcutReminderDismissed'));
          metricsProxy.resetResolver('recordShortcutReminderDismissed');

          // Add US Swahili keyboard, a third party IME.
          languageHelper.addInputMethod(
              'ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:sw');
          flush();

          // Shortcut reminder should show "next IME" shortcut.
          element =
              inputPage.shadowRoot!.querySelector('keyboard-shortcut-banner');
          assertTrue(!!element);
          dismissButton =
              element.shadowRoot!.querySelector<HTMLButtonElement>('#dismiss');
          assertTrue(!!dismissButton);
          dismissButton.click();
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
          element =
              inputPage.shadowRoot!.querySelector('keyboard-shortcut-banner');
          assertTrue(!!element);
          dismissButton =
              element.shadowRoot!.querySelector<HTMLButtonElement>('#dismiss');
          assertTrue(!!dismissButton);
          dismissButton.click();
          assertEquals(
              InputsShortcutReminderState.LAST_USED_IME_AND_NEXT_IME,
              await metricsProxy.whenCalled('recordShortcutReminderDismissed'));
        });

    test('when clicking on "learn more" about language packs', async () => {
      const languagePacksNotice =
          inputPage.shadowRoot!.querySelector('#languagePacksNotice');
      assertTrue(!!languagePacksNotice);
      const anchor = languagePacksNotice.shadowRoot!.querySelector('a');
      assertTrue(!!anchor);
      // The below would normally create a new window, which would change the
      // focus from this test to the new window.
      // Prevent this from happening by adding an event listener on the anchor
      // element which stops the default behaviour (of opening a new window).
      anchor.addEventListener('click', (e: Event) => e.preventDefault());
      anchor.click();
      assertEquals(
          LanguagesPageInteraction.OPEN_LANGUAGE_PACKS_LEARN_MORE,
          await metricsProxy.whenCalled('recordInteraction'));
    });
  });

  suite('spell check v2', () => {
    let spellCheckToggle: SettingsToggleButtonElement;
    let spellCheckListContainer: HTMLElement;
    // This list is not dynamically updated.
    let spellCheckList: NodeListOf<HTMLElement>;

    setup(async () => {
      // Enable grammar check.
      loadTimeData.overrideValues({
        onDeviceGrammarCheckEnabled: true,
      });
      await createInputPage();

      // Spell check is initially on.
      // Work around b/289955380 by only finding the only button which is not
      // hidden. <dom-if>s use a `display: none;` inline style to hide elements.
      // Because we do not use inline styles, the button which is not hidden
      // does not have a `style` attribute, and the one which is hidden does.
      const toggle =
          inputPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#enableSpellcheckingToggle:not([style])');
      assertTrue(!!toggle);
      spellCheckToggle = toggle;
      assertTrue(spellCheckToggle.checked);

      const list = inputPage.shadowRoot!.querySelector<HTMLElement>(
          '#spellCheckLanguagesListV2');
      assertTrue(!!list);
      spellCheckListContainer = list;

      // The spell check list should only have en-US (excluding the "add
      // languages" button).
      spellCheckList = spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(1 + 1, spellCheckList.length);
      assertStringContains(
          spellCheckList[0]!.textContent!, 'English (United States)');
      assertStringContains(spellCheckList[1]!.textContent!, 'Add languages');
    });

    test('can remove enabled language from spell check list', () => {
      assertDeepEquals(
          ['en-US'], inputPage.prefs.spellcheck.dictionaries.value);
      // Get remove button for en-US.
      const spellCheckLanguageToggle =
          spellCheckList[0]!.querySelector<HTMLButtonElement>('cr-icon-button');
      assertTrue(!!spellCheckLanguageToggle);

      // Remove the language.
      spellCheckLanguageToggle.click();
      flush();

      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      // The spell check list should just have "add languages".
      assertEquals(0 + 1, newSpellCheckList.length);

      assertDeepEquals([], inputPage.prefs.spellcheck.dictionaries.value);
    });

    test('can remove non-enabled language from spell check list', () => {
      // Add a new non-enabled language to spellcheck.dictionaries.
      inputPage.setPrefValue('spellcheck.dictionaries', ['en-US', 'nb']);
      flush();

      let newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should have en-US, nb and "add languages".
      assertEquals(2 + 1, newSpellCheckList.length);
      assertStringContains(
          newSpellCheckList[0]!.textContent!, 'English (United States)');
      assertStringContains(
          newSpellCheckList[1]!.textContent!, 'Norwegian Bokmål');

      // Remove nb.
      const icon = newSpellCheckList[1]!.querySelector('cr-icon-button');
      assertTrue(!!icon);
      icon.click();
      flush();
      newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should have en-US and "add languages".
      assertEquals(1 + 1, newSpellCheckList.length);
      assertStringContains(
          newSpellCheckList[0]!.textContent!, 'English (United States)');

      assertDeepEquals(
          ['en-US'], inputPage.prefs.spellcheck.dictionaries.value);
    });

    test('shows force-on spell check language turned on by user', () => {
      // Force-enable a spell check language originally set by the user.
      inputPage.setPrefValue('spellcheck.forced_dictionaries', ['en-US']);
      flush();

      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should have en-US and "add languages".
      assertEquals(1 + 1, newSpellCheckList.length);

      const forceEnabledEnUSLanguageRow = newSpellCheckList[0];
      assertTrue(!!forceEnabledEnUSLanguageRow);
      assertStringContains(
          forceEnabledEnUSLanguageRow.textContent!, 'English (United States)');
      assertTrue(!!forceEnabledEnUSLanguageRow.querySelector(
          'cr-policy-pref-indicator'));
      // Polymer sometimes hides the old enabled element by using a
      // display: none, so we use the managed-button class to get a reference to
      // the new disabled button.
      const managedButton =
          forceEnabledEnUSLanguageRow.querySelector<HTMLButtonElement>(
              '.managed-button');
      assertTrue(!!managedButton);
      assertTrue(managedButton.disabled);
    });

    test('shows force-on enabled spell check language', () => {
      // Force-enable an enabled language via policy.
      inputPage.setPrefValue('spellcheck.forced_dictionaries', ['sw']);
      flush();

      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should have en-US, sw and "add languages".
      assertEquals(2 + 1, newSpellCheckList.length);
      assertStringContains(
          newSpellCheckList[0]!.textContent!, 'English (United States)');

      const forceEnabledSwLanguageRow = newSpellCheckList[1];
      assertTrue(!!forceEnabledSwLanguageRow);
      assertStringContains(forceEnabledSwLanguageRow.textContent!, 'Swahili');
      assertTrue(!!forceEnabledSwLanguageRow.querySelector(
          'cr-policy-pref-indicator'));
      const managedButton =
          forceEnabledSwLanguageRow.querySelector<HTMLButtonElement>(
              '.managed-button');
      assertTrue(!!managedButton);
      assertTrue(managedButton.disabled);
    });

    test('shows force-on non-enabled spell check language', () => {
      // Force-enable a non-enabled language via policy.
      inputPage.setPrefValue('spellcheck.forced_dictionaries', ['nb']);
      flush();

      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');

      // The spell check list should have en-US, nb and "add languages".
      assertEquals(2 + 1, newSpellCheckList.length);
      assertStringContains(
          newSpellCheckList[0]!.textContent!, 'English (United States)');

      const forceEnabledNbLanguageRow = newSpellCheckList[1];
      assertTrue(!!forceEnabledNbLanguageRow);
      assertStringContains(
          forceEnabledNbLanguageRow.textContent!, 'Norwegian Bokmål');
      assertTrue(!!forceEnabledNbLanguageRow.querySelector(
          'cr-policy-pref-indicator'));
      const managedButton =
          forceEnabledNbLanguageRow.querySelector<HTMLButtonElement>(
              '.managed-button');
      assertTrue(!!managedButton);
      assertTrue(managedButton.disabled);
    });

    test('does not show force-off spell check language enabled by user', () => {
      // Force-disable a spell check language originally set by the user.
      inputPage.setPrefValue('spellcheck.blocked_dictionaries', ['en-US']);
      flush();

      // The spell check list should just have "add languages".
      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(0 + 1, newSpellCheckList.length);
    });

    test('does not show force-off enabled spell check language', () => {
      // Force-disable an enabled language via policy.
      inputPage.setPrefValue('spellcheck.blocked_dictionaries', ['sw']);
      flush();

      // The spell check list should be the same (en-US, "add languages").
      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(1 + 1, newSpellCheckList.length);
      assertStringContains(
          newSpellCheckList[0]!.textContent!, 'English (United States)');
    });

    test('does not show force-off non-enabled spell check language', () => {
      // Force-disable a non-enabled language via policy.
      inputPage.setPrefValue('spellcheck.blocked_dictionaries', ['nb']);
      flush();

      // The spell check list should be the same (en-US, "add languages").
      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(1 + 1, newSpellCheckList.length);
      assertStringContains(
          newSpellCheckList[0]!.textContent!, 'English (United States)');
    });

    test('toggle off disables buttons', () => {
      assertTrue(spellCheckToggle.checked);
      let iconButton = spellCheckList[0]!.querySelector('cr-icon-button');
      assertTrue(!!iconButton);
      assertFalse(iconButton.disabled);
      // "Add languages" uses a cr-button instead of a cr-icon-button.
      let button = spellCheckList[1]!.querySelector('cr-button');
      assertTrue(!!button);
      assertFalse(button.disabled);

      spellCheckToggle.click();

      assertFalse(spellCheckToggle.checked);
      iconButton = spellCheckList[0]!.querySelector('cr-icon-button');
      assertTrue(!!iconButton);
      assertTrue(iconButton.disabled);
      button = spellCheckList[1]!.querySelector('cr-button');
      assertTrue(!!button);
      assertTrue(button.disabled);
    });

    test('languages are in sorted order', () => {
      inputPage.setPrefValue(
          'spellcheck.dictionaries', ['sw', 'en-US', 'nb', 'en-CA']);
      flush();
      // The spell check list should be sorted by display name:
      // English (Canada), English (United States), Norwegian Bokmål, then
      // Swahili.
      const newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(4 + 1, newSpellCheckList.length);
      assertStringContains(
          newSpellCheckList[0]!.textContent!, 'English (Canada)');
      assertStringContains(
          newSpellCheckList[1]!.textContent!, 'English (United States)');
      assertStringContains(
          newSpellCheckList[2]!.textContent!, 'Norwegian Bokmål');
      assertStringContains(newSpellCheckList[3]!.textContent!, 'Swahili');
    });

    test('removing all languages, then adding enabled language works', () => {
      // See https://crbug.com/1197386 for more information.
      // Remove en-US so there are no spell check languages.
      const spellCheckLanguageToggle =
          spellCheckList[0]!.querySelector('cr-icon-button');
      assertTrue(!!spellCheckLanguageToggle);
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
      assertStringContains(
          newSpellCheckList[0]!.textContent!, 'English (United States)');
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
      assertStringContains(
          newSpellCheckList[0]!.textContent!, 'English (United States)');

      // Add a spell check language not in Accept-Language.
      languageHelper.toggleSpellCheck('nb', true);
      flush();

      // The spell check list should now have en-US, nb and "add languages".
      newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(2 + 1, newSpellCheckList.length);
      assertStringContains(
          newSpellCheckList[0]!.textContent!, 'English (United States)');
      assertStringContains(
          newSpellCheckList[1]!.textContent!, 'Norwegian Bokmål');

      // Add an arbitrary language to Accept-Language.
      languageHelper.enableLanguage('tk');
      flush();

      // The spell check list should remain the same.
      newSpellCheckList =
          spellCheckListContainer.querySelectorAll('.list-item');
      assertEquals(2 + 1, newSpellCheckList.length);
      assertStringContains(
          newSpellCheckList[0]!.textContent!, 'English (United States)');
      assertStringContains(
          newSpellCheckList[1]!.textContent!, 'Norwegian Bokmål');
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
      inputPage.setPrefValue('spellcheck.dictionaries', ['en-US', 'sw']);
      flush();
      const checkAllHidden = (nodes: HTMLElement[]) => {
        assertTrue(nodes.every(node => node.hidden));
      };

      const languageSettingsPrivate =
          browserProxy.getLanguageSettingsPrivate() as unknown as
          FakeLanguageSettingsPrivate;
      const errorDivs =
          Array.from(spellCheckListContainer.querySelectorAll<HTMLElement>(
              '.name-with-error div'));
      assertEquals(2, errorDivs.length);
      checkAllHidden(errorDivs);

      const retryButtons = Array.from(
          spellCheckListContainer.querySelectorAll<HTMLButtonElement>(
              'cr-button:not(#addSpellcheckLanguages)'));
      assertEquals(2, retryButtons.length);

      const languageCode = inputPage.get('languages.enabled.0.language.code');
      languageSettingsPrivate.onSpellcheckDictionariesChanged.callListeners([
        {languageCode, isReady: false, downloadFailed: true},
      ]);

      flush();
      assertFalse(errorDivs[0]!.hidden);
      assertFalse(retryButtons[0]!.hidden);
      assertFalse(retryButtons[0]!.disabled);

      // turns off spell check disable retry button.
      spellCheckToggle.click();
      assertTrue(retryButtons[0]!.disabled);

      // turns spell check back on and enable download.
      spellCheckToggle.click();
      languageSettingsPrivate.onSpellcheckDictionariesChanged.callListeners([
        {languageCode, isReady: true, downloadFailed: false},
      ]);

      flush();
      assertTrue(errorDivs[0]!.hidden);
      assertTrue(retryButtons[0]!.hidden);
    });

    test('toggle off disables edit dictionary', () => {
      const editDictionarySubpageTrigger =
          inputPage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#editDictionarySubpageTrigger');
      assertTrue(!!editDictionarySubpageTrigger);
      assertFalse(editDictionarySubpageTrigger.disabled);
      spellCheckToggle.click();

      assertTrue(editDictionarySubpageTrigger.disabled);
    });

    test('opens edit dictionary page', () => {
      const editDictionarySubpageTrigger =
          inputPage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#editDictionarySubpageTrigger');
      assertTrue(!!editDictionarySubpageTrigger);
      editDictionarySubpageTrigger.click();
      const router = Router.getInstance();
      assertEquals(
          'chrome://os-settings/osLanguages/editDictionary',
          router.currentRoute.getAbsolutePath());
    });
  });

  suite('add spell check languages dialog', () => {
    let dialog: OsSettingsAddItemsDialogElement;
    let suggestedList: IronListElement;
    let allLangsList: IronListElement;
    let cancelButton: HTMLButtonElement;
    let actionButton: HTMLButtonElement;

    /**
     * Returns the list items in the dialog.
     */
    function getAllLanguagesCheckboxWithPolicies():
        CrCheckboxWithPolicyElement[] {
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
     */
    function getAllLanguagesCheckboxes(): CrCheckboxElement[] {
      const checkboxWithPolicies = getAllLanguagesCheckboxWithPolicies();
      return checkboxWithPolicies.map(checkboxWithPolicy => {
        const checkBox =
            checkboxWithPolicy.shadowRoot!.querySelector<CrCheckboxElement>(
                '#checkbox');
        assertTrue(!!checkBox);
        return checkBox;
      });
    }

    setup(async () => {
      await createInputPage();

      let element = inputPage.shadowRoot!.querySelector(
          'os-settings-add-spellcheck-languages-dialog');
      assertNull(element);
      const addSpellcheckLanguages =
          inputPage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#addSpellcheckLanguages');
      assertTrue(!!addSpellcheckLanguages);
      addSpellcheckLanguages.click();
      flush();

      element = inputPage.shadowRoot!.querySelector(
          'os-settings-add-spellcheck-languages-dialog');
      assertTrue(!!element);
      const dialogElement =
          element.shadowRoot!.querySelector('os-settings-add-items-dialog');
      assertTrue(!!dialogElement);
      dialog = dialogElement;
      assertTrue(dialog.$.dialog.open);

      const list = dialog.shadowRoot!.querySelector<IronListElement>(
          '#suggested-items-list');
      assertTrue(!!list);
      suggestedList = list;
      const langList = dialog.shadowRoot!.querySelector<IronListElement>(
          '#filtered-items-list');
      assertTrue(!!langList);
      allLangsList = langList;

      const button =
          dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
      assertTrue(!!button);
      actionButton = button;
      const cancel =
          dialog.shadowRoot!.querySelector<HTMLButtonElement>('.cancel-button');
      assertTrue(!!cancel);
      cancelButton = cancel;
    });

    test('action button is enabled and disabled when necessary', () => {
      // Mimic $$, but with a querySelectorAll instead of querySelector.
      const checkboxes = getAllLanguagesCheckboxes();
      assertGT(checkboxes.length, 0);

      // By default, no languages have been selected so the action button is
      // disabled.
      assertTrue(actionButton.disabled);

      // Selecting a language enables the action button.
      checkboxes[0]!.click();
      assertFalse(actionButton.disabled);

      // Selecting the same language again disables the action button.
      checkboxes[0]!.click();
      assertTrue(actionButton.disabled);
    });

    test('cancel button is never disabled', () => {
      assertFalse(cancelButton.disabled);
    });

    test('initial expected layout', () => {
      // As Swahili is an enabled language, it should be shown as a suggested
      // language.
      const suggestedItems = suggestedList.querySelectorAll('cr-checkbox');
      assertEquals(1, suggestedItems.length);
      assertStringContains(suggestedItems[0]!.textContent!, 'Swahili');

      // There are four languages with spell check enabled in
      // fake_language_settings_private.js: en-US, en-CA, sw, nb.
      // en-US shouldn't be displayed as it is already enabled.
      const allItems = getAllLanguagesCheckboxWithPolicies();
      assertEquals(3, allItems.length);
      assertStringContains(allItems[0]!.textContent!, 'English (Canada)');
      assertStringContains(allItems[1]!.textContent!, 'Swahili');
      assertStringContains(allItems[2]!.textContent!, 'Norwegian Bokmål');

      // By default, all checkboxes should not be disabled, and should not be
      // checked.
      const checkboxes = [...suggestedItems, ...getAllLanguagesCheckboxes()];
      assertTrue(checkboxes.every(checkbox => !checkbox.disabled));
      assertTrue(checkboxes.every(checkbox => !checkbox.checked));

      // There should be a label for both sections.
      const suggestedLabel =
          dialog.shadowRoot!.querySelector('#suggested-items-label');
      assertTrue(isVisible(suggestedLabel));

      const allLangsLabel =
          dialog.shadowRoot!.querySelector('#filtered-items-label');
      assertTrue(isVisible(allLangsLabel));
    });

    test('can add single language and uncheck language', () => {
      const checkboxes = getAllLanguagesCheckboxes();
      const swCheckbox = checkboxes[1];
      const nbCheckbox = checkboxes[2];
      assertTrue(!!swCheckbox);
      assertTrue(!!nbCheckbox);

      // By default, en-US should be the only enabled spell check dictionary.
      assertDeepEquals(
          ['en-US'], inputPage.prefs.spellcheck.dictionaries.value);

      swCheckbox.click();
      assertTrue(swCheckbox.checked);

      // Check and uncheck nb to ensure that it gets "ignored".
      nbCheckbox.click();
      assertTrue(nbCheckbox.checked);

      nbCheckbox.click();
      assertFalse(nbCheckbox.checked);

      actionButton.click();
      assertDeepEquals(
          ['en-US', 'sw'], inputPage.prefs.spellcheck.dictionaries.value);
      assertFalse(dialog.$.dialog.open);
    });

    test('can add multiple languages', () => {
      const checkboxes = getAllLanguagesCheckboxes();

      assertDeepEquals(
          ['en-US'], inputPage.prefs.spellcheck.dictionaries.value);

      // Click en-CA and nb.
      checkboxes[0]!.click();
      assertTrue(checkboxes[0]!.checked);
      checkboxes[2]!.click();
      assertTrue(checkboxes[2]!.checked);

      actionButton.click();
      // The two possible results are en-US, en-CA, nb OR en-US, nb, en-CA.
      // We do not care about the ordering of the last two, but the first one
      // should still be en-US.
      assertEquals('en-US', inputPage.prefs.spellcheck.dictionaries.value[0]);
      // Note that .sort() mutates the array, but as this is the end of the test
      // the prefs will be reset after this anyway.
      assertDeepEquals(
          ['en-CA', 'en-US', 'nb'],
          inputPage.prefs.spellcheck.dictionaries.value.sort());
      assertFalse(dialog.$.dialog.open);
    });

    test('policy disabled languages cannot be selected and show icon', () => {
      // Force-disable sw.
      inputPage.setPrefValue('spellcheck.blocked_dictionaries', ['sw']);
      flush();

      const swCheckboxWithPolicy = getAllLanguagesCheckboxWithPolicies()[1];
      assertTrue(!!swCheckboxWithPolicy);
      const swCheckbox =
          swCheckboxWithPolicy.shadowRoot!.querySelector('cr-checkbox');
      assertTrue(!!swCheckbox);
      const swPolicyIcon =
          swCheckboxWithPolicy.shadowRoot!.querySelector('iron-icon');
      assertTrue(!!swPolicyIcon);

      assertTrue(swCheckbox.disabled);
      assertFalse(swCheckbox.checked);
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
      inputPage.setPrefValue('spellcheck.dictionaries', []);
      languageHelper.disableLanguage('en-US');
      flush();

      // Both Swahili (as it is an enabled language) and English (US) (as it is
      // enabled as an input method) should appear in the list.
      const suggestedListItems = suggestedList.querySelectorAll('.list-item');
      assertEquals(2, suggestedListItems.length);
      assertStringContains(
          suggestedListItems[0]!.textContent!, 'English (United States)');
      assertStringContains(suggestedListItems[1]!.textContent!, 'Swahili');

      // en-US should also appear in the all languages list now.
      assertEquals(4, allLangsList.querySelectorAll('.list-item').length);
    });

    test('searches languages on display name', () => {
      const searchInput = dialog.shadowRoot!.querySelector('cr-search-field');
      assertTrue(!!searchInput);
      // Expecting a few languages to be displayed when no query exists.
      assertGE(getAllLanguagesCheckboxWithPolicies().length, 1);

      // Issue query that matches the |displayedName| in lowercase.
      searchInput.setValue('norwegian');
      flush();
      assertEquals(1, getAllLanguagesCheckboxWithPolicies().length);
      assertStringContains(
          getAllLanguagesCheckboxWithPolicies()[0]!.textContent!,
          'Norwegian Bokmål');

      // Issue query that matches the |nativeDisplayedName|.
      searchInput.setValue('norsk');
      flush();
      assertEquals(1, getAllLanguagesCheckboxWithPolicies().length);

      // Issue query that does not match any language.
      searchInput.setValue('egaugnal');
      flush();
      assertEquals(0, getAllLanguagesCheckboxWithPolicies().length);
      assertTrue(
          isVisible(dialog.shadowRoot!.querySelector('#no-search-results')));
    });

    test('has escape key behavior working correctly', () => {
      const searchInput = dialog.shadowRoot!.querySelector('cr-search-field');
      assertTrue(!!searchInput);
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

  suite('Suggestions', () => {
    suite('when emoji suggestions are not available', () => {
      setup(() => {
        loadTimeData.overrideValues({allowEmojiSuggestion: false});
      });
    });

    test('Emoji suggestion toggle is visible', async () => {
      await createInputPage();
      const emojiSuggestionToggle =
          inputPage.shadowRoot!.querySelector('#emojiSuggestionToggle');
      assertTrue(isVisible(emojiSuggestionToggle));
    });

    test('Deep link to emoji suggestion toggle', async () => {
      await createInputPage();

      const params = new URLSearchParams();
      const setting = settingMojom.Setting.kShowEmojiSuggestions;
      params.append('settingId', setting.toString());
      Router.getInstance().navigateTo(routes.OS_LANGUAGES_INPUT, params);
      flush();

      const deepLinkElement = inputPage.shadowRoot!.querySelector<HTMLElement>(
          '#emojiSuggestionToggle');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, inputPage.shadowRoot!.activeElement,
          `Emoji suggestion toggle should be focused for settingId=${
              setting}.`);
    });

    suite('when allowOrca is false', () => {
      setup(() => {
        loadTimeData.overrideValues({allowOrca: false});
      });

      test('Orca toggle should be hidden', async () => {
        await createInputPage();
        const orcaToggle = inputPage.shadowRoot!.querySelector('#orcaToggle');
        assertFalse(isVisible(orcaToggle));
      });
    });

    suite('when allowOrca is true', () => {
      setup(() => {
        loadTimeData.overrideValues({allowOrca: true});
      });

      test('Orca toggle should be visible', async () => {
        await createInputPage();
        const orcaToggle = inputPage.shadowRoot!.querySelector('#orcaToggle');
        assertTrue(isVisible(orcaToggle));
      });

      test('Deep link to orca suggestion toggle', async () => {
        await createInputPage();

        const params = new URLSearchParams();
        const setting = settingMojom.Setting.kShowOrca;
        params.append('settingId', setting.toString());
        Router.getInstance().navigateTo(routes.OS_LANGUAGES_INPUT, params);
        flush();

        const deepLinkElement =
            inputPage.shadowRoot!.querySelector<HTMLElement>('#orcaToggle');
        assertTrue(!!deepLinkElement);
        await waitAfterNextRender(deepLinkElement);
        assertEquals(
            deepLinkElement, inputPage.shadowRoot!.activeElement,
            `Orca suggestion toggle should be focused for settingId=${
                setting}.`);
      });
    });

    suite('when both the emoji suggestions and orca are not allowed', () => {
      setup(() => {
        loadTimeData.overrideValues(
            {allowEmojiSuggestion: false, allowOrca: false});
      });

      test('Suggestions section is not visible', async () => {
        await createInputPage();
        const suggestionsSection =
            inputPage.shadowRoot!.querySelector('#suggestionsSection');
        assertFalse(isVisible(suggestionsSection));
      });
    });
  });
});
