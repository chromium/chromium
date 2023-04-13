// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isWindows} from 'chrome://resources/js/platform.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrCheckboxElement, kMenuCloseDelay, LanguageHelper, LanguagesBrowserProxyImpl, SettingsAddLanguagesDialogElement, SettingsLanguagesPageElement} from 'chrome://settings/lazy_load.js';
import {CrActionMenuElement, CrButtonElement, CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGE, assertGT, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import {FakeLanguageSettingsPrivate, getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';

// clang-format on

const languages_page_tests = {
  TestNames: {
    AddLanguagesDialog: 'add languages dialog',
    LanguageMenuWithTranslate: 'language menu with translate options',
    LanguageMenuMoveLanguages: 'move language menu items',
  },
};

Object.assign(window, {languages_page_tests});

suite('languages page', function() {
  let languageHelper: LanguageHelper;
  let languagesPage: SettingsLanguagesPageElement;
  let actionMenu: CrActionMenuElement;
  let browserProxy: TestLanguagesBrowserProxy;

  /*
   * Finds, asserts and returns the menu item for the given i18n key.
   * @param i18nKey Name of the i18n string for the item's text.
   */
  function getMenuItem<T extends HTMLElement>(i18nKey: string): T {
    const i18nString = loadTimeData.getString(i18nKey);
    assertTrue(!!i18nString);
    const menuItems = actionMenu.querySelectorAll<T>('.dropdown-item');
    const menuItem = Array.from(menuItems).find(
        item => item.textContent!.trim() === i18nString);
    assertTrue(!!menuItem, 'Menu item "' + i18nKey + '" not found');
    return menuItem;
  }

  // Initial value of enabled languages pref used in tests.
  const initialLanguages = 'en-US,sw';

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

      languagesPage = document.createElement('settings-languages-page');

      languagesPage.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, languagesPage, 'prefs');

      languagesPage.languageHelper = settingsLanguages.languageHelper;
      fakeDataBind(settingsLanguages, languagesPage, 'language-helper');

      languagesPage.languages = settingsLanguages.languages;
      fakeDataBind(settingsLanguages, languagesPage, 'languages');

      document.body.appendChild(languagesPage);
      flush();
      actionMenu = languagesPage.$.menu.get();

      languageHelper = languagesPage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  suite(languages_page_tests.TestNames.AddLanguagesDialog, function() {
    let dialog: SettingsAddLanguagesDialogElement;
    let dialogItems: NodeListOf<CrCheckboxElement>;
    let cancelButton: CrButtonElement;
    let actionButton: CrButtonElement;
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
            languagesPage.shadowRoot!.querySelector(
                'settings-add-languages-dialog'));
        observer.disconnect();
        assertTrue(!!dialogClosedResolver);
        dialogClosedResolver.resolve();
      }
    }

    setup(function() {
      const addLanguagesButton =
          languagesPage.shadowRoot!.querySelector<HTMLElement>('#addLanguages')!
          ;
      const whenDialogOpen = eventToPromise('cr-dialog-open', languagesPage);
      addLanguagesButton.click();

      // The page stamps the dialog, registers listeners, and populates the
      // iron-list asynchronously at microtask timing, so wait for a new task.
      return whenDialogOpen.then(() => {
        dialog = languagesPage.shadowRoot!.querySelector(
            'settings-add-languages-dialog')!;
        assertTrue(!!dialog);

        // Observe the removal of the dialog via MutationObserver since the
        // HTMLDialogElement 'close' event fires at an unpredictable time.
        dialogClosedResolver = new PromiseResolver();
        dialogClosedObserver = new MutationObserver(onMutation);
        dialogClosedObserver.observe(
            languagesPage.shadowRoot!, {childList: true});

        actionButton = dialog.shadowRoot!.querySelector<CrButtonElement>(
            '.action-button')!;
        assertTrue(!!actionButton);
        cancelButton = dialog.shadowRoot!.querySelector<CrButtonElement>(
            '.cancel-button')!;
        assertTrue(!!cancelButton);
        flush();

        // The fixed-height dialog's iron-list should stamp far fewer than
        // 50 items.
        dialogItems = dialog.$.dialog.querySelectorAll<CrCheckboxElement>(
            '.list-item:not([hidden])');
        assertGT(dialogItems.length, 1);
        assertLT(dialogItems.length, 50);

        // No languages have been checked, so the action button is disabled.
        assertTrue(actionButton.disabled);
        assertFalse(cancelButton.disabled);
      });
    });

    teardown(function() {
      dialogClosedObserver.disconnect();
    });

    test('cancel', function() {
      // Canceling the dialog should close and remove it.
      cancelButton.click();

      return dialogClosedResolver.promise;
    });

    test('add languages and cancel', function() {
      // Check some languages.
      dialogItems[1]!.click();  // en-CA.
      dialogItems[2]!.click();  // tk.

      // Canceling the dialog should close and remove it without enabling
      // the checked languages.
      cancelButton.click();
      return dialogClosedResolver.promise.then(function() {
        assertEquals(
            initialLanguages,
            languagesPage.getPref('intl.accept_languages').value);
      });
    });

    test('add languages and confirm', function() {
      // No languages have been checked, so the action button is inert.
      actionButton.click();
      flush();
      assertEquals(
          dialog,
          languagesPage.shadowRoot!.querySelector(
              'settings-add-languages-dialog'));

      // Check and uncheck one language.
      dialogItems[0]!.click();
      assertFalse(actionButton.disabled);
      dialogItems[0]!.click();
      assertTrue(actionButton.disabled);

      // Check multiple languages.
      dialogItems[0]!.click();  // en.
      dialogItems[2]!.click();  // tk.
      assertFalse(actionButton.disabled);

      // The action button should close and remove the dialog, enabling the
      // checked languages.
      actionButton.click();

      assertEquals(
          initialLanguages + ',en,tk',
          languagesPage.getPref('intl.accept_languages').value);

      return dialogClosedResolver.promise;
    });

    // Test that searching languages works whether the displayed or native
    // language name is queried.
    test('search languages', function() {
      const searchInput = dialog.shadowRoot!.querySelector('cr-search-field');
      assertTrue(!!searchInput);

      const getItems = function() {
        return dialog.$.dialog.querySelectorAll('.list-item:not([hidden])');
      };

      // Expecting a few languages to be displayed when no query exists.
      assertGE(getItems().length, 1);

      // Issue query that matches the |displayedName|.
      searchInput.setValue('greek');
      flush();
      assertEquals(1, getItems().length);

      // Issue query that matches the |nativeDisplayedName|.
      searchInput.setValue('Ελληνικά');
      flush();
      assertEquals(1, getItems().length);

      // Issue query that does not match any language.
      searchInput.setValue('egaugnal');
      flush();
      assertEquals(0, getItems().length);

      // Issue query that should never match any language.
      searchInput.setValue('_arc_ime_language_');
      flush();
      assertEquals(0, getItems().length);
    });

    test('Escape key behavior', function() {
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

  suite(languages_page_tests.TestNames.LanguageMenuWithTranslate, function() {
    /*
     * When the detailed language settings are not shown the translate options
     * are integrated with the language list. This suite tests the translate
     * option in the languages submenu.
     */
    suiteSetup(function() {
      loadTimeData.overrideValues({
        enableDesktopDetailedLanguageSettings: false,
      });
    });

    test('structure', function() {
      const languageOptionsDropdownTrigger =
          languagesPage.shadowRoot!.querySelector('cr-icon-button');
      assertTrue(!!languageOptionsDropdownTrigger);
      languageOptionsDropdownTrigger.click();
      assertTrue(actionMenu.open);

      const separator = actionMenu.querySelector('hr');
      assertTrue(!!separator);
      assertEquals(1, separator.offsetHeight);

      // Disable Translate. On platforms that can't change the UI language,
      // this hides all the checkboxes, so the separator isn't needed.
      // Windows still shows a checkbox and thus the separator.
      languagesPage.setPrefValue('translate.enabled', false);
      assertEquals(isWindows ? 1 : 0, separator.offsetHeight);
    });

    test('test translate.enable toggle', function() {
      const settingsToggle =
          languagesPage.shadowRoot!.querySelector<HTMLElement>(
              '#offerTranslateOtherLanguages');
      assertTrue(!!settingsToggle);

      // Clicking on the toggle switches it to false.
      settingsToggle.click();
      let newToggleValue = languagesPage.getPref('translate.enabled').value;
      assertFalse(newToggleValue);

      // Clicking on the toggle switches it to true again.
      settingsToggle.click();
      newToggleValue = languagesPage.getPref('translate.enabled').value;
      assertTrue(newToggleValue);
    });

    test('test translate target language is labelled', function() {
      // Translate target language disabled.
      const targetLanguageCode = languageHelper.languages!.translateTarget;
      assertTrue(!!targetLanguageCode);
      assertTrue(languageHelper.languages!.enabled.some(
          l => languageHelper.convertLanguageCodeForTranslate(
                   l.language.code) === targetLanguageCode));
      assertTrue(languageHelper.languages!.enabled.some(
          l => languageHelper.convertLanguageCodeForTranslate(
                   l.language.code) !== targetLanguageCode));
      let translateTargetLabel = null;
      let item = null;

      const listItems =
          languagesPage.shadowRoot!.querySelector('#languagesSection')!
              .querySelectorAll<HTMLElement>('.list-item');
      const domRepeat = languagesPage.shadowRoot!.querySelector('dom-repeat');
      assertTrue(!!domRepeat);

      let num_visibles = 0;
      Array.from(listItems).forEach(function(el) {
        item = domRepeat.itemForElement(el);
        if (item) {
          translateTargetLabel = el.querySelector('.target-info');
          assertTrue(!!translateTargetLabel);
          if (getComputedStyle(translateTargetLabel).display !== 'none') {
            num_visibles++;
            assertEquals(
                targetLanguageCode,
                languageHelper.convertLanguageCodeForTranslate(
                    item.language.code));
          }
        }
        assertEquals(
            1, num_visibles,
            'Not exactly one target info label (' + num_visibles + ').');
      });
    });

    test('toggle translate for a specific language', function(done) {
      // Open options for 'sw'.
      const languageOptionsDropdownTrigger =
          languagesPage.shadowRoot!.querySelector<HTMLElement>('#more-sw');
      assertTrue(!!languageOptionsDropdownTrigger);
      languageOptionsDropdownTrigger.click();
      assertTrue(actionMenu.open);

      // 'sw' supports translate to the target language ('en').
      const translateOption =
          getMenuItem<CrCheckboxElement>('offerToTranslateInThisLanguage');
      assertFalse(translateOption.disabled);
      assertTrue(translateOption.checked);

      // Toggle the translate option.
      translateOption.click();

      // Menu should stay open briefly.
      assertTrue(actionMenu.open);
      // Guaranteed to run later than the menu close delay.
      setTimeout(function() {
        assertFalse(actionMenu.open);
        assertDeepEquals(
            ['en-US', 'sw'],
            languagesPage.getPref('translate_blocked_languages').value);
        done();
      }, kMenuCloseDelay + 1);
    });

    test('toggle translate for target language', function() {
      // Open options for 'en'.
      const languageOptionsDropdownTrigger =
          languagesPage.shadowRoot!.querySelector('cr-icon-button');
      assertTrue(!!languageOptionsDropdownTrigger);
      languageOptionsDropdownTrigger.click();
      assertTrue(actionMenu.open);

      // 'en' does not support.
      const translateOption =
          getMenuItem<CrCheckboxElement>('offerToTranslateInThisLanguage');
      assertTrue(translateOption.disabled);
    });

    test('disable translate hides language-specific option', function() {
      // Disables translate.
      languagesPage.setPrefValue('translate.enabled', false);

      // Open options for 'sw'.
      const languageOptionsDropdownTrigger =
          languagesPage.shadowRoot!.querySelector<HTMLElement>('#more-sw');
      assertTrue(!!languageOptionsDropdownTrigger);
      languageOptionsDropdownTrigger.click();
      assertTrue(actionMenu.open);

      // The language-specific translation option should be hidden.
      const translateOption =
          actionMenu.querySelector<HTMLElement>('#offerTranslations');
      assertTrue(!!translateOption);
      assertTrue(translateOption.hidden);
    });
  });

  suite(languages_page_tests.TestNames.LanguageMenuMoveLanguages, function() {
    /*
     * Checks the visibility of each expected menu item button.
     * @param Dictionary from i18n keys to expected visibility of those menu
     *     items.
     */
    function assertMenuItemButtonsVisible(
        buttonVisibility: {[key: string]: boolean}) {
      assertTrue(actionMenu.open);
      for (const buttonKey of Object.keys(buttonVisibility)) {
        const buttonItem = getMenuItem(buttonKey);
        assertEquals(
            !buttonVisibility[buttonKey], buttonItem.hidden,
            'Menu item "' + buttonKey + '" hidden');
      }
    }


    test('remove language when starting with 3 languages', function() {
      // Enable a language which we can then disable.
      languageHelper.enableLanguage('no');

      // Populate the dom-repeat.
      flush();

      // Find the new language item.
      const items =
          languagesPage.shadowRoot!.querySelector('#languagesSection')!
              .querySelectorAll<HTMLElement>('.list-item');
      const domRepeat = languagesPage.shadowRoot!.querySelector('dom-repeat');
      assertTrue(!!domRepeat);
      const item = Array.from(items).find(function(el) {
        return domRepeat.itemForElement(el) &&
            domRepeat.itemForElement(el).language.code === 'no';
      });
      assertTrue(!!item);

      // Open the menu and select Remove.
      item.querySelector('cr-icon-button')!.click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem<HTMLButtonElement>('removeLanguage');
      assertFalse(removeMenuItem.disabled);
      assertFalse(removeMenuItem.hidden);
      removeMenuItem.click();
      assertFalse(actionMenu.open);

      assertEquals(
          initialLanguages,
          languagesPage.getPref('intl.accept_languages').value);
    });

    test('remove last blocked language', function() {
      assertEquals(
          initialLanguages,
          languagesPage.getPref('intl.accept_languages').value);
      assertDeepEquals(
          ['en-US'],
          languagesPage.getPref('translate_blocked_languages').value);

      const items =
          languagesPage.shadowRoot!.querySelector('#languagesSection')!
              .querySelectorAll<HTMLElement>('.list-item');
      const domRepeat = languagesPage.shadowRoot!.querySelector('dom-repeat');
      assertTrue(!!domRepeat);
      const item = Array.from(items).find(function(el) {
        return domRepeat.itemForElement(el) &&
            domRepeat.itemForElement(el).language.code === 'en-US';
      });
      assertTrue(!!item);
      // Open the menu and select Remove.
      item.querySelector('cr-icon-button')!.click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem<HTMLButtonElement>('removeLanguage');
      // Remove button should be disabled
      assertFalse(removeMenuItem.hidden);
      assertTrue(removeMenuItem.disabled);
    });

    test('remove language when starting with 2 languages', function() {
      const items =
          languagesPage.shadowRoot!.querySelector('#languagesSection')!
              .querySelectorAll<HTMLElement>('.list-item');
      const domRepeat = languagesPage.shadowRoot!.querySelector('dom-repeat');
      assertTrue(!!domRepeat);
      const item = Array.from(items).find(function(el) {
        return domRepeat.itemForElement(el) &&
            domRepeat.itemForElement(el).language.code === 'sw';
      });
      assertTrue(!!item);

      // Open the menu and select Remove.
      item.querySelector('cr-icon-button')!.click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem<HTMLButtonElement>('removeLanguage');
      assertFalse(removeMenuItem.disabled);
      assertFalse(removeMenuItem.hidden);
      removeMenuItem.click();
      assertFalse(actionMenu.open);

      assertEquals(
          'en-US', languagesPage.getPref('intl.accept_languages').value);
    });

    test('move up/down buttons', function() {
      // Add several languages.
      for (const language of ['en-CA', 'en-US', 'tk', 'no']) {
        languageHelper.enableLanguage(language);
      }

      flush();

      const menuButtons =
          languagesPage.shadowRoot!.querySelector('#languagesSection')!
              .querySelectorAll<HTMLElement>(
                  '.list-item cr-icon-button.icon-more-vert');

      // First language should not have "Move up" or "Move to top".
      menuButtons[0]!.click();
      assertMenuItemButtonsVisible({
        moveToTop: false,
        moveUp: false,
        moveDown: true,
      });
      actionMenu.close();

      // Second language should not have "Move up".
      menuButtons[1]!.click();
      assertMenuItemButtonsVisible({
        moveToTop: true,
        moveUp: false,
        moveDown: true,
      });
      actionMenu.close();

      // Middle languages should have all buttons.
      menuButtons[2]!.click();
      assertMenuItemButtonsVisible({
        moveToTop: true,
        moveUp: true,
        moveDown: true,
      });
      actionMenu.close();

      // Last language should not have "Move down".
      menuButtons[menuButtons.length - 1]!.click();
      assertMenuItemButtonsVisible({
        moveToTop: true,
        moveUp: true,
        moveDown: false,
      });
      actionMenu.close();
    });
  });
});
