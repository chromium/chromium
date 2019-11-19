// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('os_languages_page_tests', function() {
  /** @enum {string} */
  const TestNames = {
    // The "add languages" dialog is tested by browser settings.
    LanguageMenu: 'language menu',
    InputMethods: 'input methods',
  };

  suite('languages page', function() {
    /** @type {?LanguageHelper} */
    let languageHelper = null;
    /** @type {?SettingsLanguagesPageElement} */
    let languagesPage = null;
    /** @type {?HTMLElement} */
    let languagesList = null;
    /** @type {?CrActionMenuElement} */
    let actionMenu = null;
    /** @type {?settings.LanguagesBrowserProxy} */
    let browserProxy = null;

    // Enabled language pref name for the platform.
    const languagesPref = 'settings.language.preferred_languages';

    // Initial value of enabled languages pref used in tests.
    const initialLanguages = 'en-US,sw';

    suiteSetup(function() {
      testing.Test.disableAnimationsAndTransitions();
      PolymerTest.clearBody();
      CrSettingsPrefs.deferInitialization = true;
    });

    setup(async () => {
      const settingsPrefs = document.createElement('settings-prefs');
      const settingsPrivate =
          new settings.FakeSettingsPrivate(settings.getFakeLanguagePrefs());
      settingsPrefs.initialize(settingsPrivate);
      document.body.appendChild(settingsPrefs);
      await CrSettingsPrefs.initialized;
      // Set up test browser proxy.
      browserProxy = new settings.TestLanguagesBrowserProxy();
      settings.LanguagesBrowserProxyImpl.instance_ = browserProxy;

      // Set up fake languageSettingsPrivate API.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      languageSettingsPrivate.setSettingsPrefs(settingsPrefs);

      // Instantiate the data model with data bindings for prefs.
      const settingsLanguages = document.createElement('settings-languages');
      settingsLanguages.prefs = settingsPrefs.prefs;
      test_util.fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
      document.body.appendChild(settingsLanguages);

      // Create page with data bindings for prefs and data model.
      languagesPage = document.createElement('os-settings-languages-page');
      languagesPage.prefs = settingsPrefs.prefs;
      test_util.fakeDataBind(settingsPrefs, languagesPage, 'prefs');
      languagesPage.languages = settingsLanguages.languages;
      test_util.fakeDataBind(settingsLanguages, languagesPage, 'languages');
      languagesPage.languageHelper = settingsLanguages.languageHelper;
      test_util.fakeDataBind(
          settingsLanguages, languagesPage, 'language-helper');
      document.body.appendChild(languagesPage);

      languagesList = languagesPage.$.languagesList;
      actionMenu = languagesPage.$.menu.get();

      languageHelper = languagesPage.languageHelper;
      await languageHelper.whenReady();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    suite(TestNames.LanguageMenu, function() {
      /*
       * Finds, asserts and returns the menu item for the given i18n key.
       * @param {string} i18nKey Name of the i18n string for the item's text.
       * @return {!HTMLElement} Menu item.
       */
      function getMenuItem(i18nKey) {
        const i18nString = assert(loadTimeData.getString(i18nKey));
        const menuItems = actionMenu.querySelectorAll('.dropdown-item');
        const menuItem = Array.from(menuItems).find(
            item => item.textContent.trim() == i18nString);
        return assert(menuItem, `Menu item "${i18nKey}" not found`);
      }

      /*
       * Checks the visibility of each expected menu item button.
       * param {!Object<boolean>} Dictionary from i18n keys to expected
       *     visibility of those menu items.
       */
      function assertMenuItemButtonsVisible(buttonVisibility) {
        assertTrue(actionMenu.open);
        for (const buttonKey of Object.keys(buttonVisibility)) {
          const buttonItem = getMenuItem(buttonKey);
          assertEquals(
              !buttonVisibility[buttonKey], buttonItem.hidden,
              `Menu item "${buttonKey}" hidden`);
        }
      }

      /**
       * @return {HTMLElement} Traverses the DOM tree to find the lowest level
       *     active element.
       */
      function getActiveElement() {
        let node = document.activeElement;
        let lastNode;
        while (node) {
          lastNode = node;
          node = (node.shadowRoot || node).activeElement;
        }
        return lastNode;
      }

      /**
       * Assert whether the 'restart' button should be active.
       * @param {boolean} shouldBeActive True to assert that the 'restart'
       *     button is present and active or false the assert the negation.
       */
      function assertRestartButtonActiveState(shouldBeActive) {
        const activeElement = getActiveElement();
        isRestartButtonActive =
            activeElement && (activeElement.id == 'restartButton');
        assertEquals(isRestartButtonActive, shouldBeActive);
      }

      test('structure', function() {
        const languageOptionsDropdownTrigger =
            languagesList.querySelector('cr-icon-button');
        assertTrue(!!languageOptionsDropdownTrigger);
        languageOptionsDropdownTrigger.click();
        assertTrue(actionMenu.open);

        const separator = actionMenu.querySelector('hr');
        assertEquals(1, separator.offsetHeight);
      });

      test('changing UI language', function() {
        // Mock changing language.
        languageHelper.setProspectiveUILanguage = languageCode => {
          languagesPage.set('languages.prospectiveUILanguage', languageCode);
        };

        // Restart button is not active.
        assertRestartButtonActiveState(false);

        const swListItem = languagesList.querySelectorAll('.list-item')[1];
        // Open options for 'sw'.
        const languageOptionsDropdownTrigger =
            swListItem.querySelector('cr-icon-button');
        assertTrue(!!languageOptionsDropdownTrigger);
        // No restart button in 'sw' list-item.
        assertTrue(!swListItem.querySelector('#restartButton'));
        languageOptionsDropdownTrigger.click();
        assertTrue(actionMenu.open);

        // OS language is not 'sw'
        const uiLanguageOption = getMenuItem('displayInThisLanguage');
        assertFalse(uiLanguageOption.disabled);
        assertFalse(uiLanguageOption.checked);

        return new Promise(resolve => {
          actionMenu.addEventListener('close', () => {
            // Restart button is attached to the first list item and is active.
            const firstListItem =
                languagesList.querySelectorAll('.list-item')[0];
            const domRepeat = languagesList.querySelector('dom-repeat');
            assertEquals(
                'sw',
                domRepeat.modelForElement(firstListItem).item.language.code);
            assertTrue(!!firstListItem.querySelector('#restartButton'));
            assertRestartButtonActiveState(true);
            resolve();
          });

          // Change UI language.
          uiLanguageOption.click();
        });
      });

      test('remove language when starting with 3 languages', function() {
        // Enable a language which we can then disable.
        languageHelper.enableLanguage('no');

        // Populate the dom-repeat.
        Polymer.dom.flush();

        // Find the new language item.
        const items = languagesList.querySelectorAll('.list-item');
        const domRepeat = assert(languagesList.querySelector('dom-repeat'));
        const item = Array.from(items).find(function(el) {
          return domRepeat.itemForElement(el) &&
              domRepeat.itemForElement(el).language.code == 'no';
        });

        // Open the menu and select Remove.
        item.querySelector('cr-icon-button').click();

        assertTrue(actionMenu.open);
        const removeMenuItem = getMenuItem('removeLanguage');
        assertFalse(removeMenuItem.disabled);
        assertFalse(removeMenuItem.hidden);
        removeMenuItem.click();
        assertFalse(actionMenu.open);

        assertEquals(
            initialLanguages, languageHelper.getPref(languagesPref).value);
      });

      test('remove language when starting with 2 languages', function() {
        const items = languagesList.querySelectorAll('.list-item');
        const domRepeat = assert(languagesList.querySelector('dom-repeat'));
        const item = Array.from(items).find(function(el) {
          return domRepeat.itemForElement(el) &&
              domRepeat.itemForElement(el).language.code == 'sw';
        });

        // Open the menu and select Remove.
        item.querySelector('cr-icon-button').click();

        assertTrue(actionMenu.open);
        const removeMenuItem = getMenuItem('removeLanguage');
        assertFalse(removeMenuItem.disabled);
        assertFalse(removeMenuItem.hidden);
        removeMenuItem.click();
        assertFalse(actionMenu.open);

        assertEquals('en-US', languageHelper.getPref(languagesPref).value);
      });

      test('move up/down buttons', function() {
        // Add several languages.
        for (const language of ['en-CA', 'en-US', 'tk', 'no']) {
          languageHelper.enableLanguage(language);
        }

        Polymer.dom.flush();

        const menuButtons = languagesList.querySelectorAll(
            '.list-item cr-icon-button.icon-more-vert');

        // First language should not have "Move up" or "Move to top".
        menuButtons[0].click();
        assertMenuItemButtonsVisible({
          moveToTop: false,
          moveUp: false,
          moveDown: true,
        });
        actionMenu.close();

        // Second language should not have "Move up".
        menuButtons[1].click();
        assertMenuItemButtonsVisible({
          moveToTop: true,
          moveUp: false,
          moveDown: true,
        });
        actionMenu.close();

        // Middle languages should have all buttons.
        menuButtons[2].click();
        assertMenuItemButtonsVisible({
          moveToTop: true,
          moveUp: true,
          moveDown: true,
        });
        actionMenu.close();

        // Last language should not have "Move down".
        menuButtons[menuButtons.length - 1].click();
        assertMenuItemButtonsVisible({
          moveToTop: true,
          moveUp: true,
          moveDown: false,
        });
        actionMenu.close();
      });
    });

    test(TestNames.InputMethods, function() {
      const inputMethodsList = languagesPage.$.inputMethodsList;
      assertTrue(!!inputMethodsList);

      // The test input methods should appear.
      const items =
          inputMethodsList.querySelectorAll('.list-item .display-name');
      assertEquals(2, items.length);
      assertEquals('US keyboard', items[0].textContent.trim());
      assertEquals('US Dvorak keyboard', items[1].textContent.trim());

      const manageInputMethodsButton =
          inputMethodsList.querySelector('#manageInputMethods');
      assertTrue(!!manageInputMethodsButton);

      // settings-manage-input-methods-page is owned by os-languages-section,
      // not os-languages-page, and hence isn't tested here.
    });
  });

  return {TestNames: TestNames};
});
