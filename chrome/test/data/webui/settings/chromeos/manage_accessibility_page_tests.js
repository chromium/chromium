// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Checks whether a given element is visible to the user.
 * @param {!Element} element
 * @returns {boolean}
 */
function isVisible(element) {
  return !!(element && element.getBoundingClientRect().width > 0);
}

suite('ManageAccessibilityPageTests', function() {
  let page = null;
  let deviceBrowserProxy = null;

  /** @implements {settings.DevicePageBrowserProxy} */
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
      cr.webUIListenerCallback('has-mouse-changed', this.hasMouse_);
    }

    /** @param {boolean} hasTouchpad */
    set hasTouchpad(hasTouchpad) {
      this.hasTouchpad_ = hasTouchpad;
      cr.webUIListenerCallback('has-touchpad-changed', this.hasTouchpad_);
    }

    /** @override */
    initializePointers() {
      cr.webUIListenerCallback('has-mouse-changed', this.hasMouse_);
      cr.webUIListenerCallback('has-touchpad-changed', this.hasTouchpad_);
    }

    /** @override */
    initializeKeyboardWatcher() {
      cr.webUIListenerCallback('has-hardware-keyboard', this.hasKeyboard_);
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
          }
        },
        'accessibility': {
          key: 'settings.accessibility',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        }
      }
    };
  }

  setup(function() {
    deviceBrowserProxy = new TestDevicePageBrowserProxy();
    settings.DevicePageBrowserProxyImpl.instance_ = deviceBrowserProxy;

    PolymerTest.clearBody();
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Pointers row only visible if mouse/touchpad present', function() {
    initPage();
    const row = page.$$('#pointerSubpageButton');
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
    Polymer.dom.flush();

    assertTrue(isVisible(page.$$('#shelfNavigationButtonsEnabledControl')));
  });

  test('toggle tablet mode buttons', function() {
    loadTimeData.overrideValues({
      isKioskModeActive: false,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    initPage();
    Polymer.dom.flush();

    const navButtonsToggle = page.$$('#shelfNavigationButtonsEnabledControl');
    assertTrue(isVisible(navButtonsToggle));
    // The default pref value is false.
    assertFalse(navButtonsToggle.checked);

    // Clicking the toggle should update the toggle checked value, and the
    // backing preference.
    navButtonsToggle.click();
    Polymer.dom.flush();

    assertTrue(navButtonsToggle.checked);
    assertFalse(navButtonsToggle.disabled);
    assertTrue(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);

    navButtonsToggle.click();
    Polymer.dom.flush();

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

    prefs = getDefaultPrefs();
    // Enable spoken feedback.
    prefs.settings.accessibility.value = true;

    initPage(prefs);
    Polymer.dom.flush();

    const navButtonsToggle = page.$$('#shelfNavigationButtonsEnabledControl');
    assertTrue(isVisible(navButtonsToggle));

    // If spoken feedback is enabled, the shelf nav buttons toggle should be
    // disabled and checked.
    assertTrue(navButtonsToggle.disabled);
    assertTrue(navButtonsToggle.checked);

    // Clicking the toggle should have no effect.
    navButtonsToggle.click();
    Polymer.dom.flush();

    assertTrue(navButtonsToggle.disabled);
    assertTrue(navButtonsToggle.checked);
    assertFalse(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);

    // The toggle should be enabled if the spoken feedback gets disabled.
    page.set('prefs.settings.accessibility.value', false);
    Polymer.dom.flush();

    assertFalse(!!navButtonsToggle.disabled);
    assertFalse(navButtonsToggle.checked);
    assertFalse(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);

    // Clicking the toggle should update the backing pref.
    navButtonsToggle.click();
    Polymer.dom.flush();

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
    Polymer.dom.flush();

    // Accessibility learn more link should be hidden.
    assertFalse(isVisible(page.$$('setings-localized-link')));

    // Shelf navigation buttons are not shown in kiosk mode, even if
    // showTabletModeShelfNavigationButtonsSettings is true.
    assertFalse(isVisible(page.$$('#shelfNavigationButtonsEnabledControl')));

    const allowed_subpages = [
      'chromeVoxSubpageButton', 'selectToSpeakSubpageButton', 'ttsSubpageButton'
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
      isDeepLinkingEnabled: true,
    });
    initPage();

    const params = new URLSearchParams;
    params.append('settingId', '1522');
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_ACCESSIBILITY, params);

    Polymer.dom.flush();

    const deepLinkElement = page.$$('#enableSwitchAccess').$$('cr-toggle');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Switch access toggle should be focused for settingId=1522.');
  });
});
