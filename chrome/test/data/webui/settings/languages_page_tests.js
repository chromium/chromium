// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('languages_page_tests', function() {
  /** @enum {string} */
  const TestNames = {
    AddLanguagesDialog: 'add languages dialog',
    LanguageMenu: 'language menu',
    InputMethods: 'input methods',
    Spellcheck: 'spellcheck',
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
    /** @type {?settings.LanguagesBrowserProxy} */
    let browserProxy = null;

    // Enabled language pref name for the platform.
    const languagesPref = cr.isChromeOS ?
        'settings.language.preferred_languages' :
        'intl.accept_languages';

    // Initial value of enabled languages pref used in tests.
    const initialLanguages = 'en-US,sw';

    suiteSetup(function() {
      testing.Test.disableAnimationsAndTransitions();
      PolymerTest.clearBody();
      CrSettingsPrefs.deferInitialization = true;
    });

    setup(function() {
      const settingsPrefs = document.createElement('settings-prefs');
      const settingsPrivate =
          new settings.FakeSettingsPrivate(settings.getFakeLanguagePrefs());
      settingsPrefs.initialize(settingsPrivate);
      document.body.appendChild(settingsPrefs);
      return CrSettingsPrefs.initialized.then(function() {
        // Set up test browser proxy.
        browserProxy = new settings.TestLanguagesBrowserProxy();
        settings.LanguagesBrowserProxyImpl.instance_ = browserProxy;

        // Set up fake languageSettingsPrivate API.
        const languageSettingsPrivate =
            browserProxy.getLanguageSettingsPrivate();
        languageSettingsPrivate.setSettingsPrefs(settingsPrefs);

        languagesPage = document.createElement('settings-languages-page');

        // Prefs would normally be data-bound to settings-languages-page.
        languagesPage.prefs = settingsPrefs.prefs;
        test_util.fakeDataBind(settingsPrefs, languagesPage, 'prefs');

        document.body.appendChild(languagesPage);
        languagesCollapse = languagesPage.$.languagesCollapse;
        languagesCollapse.opened = true;
        actionMenu = languagesPage.$.menu.get();

        languageHelper = languagesPage.languageHelper;
        return languageHelper.whenReady();
      });
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    suite(TestNames.AddLanguagesDialog, function() {
      let dialog;
      let dialogItems;
      let cancelButton;
      let actionButton;
      let dialogClosedResolver;
      let dialogClosedObserver;

      // Resolves the PromiseResolver if the mutation includes removal of the
      // settings-add-languages-dialog.
      // TODO(michaelpg): Extract into a common method similar to
      // test_util.whenAttributeIs for use elsewhere.
      const onMutation = function(mutations, observer) {
        if (mutations.some(function(mutation) {
              return mutation.type == 'childList' &&
                  Array.from(mutation.removedNodes).includes(dialog);
            })) {
          // Sanity check: the dialog should no longer be in the DOM.
          assertEquals(null, languagesPage.$$('settings-add-languages-dialog'));
          observer.disconnect();
          assertTrue(!!dialogClosedResolver);
          dialogClosedResolver.resolve();
        }
      };

      setup(function(done) {
        const addLanguagesButton =
            languagesCollapse.querySelector('#addLanguages');
        const whenDialogOpen =
            test_util.eventToPromise('cr-dialog-open', languagesPage);
        addLanguagesButton.click();

        // The page stamps the dialog, registers listeners, and populates the
        // iron-list asynchronously at microtask timing, so wait for a new task.
        whenDialogOpen.then(() => {
          dialog = languagesPage.$$('settings-add-languages-dialog');
          assertTrue(!!dialog);

          // Observe the removal of the dialog via MutationObserver since the
          // HTMLDialogElement 'close' event fires at an unpredictable time.
          dialogClosedResolver = new PromiseResolver();
          dialogClosedObserver = new MutationObserver(onMutation);
          dialogClosedObserver.observe(languagesPage.root, {childList: true});

          actionButton = assert(dialog.$$('.action-button'));
          cancelButton = assert(dialog.$$('.cancel-button'));
          Polymer.dom.flush();

          // The fixed-height dialog's iron-list should stamp far fewer than
          // 50 items.
          dialogItems =
              dialog.$.dialog.querySelectorAll('.list-item:not([hidden])');
          assertGT(dialogItems.length, 1);
          assertLT(dialogItems.length, 50);

          // No languages have been checked, so the action button is disabled.
          assertTrue(actionButton.disabled);
          assertFalse(cancelButton.disabled);

          done();
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
        dialogItems[1].click();  // en-CA.
        dialogItems[2].click();  // tk.

        // Canceling the dialog should close and remove it without enabling
        // the checked languages.
        cancelButton.click();
        return dialogClosedResolver.promise.then(function() {
          assertEquals(
              initialLanguages, languageHelper.getPref(languagesPref).value);
        });
      });

      test('add languages and confirm', function() {
        // No languages have been checked, so the action button is inert.
        actionButton.click();
        Polymer.dom.flush();
        assertEquals(dialog, languagesPage.$$('settings-add-languages-dialog'));

        // Check and uncheck one language.
        dialogItems[0].click();
        assertFalse(actionButton.disabled);
        dialogItems[0].click();
        assertTrue(actionButton.disabled);

        // Check multiple languages.
        dialogItems[0].click();  // en.
        dialogItems[2].click();  // tk.
        assertFalse(actionButton.disabled);

        // The action button should close and remove the dialog, enabling the
        // checked languages.
        actionButton.click();

        assertEquals(
            initialLanguages + ',en,tk',
            languageHelper.getPref(languagesPref).value);

        return dialogClosedResolver.promise;
      });

      // Test that searching languages works whether the displayed or native
      // language name is queried.
      test('search languages', function() {
        const searchInput = dialog.$$('settings-subpage-search');

        const getItems = function() {
          return dialog.$.dialog.querySelectorAll('.list-item:not([hidden])');
        };

        // Expecting a few languages to be displayed when no query exists.
        assertGE(getItems().length, 1);

        // Issue query that matches the |displayedName|.
        searchInput.setValue('greek');
        Polymer.dom.flush();
        assertEquals(1, getItems().length);

        // Issue query that matches the |nativeDisplayedName|.
        searchInput.setValue('Ελληνικά');
        Polymer.dom.flush();
        assertEquals(1, getItems().length);

        // Issue query that does not match any language.
        searchInput.setValue('egaugnal');
        Polymer.dom.flush();
        assertEquals(0, getItems().length);

        // Issue query that should never match any language.
        searchInput.setValue('_arc_ime_language_');
        Polymer.dom.flush();
        assertEquals(0, getItems().length);
      });

      test('Escape key behavior', function() {
        const searchInput = dialog.$$('settings-subpage-search');
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
        return assert(menuItem, 'Menu item "' + i18nKey + '" not found');
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
              'Menu item "' + buttonKey + '" hidden');
        }
      }

      test('structure', function() {
        const languageOptionsDropdownTrigger =
            languagesCollapse.querySelector('button');
        assertTrue(!!languageOptionsDropdownTrigger);
        languageOptionsDropdownTrigger.click();
        assertTrue(actionMenu.open);

        const separator = actionMenu.querySelector('hr');
        assertEquals(1, separator.offsetHeight);

        // Disable Translate. On platforms that can't change the UI language,
        // this hides all the checkboxes, so the separator isn't needed.
        // Chrome OS and Windows still show a checkbox and thus the separator.
        languageHelper.setPrefValue('translate.enabled', false);
        assertEquals(
            cr.isChromeOS || cr.isWindows ? 1 : 0, separator.offsetHeight);
      });

      test('test translate.enable toggle', function() {
        const settingsToggle = languagesPage.$.offerTranslateOtherLanguages;
        assertTrue(!!settingsToggle);
        assertTrue(!!settingsToggle);

        // Clicking on the toggle switches it to false.
        settingsToggle.click();
        let newToggleValue = languageHelper.prefs.translate.enabled.value;
        assertFalse(newToggleValue);

        // Clicking on the toggle switches it to true again.
        settingsToggle.click();
        newToggleValue = languageHelper.prefs.translate.enabled.value;
        assertTrue(newToggleValue);
      });

      test('toggle translate for a specific language', function(done) {
        // Open options for 'sw'.
        const languageOptionsDropdownTrigger =
            languagesCollapse.querySelectorAll('button')[1];
        assertTrue(!!languageOptionsDropdownTrigger);
        languageOptionsDropdownTrigger.click();
        assertTrue(actionMenu.open);

        // 'sw' supports translate to the target language ('en').
        const translateOption = getMenuItem('offerToTranslateInThisLanguage');
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
              languageHelper.prefs.translate_blocked_languages.value);
          done();
        }, settings.kMenuCloseDelay + 1);
      });

      test('disable translate hides language-specific option', function() {
        // Disables translate.
        languageHelper.setPrefValue('translate.enabled', false);

        // Open options for 'sw'.
        const languageOptionsDropdownTrigger =
            languagesCollapse.querySelectorAll('button')[1];
        assertTrue(!!languageOptionsDropdownTrigger);
        languageOptionsDropdownTrigger.click();
        assertTrue(actionMenu.open);

        // The language-specific translation option should be hidden.
        const translateOption = actionMenu.querySelector('#offerTranslations');
        assertTrue(!!translateOption);
        assertTrue(translateOption.hidden);
      });

      test('remove language', function() {
        // Enable a language which we can then disable.
        languageHelper.enableLanguage('no');

        // Populate the dom-repeat.
        Polymer.dom.flush();

        // Find the new language item.
        const items = languagesCollapse.querySelectorAll('.list-item');
        const domRepeat = assert(languagesCollapse.querySelector(
            Polymer.DomRepeat ? 'dom-repeat' : 'template[is="dom-repeat"]'));
        const item = Array.from(items).find(function(el) {
          return domRepeat.itemForElement(el) &&
              domRepeat.itemForElement(el).language.code == 'no';
        });

        // Open the menu and select Remove.
        item.querySelector('button').click();

        assertTrue(actionMenu.open);
        const removeMenuItem = getMenuItem('removeLanguage');
        assertFalse(removeMenuItem.disabled);
        removeMenuItem.click();
        assertFalse(actionMenu.open);

        assertEquals(
            initialLanguages, languageHelper.getPref(languagesPref).value);
      });

      test('move up/down buttons', function() {
        // Add several languages.
        for (const language of ['en-CA', 'en-US', 'tk', 'no'])
          languageHelper.enableLanguage(language);

        Polymer.dom.flush();

        const menuButtons = languagesCollapse.querySelectorAll(
            '.list-item paper-icon-button-light.icon-more-vert');

        // First language should not have "Move up" or "Move to top".
        menuButtons[0].querySelector('button').click();
        assertMenuItemButtonsVisible({
          moveToTop: false,
          moveUp: false,
          moveDown: true,
        });
        actionMenu.close();

        // Second language should not have "Move up".
        menuButtons[1].querySelector('button').click();
        assertMenuItemButtonsVisible({
          moveToTop: true,
          moveUp: false,
          moveDown: true,
        });
        actionMenu.close();

        // Middle languages should have all buttons.
        menuButtons[2].querySelector('button').click();
        assertMenuItemButtonsVisible({
          moveToTop: true,
          moveUp: true,
          moveDown: true,
        });
        actionMenu.close();

        // Last language should not have "Move down".
        menuButtons[menuButtons.length - 1].querySelector('button').click();
        assertMenuItemButtonsVisible({
          moveToTop: true,
          moveUp: true,
          moveDown: false,
        });
        actionMenu.close();
      });
    });

    test(TestNames.InputMethods, function() {
      const inputMethodsCollapse = languagesPage.$.inputMethodsCollapse;
      const inputMethodSettingsExist = !!inputMethodsCollapse;
      if (cr.isChromeOS) {
        assertTrue(inputMethodSettingsExist);
        const manageInputMethodsButton =
            inputMethodsCollapse.querySelector('#manageInputMethods');
        manageInputMethodsButton.click();
        assertTrue(!!languagesPage.$$('settings-manage-input-methods-page'));
      } else {
        assertFalse(inputMethodSettingsExist);
      }
    });

    suite(TestNames.Spellcheck, function() {
      test('structure', function() {
        const spellCheckCollapse = languagesPage.$.spellCheckCollapse;
        const spellCheckSettingsExist = !!spellCheckCollapse;
        if (cr.isMac) {
          assertFalse(spellCheckSettingsExist);
          return;
        }

        assertTrue(spellCheckSettingsExist);

        // The row button should have a secondary row specifying which
        // language spell check is enabled for.
        const triggerRow = languagesPage.$.spellCheckSubpageTrigger;

        // en-US starts with spellcheck enabled, so the secondary row is
        // populated.
        assertTrue(triggerRow.classList.contains('two-line'));
        assertLT(0, triggerRow.querySelector('.secondary').textContent.length);

        triggerRow.click();
        Polymer.dom.flush();

        // Disable spellcheck for en-US.
        const spellcheckLanguageToggle =
            spellCheckCollapse.querySelector('cr-toggle[checked]');
        assertTrue(!!spellcheckLanguageToggle);
        spellcheckLanguageToggle.click();
        assertFalse(spellcheckLanguageToggle.checked);
        assertEquals(
            0, languageHelper.prefs.spellcheck.dictionaries.value.length);

        // Now the secondary row is empty, so it shouldn't be shown.
        assertFalse(triggerRow.classList.contains('two-line'));
        assertEquals(
            0, triggerRow.querySelector('.secondary').textContent.length);

        // Force-enable a language via policy.
        languageHelper.setPrefValue('spellcheck.forced_dictionaries', ['nb']);

        // The second row should no longer be empty.
        assertTrue(triggerRow.classList.contains('two-line'));
        assertLT(0, triggerRow.querySelector('.secondary').textContent.length);

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
        Polymer.dom.flush();

        // The second row should not be empty.
        assertTrue(triggerRow.classList.contains('two-line'));
        assertLT(0, triggerRow.querySelector('.secondary').textContent.length);

        // The policy indicator should be present.
        assertTrue(!!triggerRow.querySelector('cr-policy-pref-indicator'));

        // Force-enable spellchecking via policy, and ensure that the policy
        // indicator is not present. |enable_spellchecking| can be forced to
        // true by policy, but no indicator should be shown in that case.
        setEnableSpellcheckingViaPolicy(true);
        Polymer.dom.flush();
        assertFalse(!!triggerRow.querySelector('cr-policy-pref-indicator'));
      });

      test('error handling', function() {
        if (cr.isMac)
          return;

        const checkAllHidden = nodes => {
          assertTrue(nodes.every(node => node.hidden));
        };

        const languageSettingsPrivate =
            browserProxy.getLanguageSettingsPrivate();
        const spellCheckCollapse = languagesPage.$.spellCheckCollapse;
        const errorDivs = Array.from(
            spellCheckCollapse.querySelectorAll('.name-with-error-list div'));
        assertEquals(4, errorDivs.length);
        checkAllHidden(errorDivs);

        const retryButtons =
            Array.from(spellCheckCollapse.querySelectorAll('paper-button'));
        assertEquals(2, retryButtons.length);
        checkAllHidden(retryButtons);

        const languageCode =
            languagesPage.get('languages.enabled.0.language.code');
        languageSettingsPrivate.onSpellcheckDictionariesChanged.callListeners([
          {languageCode, isReady: false, downloadFailed: true},
        ]);

        Polymer.dom.flush();
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
        Polymer.dom.flush();
        assertTrue(moreInfo.hidden);

        retryButtons[0].click();
        Polymer.dom.flush();
        assertFalse(moreInfo.hidden);
      });
    });
  });

  return {TestNames: TestNames};
});
