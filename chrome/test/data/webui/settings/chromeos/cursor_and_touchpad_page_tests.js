// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('CursorAndTouchpadPageTests', function() {
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
  }

  function initPage(opt_prefs) {
    page = document.createElement('settings-cursor-and-touchpad-page');
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
    Router.getInstance().navigateTo(routes.A11Y_CURSOR_AND_TOUCHPAD);
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
    Router.getInstance().resetRouteForTesting();
  });

  test(
      'should focus pointerSubpageButton button when returning from Pointers subpage',
      async () => {
        const selector = '#pointerSubpageButton';
        const route = routes.POINTERS;
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

        assertEquals(routes.A11Y_CURSOR_AND_TOUCHPAD, router.getCurrentRoute());
        assertEquals(
            subpageButton, page.shadowRoot.activeElement,
            `${selector} should be focused`);
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

    // Shelf navigation buttons are not shown in kiosk mode, even if
    // showTabletModeShelfNavigationButtonsSettings is true.
    assertFalse(isVisible(page.shadowRoot.querySelector(
        '#shelfNavigationButtonsEnabledControl')));

    const subpageLinks = page.root.querySelectorAll('cr-link-row');
    subpageLinks.forEach(subpageLink => assertFalse(isVisible(subpageLink)));
  });
});
