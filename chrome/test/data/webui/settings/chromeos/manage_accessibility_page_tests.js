// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise, isVisible, waitAfterNextRender, waitBeforeNextRender} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

suite('ManageAccessibilityPageTests', function() {
  let page = null;
  let deviceBrowserProxy = null;

  /** @implements {DevicePageBrowserProxy} */
  class TestDevicePageBrowserProxy {
    constructor() {
      /** @private {boolean} */
      this.hasMouse_ = true;
      /** @private {boolean} */
      this.hasTouchpad_ = true;
    }

    /** @param {boolean} hasMouse */
    set hasMouse(hasMouse) {
      this.hasMouse_ = hasMouse;
      webUIListenerCallback('has-mouse-changed', this.hasMouse_);
    }

    /** @param {boolean} hasTouchpad */
    set hasTouchpad(hasTouchpad) {
      this.hasTouchpad_ = hasTouchpad;
      webUIListenerCallback('has-touchpad-changed', this.hasTouchpad_);
    }

    /** @override */
    initializePointers() {
      webUIListenerCallback('has-mouse-changed', this.hasMouse_);
      webUIListenerCallback('has-touchpad-changed', this.hasTouchpad_);
    }

    /** @override */
    initializeKeyboardWatcher() {
      webUIListenerCallback('has-hardware-keyboard', this.hasKeyboard_);
    }
  }

  function initPage(opt_prefs) {
    page = document.createElement('settings-manage-a11y-page');
    page.prefs = opt_prefs || getDefaultPrefs();
    document.body.appendChild(page);
  }

  function getDefaultPrefs() {
    return {
      'settings': {
        'a11y': {
          'tablet_mode_shelf_nav_buttons_enabled': {
            key: 'settings.a11y.tablet_mode_shelf_nav_buttons_enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          'dictation': {
            key: 'prefs.settings.a11y.dictation',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          },
          'dictation_locale': {
            key: 'prefs.settings.a11y.dictation_locale',
            type: chrome.settingsPrivate.PrefType.STRING,
            value: 'en-US',
          },
        },
        'accessibility': {
          key: 'settings.accessibility',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
    };
  }

  setup(function() {
    deviceBrowserProxy = new TestDevicePageBrowserProxy();
    DevicePageBrowserProxyImpl.setInstanceForTesting(deviceBrowserProxy);

    PolymerTest.clearBody();
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
    Router.getInstance().resetRouteForTesting();
  });

  test('Pointers row only visible if mouse/touchpad present', function() {
    initPage();
    const row = page.shadowRoot.querySelector('#pointerSubpageButton');
    assertFalse(row.hidden);

    // Has touchpad, doesn't have mouse ==> not hidden.
    deviceBrowserProxy.hasMouse = false;
    assertFalse(row.hidden);

    // Doesn't have either ==> hidden.
    deviceBrowserProxy.hasTouchpad = false;
    assertTrue(row.hidden);

    // Has mouse, doesn't have touchpad ==> not hidden.
    deviceBrowserProxy.hasMouse = true;
    assertFalse(row.hidden);

    // Has both ==> not hidden.
    deviceBrowserProxy.hasTouchpad = true;
    assertFalse(row.hidden);
  });

  test('tablet mode buttons visible', function() {
    loadTimeData.overrideValues({
      isKioskModeActive: false,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    initPage();
    flush();

    assertTrue(isVisible(page.shadowRoot.querySelector(
        '#shelfNavigationButtonsEnabledControl')));
  });

  test('toggle tablet mode buttons', function() {
    loadTimeData.overrideValues({
      isKioskModeActive: false,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    initPage();
    flush();

    const navButtonsToggle =
        page.shadowRoot.querySelector('#shelfNavigationButtonsEnabledControl');
    assertTrue(isVisible(navButtonsToggle));
    // The default pref value is false.
    assertFalse(navButtonsToggle.checked);

    // Clicking the toggle should update the toggle checked value, and the
    // backing preference.
    navButtonsToggle.click();
    flush();

    assertTrue(navButtonsToggle.checked);
    assertFalse(navButtonsToggle.disabled);
    assertTrue(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);

    navButtonsToggle.click();
    flush();

    assertFalse(navButtonsToggle.checked);
    assertFalse(navButtonsToggle.disabled);
    assertFalse(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);
  });

  test('tablet mode buttons toggle disabled with spoken feedback', function() {
    loadTimeData.overrideValues({
      isKioskModeActive: false,
      showTabletModeShelfNavigationButtonsSettings: true,
    });

    const prefs = getDefaultPrefs();
    // Enable spoken feedback.
    prefs.settings.accessibility.value = true;

    initPage(prefs);
    flush();

    const navButtonsToggle =
        page.shadowRoot.querySelector('#shelfNavigationButtonsEnabledControl');
    assertTrue(isVisible(navButtonsToggle));

    // If spoken feedback is enabled, the shelf nav buttons toggle should be
    // disabled and checked.
    assertTrue(navButtonsToggle.disabled);
    assertTrue(navButtonsToggle.checked);

    // Clicking the toggle should have no effect.
    navButtonsToggle.click();
    flush();

    assertTrue(navButtonsToggle.disabled);
    assertTrue(navButtonsToggle.checked);
    assertFalse(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);

    // The toggle should be enabled if the spoken feedback gets disabled.
    page.set('prefs.settings.accessibility.value', false);
    flush();

    assertFalse(!!navButtonsToggle.disabled);
    assertFalse(navButtonsToggle.checked);
    assertFalse(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);

    // Clicking the toggle should update the backing pref.
    navButtonsToggle.click();
    flush();

    assertFalse(!!navButtonsToggle.disabled);
    assertTrue(navButtonsToggle.checked);
    assertTrue(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);
  });

  test('some parts are hidden in kiosk mode', function() {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    initPage();
    // Add mouse and touchpad to show some hidden settings.
    deviceBrowserProxy.hasMouse = true;
    deviceBrowserProxy.hasTouchpad = true;
    flush();

    // Accessibility learn more link should be hidden.
    assertFalse(
        isVisible(page.shadowRoot.querySelector('setings-localized-link')));

    // Shelf navigation buttons are not shown in kiosk mode, even if
    // showTabletModeShelfNavigationButtonsSettings is true.
    assertFalse(isVisible(page.shadowRoot.querySelector(
        '#shelfNavigationButtonsEnabledControl')));

    const allowed_subpages = [
      'chromeVoxSubpageButton',
      'selectToSpeakSubpageButton',
      'ttsSubpageButton',
    ];

    const subpages = page.root.querySelectorAll('cr-link-row');
    subpages.forEach(function(subpage) {
      if (isVisible(subpage)) {
        assertTrue(allowed_subpages.includes(subpage.id));
      }
    });

    // Additional features link is not visible.
    assertFalse(isVisible(page.$.additionalFeaturesLink));
  });

  test('Deep link to switch access', async () => {
    loadTimeData.overrideValues({
      isKioskModeActive: false,
    });
    initPage();

    const params = new URLSearchParams();
    params.append('settingId', '1522');
    Router.getInstance().navigateTo(routes.MANAGE_ACCESSIBILITY, params);

    flush();

    const deepLinkElement = page.shadowRoot.querySelector('#enableSwitchAccess')
                                .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Switch access toggle should be focused for settingId=1522.');
  });

  test('Dictation labels', async () => {
    // Ensure that the Dictation locale menu is shown by setting the dictation
    // pref to true (done in default prefs) and populating dictation locale
    // options with mock data.
    initPage();
    const locales = [{
      name: 'English (United States)',
      worksOffline: true,
      installed: true,
      recommended: true,
      value: 'en-US',
    }];
    webUIListenerCallback('dictation-locales-set', locales);
    flush();

    // Dictation toggle.
    const dictationSetting = page.shadowRoot.querySelector('#enableDictation');
    assertTrue(!!dictationSetting);
    assertTrue(dictationSetting.checked);
    assertEquals('Dictation', dictationSetting.label);
    assertEquals(
        'Type with your voice. Use Search + D, then start speaking.',
        dictationSetting.subLabel);

    // Dictation locale menu.
    const dictationLocaleMenuLabel =
        page.shadowRoot.querySelector('#dictationLocaleMenuLabel');
    const dictationLocaleMenuSubtitle =
        page.shadowRoot.querySelector('#dictationLocaleMenuSubtitle');
    assertTrue(!!dictationLocaleMenuLabel);
    assertTrue(!!dictationLocaleMenuSubtitle);
    assertEquals('Language', dictationLocaleMenuLabel.innerText);
    assertEquals(
        'English (United States) is processed locally and works offline',
        dictationLocaleMenuSubtitle.innerText);

    // Fake a request to change the dictation locale menu subtitle.
    webUIListenerCallback('dictation-locale-menu-subtitle-changed', 'Testing');
    flush();

    // Only the dictation locale subtitle should have changed.
    assertEquals('Dictation', dictationSetting.label);
    assertEquals(
        'Type with your voice. Use Search + D, then start speaking.',
        dictationSetting.subLabel);
    assertEquals('Language', dictationLocaleMenuLabel.innerText);
    assertEquals('Testing', dictationLocaleMenuSubtitle.innerText);
  });

  test('Test computeDictationLocaleSubtitle_()', async () => {
    initPage();
    const locales = [
      {
        name: 'English (United States)',
        worksOffline: true,
        installed: true,
        recommended: true,
        value: 'en-US',
      },
      {
        name: 'Spanish',
        worksOffline: true,
        installed: false,
        recommended: false,
        value: 'es',
      },
      {
        name: 'German',
        // Note: this data should never occur in practice. If a locale isn't
        // supported offline, then it should never be installed. Test this case
        // to verify our code still works given unexpected input.
        worksOffline: false,
        installed: true,
        recommended: false,
        value: 'de',
      },
      {
        name: 'French (France)',
        worksOffline: false,
        installed: false,
        recommended: false,
        value: 'fr-FR',
      },
    ];
    webUIListenerCallback('dictation-locales-set', locales);
    page.dictationLocaleSubtitleOverride_ = 'Testing';
    flush();
    assertEquals(
        'English (United States) is processed locally and works offline',
        page.computeDictationLocaleSubtitle_());

    // Changing the Dictation locale pref should change the subtitle
    // computation.
    page.prefs.settings.a11y.dictation_locale.value = 'es';
    assertEquals(
        'Couldn’t download Spanish speech files. Download will be attempted ' +
            'later. Speech is sent to Google for processing until download ' +
            'is completed.',
        page.computeDictationLocaleSubtitle_());

    page.prefs.settings.a11y.dictation_locale.value = 'de';
    assertEquals(
        'German speech is sent to Google for processing',
        page.computeDictationLocaleSubtitle_());

    page.prefs.settings.a11y.dictation_locale.value = 'fr-FR';
    assertEquals(
        'French (France) speech is sent to Google for processing',
        page.computeDictationLocaleSubtitle_());

    // Only use the subtitle override once.
    page.useDictationLocaleSubtitleOverride_ = true;
    assertEquals('Testing', page.computeDictationLocaleSubtitle_());
    assertFalse(page.useDictationLocaleSubtitleOverride_);
    assertEquals(
        'French (France) speech is sent to Google for processing',
        page.computeDictationLocaleSubtitle_());
  });

  [true, false].forEach(isAccessibilityOSSettingsVisibilityEnabled => {
    loadTimeData.overrideValues({isAccessibilityOSSettingsVisibilityEnabled});

    const selectorRouteList = [
      {
        selector: '#captionsSubpageButton',
        route: routes.MANAGE_CAPTION_SETTINGS,
      },
      {selector: '#displaySubpageButton', route: routes.DISPLAY},
      {selector: '#keyboardSubpageButton', route: routes.KEYBOARD},
      {selector: '#pointerSubpageButton', route: routes.POINTERS},
    ];

    if (!isAccessibilityOSSettingsVisibilityEnabled) {
      selectorRouteList.push(
          {selector: '#ttsSubpageButton', route: routes.MANAGE_TTS_SETTINGS});
    }

    selectorRouteList.forEach(({selector, route}) => {
      test(
          `should focus ${selector} button when returning from ${
              route.path} subpage`,
          async () => {
            initPage();
            flush();
            const router = Router.getInstance();

            const subpageButton = page.shadowRoot.querySelector(selector);
            assertTrue(!!subpageButton);

            subpageButton.click();
            assertEquals(route, router.getCurrentRoute());
            assertNotEquals(
                subpageButton, page.shadowRoot.activeElement,
                `${selector} should not be focused`);

            const popStateEventPromise = eventToPromise('popstate', window);
            router.navigateToPreviousRoute();
            await popStateEventPromise;
            await waitBeforeNextRender(page);

            assertEquals(routes.MANAGE_ACCESSIBILITY, router.getCurrentRoute());
            assertEquals(
                subpageButton, page.shadowRoot.activeElement,
                `${selector} should be focused`);
          });
    });
  });
});
