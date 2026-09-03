// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrCheckboxElement, LanguageHelper, SettingsAddLanguagesDialogElement, SettingsLanguagesPageElement} from 'chrome://settings/lazy_load.js';
import {LanguageHelperImpl, LanguagesBrowserProxyImpl, getLanguageHelperInstance} from 'chrome://settings/lazy_load.js';
import type {CrActionMenuElement, CrButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, convertLanguageCodeForTranslate, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertGE, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

// clang-format on

suite('LanguagesPage', function() {
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
        item => item.textContent.trim() === i18nString);
    assertTrue(!!menuItem, 'Menu item "' + i18nKey + '" not found');
    return menuItem;
  }

  // Initial value of enabled languages pref used in tests.
  const initialLanguages = 'en-US,sw';

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getFakeLanguagePrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    // Set up test browser proxy.
    browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstance(browserProxy);

    LanguageHelperImpl.resetInstanceForTesting();
    languageHelper = getLanguageHelperInstance();
    await languageHelper.whenReady();

    languagesPage = document.createElement('settings-languages-page');
    document.body.appendChild(languagesPage);
    flush();
    actionMenu = languagesPage.$.menu.get();
  });

  suite('AddLanguagesDialog', function() {
    let dialog: SettingsAddLanguagesDialogElement;
    let dialogItems: NodeListOf<CrCheckboxElement>;
    let addLanguagesButton: CrButtonElement;
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

    setup(async function() {
      addLanguagesButton =
          languagesPage.shadowRoot!.querySelector<CrButtonElement>(
              '#addLanguages')!;
      const whenDialogOpen = eventToPromise('cr-dialog-open', languagesPage);
      addLanguagesButton.click();

      // The page stamps the dialog, registers listeners, and populates the
      // DOM asynchronously at microtask timing, so wait for a new task.
      await whenDialogOpen;

      dialog = languagesPage.shadowRoot!.querySelector(
          'settings-add-languages-dialog')!;
      assertTrue(!!dialog);

      // Observe the removal of the dialog via MutationObserver since the
      // HTMLDialogElement 'close' event fires at an unpredictable time.
      dialogClosedResolver = new PromiseResolver();
      dialogClosedObserver = new MutationObserver(onMutation);
      dialogClosedObserver.observe(
          languagesPage.shadowRoot!.querySelector('settings-section')!,
          {childList: true});

      actionButton =
          dialog.shadowRoot.querySelector<CrButtonElement>('.action-button')!;
      assertTrue(!!actionButton);
      cancelButton =
          dialog.shadowRoot.querySelector<CrButtonElement>('.cancel-button')!;
      assertTrue(!!cancelButton);
      await microtasksFinished();

      dialogItems = dialog.shadowRoot.querySelectorAll<CrCheckboxElement>(
          'cr-checkbox:not([hidden])');
      assertGT(dialogItems.length, 1);

      // No languages have been checked, so the action button is disabled.
      assertTrue(actionButton.disabled);
      assertFalse(cancelButton.disabled);
    });

    teardown(function() {
      dialogClosedObserver.disconnect();
    });

    test('undefined languages', function() {
      assertFalse(addLanguagesButton.disabled);

      // Make the languages empty and make sure the button is disabled.
      languageHelper.dispatchEvent(new CustomEvent('languages-changed', {
        detail: Object.assign({}, languageHelper.languages, {
          supported: [],
        }),
      }));
      assertTrue(addLanguagesButton.disabled);
    });

    test('cancel', function() {
      // Canceling the dialog should close and remove it.
      cancelButton.click();

      return dialogClosedResolver.promise;
    });

    test('add languages and cancel', async function() {
      // Check some languages.
      dialogItems[1]!.click();  // en-CA.
      await microtasksFinished();
      dialogItems[2]!.click();  // tk.
      await microtasksFinished();

      // Canceling the dialog should close and remove it without enabling
      // the checked languages.
      cancelButton.click();
      await dialogClosedResolver.promise;
      assertEquals(
          initialLanguages,
          PrefService.getInstance().getPref('intl.accept_languages').value);
    });

    test('add languages and confirm', async function() {
      // No languages have been checked, so the action button is inert.
      actionButton.click();
      flush();
      assertEquals(
          dialog,
          languagesPage.shadowRoot!.querySelector(
              'settings-add-languages-dialog'));

      // Check and uncheck one language.
      dialogItems[0]!.click();
      await microtasksFinished();
      assertFalse(actionButton.disabled);
      dialogItems[0]!.click();
      await microtasksFinished();
      assertTrue(actionButton.disabled);

      // Check multiple languages.
      dialogItems[0]!.click();  // en.
      await microtasksFinished();
      dialogItems[2]!.click();  // tk.
      await microtasksFinished();
      assertFalse(actionButton.disabled);

      // The action button should close and remove the dialog, enabling the
      // checked languages.
      actionButton.click();

      assertEquals(
          initialLanguages + ',en,tk',
          PrefService.getInstance().getPref('intl.accept_languages').value);

      return dialogClosedResolver.promise;
    });

    // Test that searching languages works whether the displayed or native
    // language name is queried.
    test('search languages', async function() {
      const searchInput = dialog.shadowRoot.querySelector('cr-search-field');
      assertTrue(!!searchInput);

      const getItems = function() {
        return dialog.shadowRoot.querySelectorAll('cr-checkbox:not([hidden])');
      };

      // Expecting a few languages to be displayed when no query exists.
      assertGE(getItems().length, 1);

      // Issue query that matches the |displayedName|.
      searchInput.setValue('greek');
      await microtasksFinished();
      assertEquals(1, getItems().length);

      // Issue query that matches the |nativeDisplayedName|.
      searchInput.setValue('Ελληνικά');
      await microtasksFinished();
      assertEquals(1, getItems().length);

      // Issue query that does not match any language.
      searchInput.setValue('egaugnal');
      await microtasksFinished();
      assertEquals(0, getItems().length);

      // Issue query that should never match any language.
      searchInput.setValue('_arc_ime_language_');
      await microtasksFinished();
      assertEquals(0, getItems().length);
    });

    test('AddLanguagesDialogFocusgroup', function() {
      const list = dialog.shadowRoot.querySelector('#list')!;
      assertEquals('listbox block', list.getAttribute('focusgroup'));
    });

    test('Escape key behavior', function() {
      const searchInput = dialog.shadowRoot.querySelector('cr-search-field');
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

  suite('LanguageMenu', function() {
    /*
     * This suite tests that the translate target language is labelled
     */
    test('translate target language is labelled', function() {
      // Translate target language disabled.
      const targetLanguageCode = languageHelper.languages!.translateTarget;
      assertTrue(!!targetLanguageCode);
      assertTrue(languageHelper.languages!.enabled.some(
          l => convertLanguageCodeForTranslate(l.language.code) ===
              targetLanguageCode));
      assertTrue(languageHelper.languages!.enabled.some(
          l => convertLanguageCodeForTranslate(l.language.code) !==
              targetLanguageCode));
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
                convertLanguageCodeForTranslate(item.language.code));
          }
        }
        assertEquals(
            1, num_visibles,
            'Not exactly one target info label (' + num_visibles + ').');
      });
    });

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
          PrefService.getInstance().getPref('intl.accept_languages').value);
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
          'en-US',
          PrefService.getInstance().getPref('intl.accept_languages').value);
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
