// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LanguageHelper, LanguagesBrowserProxyImpl, LanguagesMetricsProxyImpl, LanguagesPageInteraction, LanguageState, LifetimeBrowserProxyImpl, OsSettingsChangeDeviceLanguageDialogElement, OsSettingsLanguagesPageV2Element, SettingsLanguagesElement} from 'chrome://os-settings/lazy_load.js';
import {CrActionMenuElement, CrCheckboxElement, CrLinkRowElement, CrPolicyIndicatorElement, CrSettingsPrefs, Router, routes, settingMojom, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGE, assertGT, assertLT, assertNull, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {fakeDataBind, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeLanguageSettingsPrivate, getFakeLanguagePrefs} from '../fake_language_settings_private.js';
import {TestLifetimeBrowserProxy} from '../test_os_lifetime_browser_proxy.js';

import {TestLanguagesBrowserProxy} from './test_os_languages_browser_proxy.js';
import {TestLanguagesMetricsProxy} from './test_os_languages_metrics_proxy.js';

suite('<os-settings-languages-page-v2>', () => {
  let languageHelper: LanguageHelper;
  let languagesPage: OsSettingsLanguagesPageV2Element;
  let languagesList: HTMLElement;
  let actionMenu: CrActionMenuElement;
  let browserProxy: TestLanguagesBrowserProxy;
  let lifetimeProxy: TestLifetimeBrowserProxy;
  let metricsProxy: TestLanguagesMetricsProxy;
  let languageSettingsPrivate: FakeLanguageSettingsPrivate;
  let settingsLanguages: SettingsLanguagesElement;
  let settingsPrefs: SettingsPrefsElement;

  // Initial value of enabled languages pref used in tests.
  const INITIAL_LANGUAGES = 'en-US,sw';

  suiteSetup(() => {
    CrSettingsPrefs.deferInitialization = true;

    // Sets up test browser proxy.
    browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstanceForTesting(browserProxy);

    lifetimeProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeProxy);

    // Sets up test metrics proxy.
    metricsProxy = new TestLanguagesMetricsProxy();
    LanguagesMetricsProxyImpl.setInstanceForTesting(metricsProxy);
  });

  setup(async () => {
    assert(window.trustedTypes);
    document.body.innerHTML =
        window.trustedTypes.emptyHTML as unknown as string;

    settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
    settingsPrefs.initialize(settingsPrivate);
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    // Sets up fake languageSettingsPrivate API.
    languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate() as
        unknown as FakeLanguageSettingsPrivate;
    languageSettingsPrivate.setSettingsPrefsForTesting(settingsPrefs);

    // Instantiates the data model with data bindings for prefs.
    settingsLanguages = document.createElement('settings-languages');
    settingsLanguages.prefs = settingsPrefs.prefs;
    fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
    document.body.appendChild(settingsLanguages);

    // Creates page with data bindings for prefs and data model.
    languagesPage = document.createElement('os-settings-languages-page-v2');
    languagesPage.prefs = settingsPrefs.prefs;
    fakeDataBind(settingsPrefs, languagesPage, 'prefs');
    languagesPage.languages = settingsLanguages.languages;
    fakeDataBind(settingsLanguages, languagesPage, 'languages');
    languagesPage.languageHelper = settingsLanguages.languageHelper;
    fakeDataBind(settingsLanguages, languagesPage, 'language-helper');
    document.body.appendChild(languagesPage);

    const element =
        languagesPage.shadowRoot!.querySelector<HTMLElement>('#languagesList');
    assertTrue(!!element);
    languagesList = element;
    actionMenu = languagesPage.$.menu.get();

    languageHelper = languagesPage.languageHelper;
    await languageHelper.whenReady();
  });

  teardown(() => {
    languagesPage.remove();
    settingsLanguages.remove();
    settingsPrefs.remove();

    browserProxy.reset();
    lifetimeProxy.reset();
    metricsProxy.reset();
    languageSettingsPrivate.reset();

    Router.getInstance().resetRouteForTesting();
  });

  suite('language menu', () => {
    /*
     * Finds, asserts and returns the menu item for the given i18n key.
     * @param i18nKey Name of the i18n string for the item's text.
     * @return Menu item.
     */
    function getMenuItem(i18nKey: string): HTMLButtonElement|CrCheckboxElement {
      const i18nString = loadTimeData.getString(i18nKey);
      assertTrue(!!i18nString);
      const menuItems =
          actionMenu.querySelectorAll<HTMLButtonElement|CrCheckboxElement>(
              '.dropdown-item');
      const menuItem = Array.from(menuItems).find(
          (item: HTMLButtonElement|CrCheckboxElement) =>
              item.textContent?.trim() === i18nString);
      assertTrue(!!menuItem, `Menu item "${i18nKey}" not found`);
      return menuItem;
    }

    /*
     * Checks the visibility of each expected menu item button.
     * param {!Object<boolean>} Dictionary from i18n keys to expected
     *     visibility of those menu items.
     */
    function assertMenuItemButtonsVisible(
        buttonVisibility: {[key: string]: boolean}): void {
      assertTrue(actionMenu.open);
      for (const buttonKey of Object.keys(buttonVisibility)) {
        const buttonItem = getMenuItem(buttonKey);
        assertEquals(
            !buttonVisibility[buttonKey], buttonItem.hidden,
            `Menu item "${buttonKey}" hidden`);
      }
    }

    test('removes language when starting with 3 languages', () => {
      // Enables a language which we can then disable.
      languageHelper.enableLanguage('no');

      // Populates the dom-repeat.
      flush();

      // Finds the new language item.
      const items = languagesList.querySelectorAll<HTMLElement>('.list-item');
      const domRepeat = languagesList.querySelector('dom-repeat');
      assertTrue(!!domRepeat);
      const item = Array.from(items).find(
          (el) => domRepeat.itemForElement(el) &&
              (domRepeat.itemForElement(el).language.code === 'no'));
      assertTrue(!!item);

      // Opens the menu and selects Remove.
      const button = item.querySelector('cr-icon-button');
      assertTrue(!!button);
      button.click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem('removeLanguage');
      assertFalse(removeMenuItem.disabled);
      assertFalse(removeMenuItem.hidden);
      removeMenuItem.click();
      assertFalse(actionMenu.open);

      assertEquals(
          INITIAL_LANGUAGES,
          languagesPage.getPref('intl.accept_languages').value);
    });

    test('removes language when starting with 2 languages', () => {
      const items = languagesList.querySelectorAll<HTMLElement>('.list-item');
      const domRepeat = languagesList.querySelector('dom-repeat');
      assertTrue(!!domRepeat);
      const item = Array.from(items).find(
          (el) => domRepeat.itemForElement(el) &&
              domRepeat.itemForElement(el).language.code === 'sw');
      assertTrue(!!item);

      // Opens the menu and selects Remove.
      const button = item.querySelector('cr-icon-button');
      assertTrue(!!button);
      button.click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem('removeLanguage');
      assertFalse(removeMenuItem.disabled);
      assertFalse(removeMenuItem.hidden);
      removeMenuItem.click();
      assertFalse(actionMenu.open);

      assertEquals(
          'en-US', languagesPage.getPref('intl.accept_languages').value);
    });

    test('the only translate blocked language is not removable', () => {
      //'en-US' is preconfigured to be the only translate blocked language.
      assertDeepEquals(
          ['en-US'], languagesPage.prefs.translate_blocked_languages.value);
      const items = languagesList.querySelectorAll<HTMLElement>('.list-item');
      const domRepeat = languagesList.querySelector('dom-repeat');
      assertTrue(!!domRepeat);
      const item = Array.from(items).find(
          (el) => domRepeat.itemForElement(el) &&
              domRepeat.itemForElement(el).language.code === 'en-US');
      assertTrue(!!item);

      // Opens the menu and selects Remove.
      const button = item.querySelector('cr-icon-button');
      assertTrue(!!button);
      button.click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem('removeLanguage');
      assertTrue(removeMenuItem.disabled);
      assertFalse(removeMenuItem.hidden);
    });

    test('device language is removable', () => {
      // 'en-US' is the preconfigured UI language.
      assertEquals(
          'en-US', languagesPage.get('languages.prospectiveUILanguage'));
      // Add 'sw' to translate_blocked_languages.
      languagesPage.setPrefValue(
          'translate_blocked_languages', ['en-US', 'sw']);
      flush();

      const items = languagesList.querySelectorAll<HTMLElement>('.list-item');
      const domRepeat = languagesList.querySelector('dom-repeat');
      assertTrue(!!domRepeat);
      const item = Array.from(items).find(
          (el) => domRepeat.itemForElement(el) &&
              domRepeat.itemForElement(el).language.code === 'en-US');
      assertTrue(!!item);

      // Opens the menu and selects Remove.
      const button = item.querySelector('cr-icon-button');
      assertTrue(!!button);
      button.click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem('removeLanguage');
      assertFalse(removeMenuItem.disabled);
      assertFalse(removeMenuItem.hidden);
      removeMenuItem.click();
      assertFalse(actionMenu.open);

      assertEquals('sw', languagesPage.getPref('intl.accept_languages').value);
    });

    test('single preferred language is not removable', () => {
      languagesPage.setPrefValue('intl.accept_languages', 'sw');
      languagesPage.setPrefValue('settings.language.preferred_languages', 'sw');
      flush();
      const items = languagesList.querySelectorAll<HTMLElement>('.list-item');
      const domRepeat = languagesList.querySelector('dom-repeat');
      assertTrue(!!domRepeat);
      const item = Array.from(items).find(
          (el) => domRepeat.itemForElement(el) &&
              domRepeat.itemForElement(el).language.code === 'sw');
      assertTrue(!!item);

      // Opens the menu and selects Remove.
      const button = item.querySelector('cr-icon-button');
      assertTrue(!!button);
      button.click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem('removeLanguage');
      assertTrue(removeMenuItem.disabled);
      assertFalse(removeMenuItem.hidden);
    });

    test('removing a language does not remove related input methods', () => {
      const sw = '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:sw:sw';
      const swUS = 'ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:sw';
      languageHelper.addInputMethod(sw);
      languageHelper.addInputMethod(swUS);
      assertEquals(
          4, languagesPage.get('languages.inputMethods.enabled').length);

      // Disable Swahili. The Swahili-only keyboard should not be removed.
      languageHelper.disableLanguage('sw');
      assertEquals(
          4, languagesPage.get('languages.inputMethods.enabled').length);
    });

    test('has move up/down buttons', () => {
      // Adds several languages.
      for (const language of ['en-CA', 'en-US', 'tk', 'no']) {
        languageHelper.enableLanguage(language);
      }

      flush();

      const menuButtons = languagesList.querySelectorAll<HTMLButtonElement>(
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

    test('test translate target language is labelled', () => {
      const targetLanguageCode = languagesPage.get('languages.translateTarget');
      assertTrue(!!targetLanguageCode);

      // Add 'en' to have more than one translate-target language.
      languageHelper.enableLanguage('en');
      const isTranslateTarget = (languageState: LanguageState) =>
          languageHelper.convertLanguageCodeForTranslate(
              languageState.language.code) === targetLanguageCode;
      const translateTargets =
          languagesPage.get('languages.enabled').filter(isTranslateTarget);
      assertGT(translateTargets.length, 1);
      // Ensure there is at least one non-translate-target language.
      assertLT(
          translateTargets.length,
          languagesPage.get('languages.enabled').length);

      const listItems =
          languagesList.querySelectorAll<HTMLElement>('.list-item');
      const domRepeat = languagesList.querySelector('dom-repeat');
      assertTrue(!!domRepeat);

      let translateTargetLabel;
      let item;
      let numVisibles = 0;
      Array.from(listItems).forEach(el => {
        item = domRepeat.itemForElement(el);
        if (item) {
          translateTargetLabel = el.querySelector('.target-info');
          assertTrue(!!translateTargetLabel);
          if (getComputedStyle(translateTargetLabel).display !== 'none') {
            numVisibles++;
            assertEquals(
                targetLanguageCode,
                languageHelper.convertLanguageCodeForTranslate(
                    item.language.code));
          }
        }
        assertEquals(
            1, numVisibles,
            'Not exactly one target info label (' + numVisibles + ').');
      });
    });

    test('toggle translate checkbox for a language', async () => {
      // Open options for 'sw'.
      const languageOptionsDropdownTrigger =
          languagesList.querySelectorAll('cr-icon-button')[1];
      assertTrue(!!languageOptionsDropdownTrigger);
      languageOptionsDropdownTrigger.click();
      assertTrue(actionMenu.open);

      // 'sw' supports translate to the target language ('en').
      const translateOption =
          getMenuItem('offerToTranslateThisLanguage') as CrCheckboxElement;
      assertFalse(translateOption.disabled);
      assertTrue(translateOption.checked);

      // Toggle the translate option.
      translateOption.click();
      assertFalse(
          await metricsProxy.whenCalled('recordTranslateCheckboxChanged'));
      assertDeepEquals(
          ['en-US', 'sw'],
          languagesPage.prefs.translate_blocked_languages.value);

      // Menu should stay open briefly.
      assertTrue(actionMenu.open);

      // Menu closes after delay
      const kMenuCloseDelay = 100;
      await new Promise(r => setTimeout(r, kMenuCloseDelay + 1));
      assertFalse(actionMenu.open);
    });

    test('translate checkbox disabled for translate blocked language', () => {
      // Open options for 'en-US'.
      const languageOptionsDropdownTrigger =
          languagesList.querySelectorAll('cr-icon-button')[0];
      assertTrue(!!languageOptionsDropdownTrigger);
      languageOptionsDropdownTrigger.click();
      assertTrue(actionMenu.open);

      // 'en-US' does not support checkbox.
      const translateOption = getMenuItem('offerToTranslateThisLanguage');
      assertTrue(translateOption.disabled);
    });

    test('disable translate hides language-specific option', () => {
      // Disables translate.
      languagesPage.setPrefValue('translate.enabled', false);

      // Open options for 'sw'.
      const languageOptionsDropdownTrigger =
          languagesList.querySelectorAll('cr-icon-button')[1];
      assertTrue(!!languageOptionsDropdownTrigger);
      languageOptionsDropdownTrigger.click();
      assertTrue(actionMenu.open);

      // The language-specific translation option should be hidden.
      const translateOption =
          actionMenu.querySelector<HTMLElement>('#offerTranslations');
      assertTrue(!!translateOption);
      assertTrue(translateOption.hidden);
    });

    test('Deep link to add language', async () => {
      const params = new URLSearchParams();
      params.append('settingId', settingMojom.Setting.kAddLanguage.toString());
      Router.getInstance().navigateTo(routes.OS_LANGUAGES_LANGUAGES, params);

      flush();

      const deepLinkElement =
          languagesPage.shadowRoot!.querySelector<HTMLElement>('#addLanguages');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Add language button should be focused for settingId=1200.');
    });
  });

  suite('change device language dialog', () => {
    let dialog: OsSettingsChangeDeviceLanguageDialogElement;
    let dialogItems: NodeListOf<HTMLElement>;
    let cancelButton: HTMLButtonElement;
    let actionButton: HTMLButtonElement;

    /**
     * Returns the list items in the dialog.
     */
    function getListItems(): Element[] {
      // If an element (the <iron-list> in this case) is hidden in Polymer,
      // Polymer will intelligently not update the DOM of the hidden element
      // to prevent DOM updates that the user can't see. However, this means
      // that when the <iron-list> is hidden (due to no results), the list
      // items still exist in the DOM.
      // This function should return the *visible* items that the user can
      // select, so if the <iron-list> is hidden we should return an empty
      // list instead.
      const dialogEl = dialog.$.dialog;
      const list = dialogEl.querySelector('iron-list');
      if (list && list.hidden) {
        return [];
      }
      return [...dialogEl.querySelectorAll('.list-item:not([hidden])')];
    }

    setup(() => {
      assertNull(languagesPage.shadowRoot!.querySelector(
          'os-settings-change-device-language-dialog'));
      const crButton =
          languagesPage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#changeDeviceLanguage');
      assertTrue(!!crButton);
      crButton.click();
      flush();

      const element = languagesPage.shadowRoot!.querySelector(
          'os-settings-change-device-language-dialog');
      assertTrue(!!element);
      dialog = element;

      const actionBtn =
          dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
      assertTrue(!!actionBtn);
      actionButton = actionBtn;
      const cancelBtn =
          dialog.shadowRoot!.querySelector<HTMLButtonElement>('.cancel-button');
      assertTrue(!!cancelBtn);
      cancelButton = cancelBtn;

      // The fixed-height dialog's iron-list should stamp far fewer than
      // 50 items.
      dialogItems =
          dialog.$.dialog.querySelectorAll('.list-item:not([hidden])');
      assertGT(dialogItems.length, 1);
      assertLT(dialogItems.length, 50);

      // No language has been selected, so the action button is disabled.
      assertTrue(actionButton.disabled);
      assertFalse(cancelButton.disabled);
    });

    teardown(() => {
      languagesPage.remove();
      dialog.remove();
    });

    test('has action button working correctly', () => {
      // selecting a language enables action button
      dialogItems[0]!.click();
      assertFalse(actionButton.disabled);

      // selecting the same language again disables action button
      dialogItems[0]!.click();
      assertTrue(actionButton.disabled);
    });

    test('setting device language restarts device', async () => {
      // selects a language
      dialogItems[0]!.click();  // en-CA
      assertFalse(actionButton.disabled);

      actionButton.click();
      assertEquals(
          'en-CA', await browserProxy.whenCalled('setProspectiveUiLanguage'));
      assertEquals(
          LanguagesPageInteraction.RESTART,
          await metricsProxy.whenCalled('recordInteraction'));
      await lifetimeProxy.whenCalled('signOutAndRestart');
    });

    test(
        'setting device language adds it to front of enabled language if not present',
        async () => {
          languagesPage.setPrefValue('intl.accept_languages', 'en-US,sw');
          languagesPage.setPrefValue(
              'settings.language.preferred_languages', 'en-US,sw');
          // selects a language
          dialogItems[0]!.click();  // en-CA
          assertFalse(actionButton.disabled);

          actionButton.click();
          assertEquals(
              'en-CA',
              await browserProxy.whenCalled('setProspectiveUiLanguage'));
          assertTrue(languagesPage.getPref('intl.accept_languages')
                         .value.startsWith('en-CA'));
        });

    test(
        'setting device language moves already enabled language to front',
        async () => {
          languagesPage.setPrefValue('intl.accept_languages', 'en-US,sw,en-CA');
          languagesPage.setPrefValue(
              'settings.language.preferred_languages', 'en-US,sw,en-CA');
          flush();

          // selects a language
          dialogItems[0]!.click();  // en-CA
          assertFalse(actionButton.disabled);

          actionButton.click();
          assertEquals(
              'en-CA',
              await browserProxy.whenCalled('setProspectiveUiLanguage'));
          assertTrue(languagesPage.getPref('intl.accept_languages')
                         .value.startsWith('en-CA'));
        });

    // Test that searching languages works whether the displayed or native
    // language name is queried.
    test('searches languages', () => {
      const searchInput = dialog.shadowRoot!.querySelector('cr-search-field');
      assertTrue(!!searchInput);

      // Expecting a few languages to be displayed when no query exists.
      assertGE(getListItems().length, 1);

      // Issue query that matches the |displayedName| in lowercase.
      searchInput.setValue('greek');
      flush();
      assertEquals(1, getListItems().length);
      assertStringContains(getListItems()[0]!.textContent!, 'Greek');

      // Issue query that matches the |nativeDisplayedName|.
      searchInput.setValue('Ελληνικά');
      flush();
      assertEquals(1, getListItems().length);

      // Issue query that does not match any language.
      searchInput.setValue('egaugnal');
      flush();
      assertEquals(0, getListItems().length);
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

    test('languages are sorted on native display name', () => {
      // See https://crbug.com/1184064 for more details.
      // We can't test whether the order is *deterministic* w.r.t. device
      // language, as changing device language is not possible in a test, so we
      // do the next best thing and check if it's sorted on native display name.

      function getNativeDisplayName(text: string): string {
        return text.includes(' - ') ? text.split(' - ')[0]! : text;
      }

      const items = getListItems();
      const nativeDisplayNames =
          items.map(item => getNativeDisplayName(item.textContent!.trim()));

      const sortedNativeDisplayNames =
          [...nativeDisplayNames].sort((a, b) => a.localeCompare(b, 'en'));
      assertDeepEquals(nativeDisplayNames, sortedNativeDisplayNames);
    });
  });

  suite('records metrics', () => {
    test('when adding languages', async () => {
      const button = languagesPage.shadowRoot!.querySelector<HTMLButtonElement>(
          '#addLanguages');
      assertTrue(!!button);
      button.click();
      flush();
      await metricsProxy.whenCalled('recordAddLanguages');
    });

    test('when disabling translate.enable toggle', async () => {
      languagesPage.setPrefValue('translate.enabled', true);
      const toggle = languagesPage.shadowRoot!.querySelector<HTMLButtonElement>(
          '#offerTranslation');
      assertTrue(!!toggle);
      toggle.click();
      flush();

      assertFalse(await metricsProxy.whenCalled('recordToggleTranslate'));
    });

    test('when enabling translate.enable toggle', async () => {
      languagesPage.setPrefValue('translate.enabled', false);
      const toggle = languagesPage.shadowRoot!.querySelector<HTMLButtonElement>(
          '#offerTranslation');
      assertTrue(!!toggle);
      toggle.click();
      flush();

      assertTrue(await metricsProxy.whenCalled('recordToggleTranslate'));
    });

    test('when clicking on Manage Google Account language', async () => {
      // The below would normally create a new window using `window.open`, which
      // would change the focus from this test to the new window.
      // Prevent this from happening by overriding `window.open`.

      window.open = () => {
        return null;
      };

      const button = languagesPage.shadowRoot!.querySelector<HTMLButtonElement>(
          '#manageGoogleAccountLanguage');
      assertTrue(!!button);
      button.click();
      flush();
      assertEquals(
          LanguagesPageInteraction.OPEN_MANAGE_GOOGLE_ACCOUNT_LANGUAGE,
          await metricsProxy.whenCalled('recordInteraction'));
    });

    test('when clicking on "learn more" about web languages U2', async () => {
      const link =
          languagesPage.shadowRoot!.querySelector('#webLanguagesDescription');
      assertTrue(!!link);
      const anchor = link.shadowRoot!.querySelector('a');
      assertTrue(!!anchor);
      // The below would normally create a new window, which would change the
      // focus from this test to the new window.
      // Prevent this from happening by adding an event listener on the anchor
      // element which stops the default behaviour (of opening a new window).
      anchor.addEventListener('click', (e: Event) => e.preventDefault());
      anchor.click();
      assertEquals(
          LanguagesPageInteraction.OPEN_WEB_LANGUAGES_LEARN_MORE,
          await metricsProxy.whenCalled('recordInteraction'));
    });
  });
});

suite('change device language button', () => {
  let page: OsSettingsLanguagesPageV2Element;

  function createPage(): void {
    page = document.createElement('os-settings-languages-page-v2');
    document.body.appendChild(page);
    flush();
  }

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML =
        window.trustedTypes.emptyHTML as unknown as string;
  });

  teardown(() => {
    page.remove();
  });

  test('is hidden for guest users', () => {
    loadTimeData.overrideValues({
      isGuest: true,
    });
    createPage();

    assertNull(page.shadowRoot!.querySelector('#changeDeviceLanguage'));
    assertNull(
        page.shadowRoot!.querySelector('#changeDeviceLanguagePolicyIndicator'));
  });

  test('is disabled for secondary users', () => {
    loadTimeData.overrideValues(
        {isGuest: false, isSecondaryUser: true, primaryUserEmail: 'test.com'});
    createPage();

    const changeDeviceLanguageButton =
        page.shadowRoot!.querySelector<HTMLButtonElement>(
            '#changeDeviceLanguage');
    assertTrue(!!changeDeviceLanguageButton);
    assertTrue(changeDeviceLanguageButton.disabled);
    assertFalse(changeDeviceLanguageButton.hidden);

    const changeDeviceLanguagePolicyIndicator =
        page.shadowRoot!.querySelector<CrPolicyIndicatorElement>(
            '#changeDeviceLanguagePolicyIndicator');
    assertTrue(!!changeDeviceLanguagePolicyIndicator);
    assertFalse(changeDeviceLanguagePolicyIndicator.hidden);
    assertEquals(
        'test.com', changeDeviceLanguagePolicyIndicator.indicatorSourceName);
  });

  test('is enabled for primary users', () => {
    loadTimeData.overrideValues({
      isGuest: false,
      isSecondaryUser: false,
    });
    createPage();

    const changeDeviceLanguageButton =
        page.shadowRoot!.querySelector<HTMLButtonElement>(
            '#changeDeviceLanguage');
    assertTrue(!!changeDeviceLanguageButton);
    assertFalse(changeDeviceLanguageButton.disabled);
    assertFalse(changeDeviceLanguageButton.hidden);

    assertNull(
        page.shadowRoot!.querySelector('#changeDeviceLanguagePolicyIndicator'));
  });

  suite('app languages settings', () => {
    let page: OsSettingsLanguagesPageV2Element;

    function createPage(): void {
      page = document.createElement('os-settings-languages-page-v2');
      document.body.appendChild(page);
      flush();
    }

    setup(() => {
      assert(window.trustedTypes);
      document.body.innerHTML =
          window.trustedTypes.emptyHTML as unknown as string;
    });

    teardown(() => {
      page.remove();
    });

    test('Enable perAppLanguage flag, show app languages section', () => {
      loadTimeData.overrideValues({
        isPerAppLanguageEnabled: true,
      });
      createPage();
      const appLanguagesSection =
          page.shadowRoot!.querySelector<CrLinkRowElement>(
              '#appLanguagesSection');
      assertTrue(
          isVisible(appLanguagesSection),
          '#appLanguagesSection is not visible.');
    });

    test('Disable perAppLanguage flag, hide app languages section', () => {
      loadTimeData.overrideValues({
        isPerAppLanguageEnabled: false,
      });
      createPage();
      const appLanguagesSection =
          page.shadowRoot!.querySelector<CrLinkRowElement>(
              '#appLanguagesSection');
      assertFalse(
          isVisible(appLanguagesSection),
          '#appLanguagesSection is not hidden.');
    });
  });
});
