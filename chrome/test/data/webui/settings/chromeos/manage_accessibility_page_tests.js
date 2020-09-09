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

  function initPage() {
    page = document.createElement('settings-manage-a11y-page');
    document.body.appendChild(page);
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

  test('some parts are hidden in kiosk mode', function() {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
    });
    initPage();
    // Add mouse and touchpad to show some hidden settings.
    deviceBrowserProxy.hasMouse = true;
    deviceBrowserProxy.hasTouchpad = true;
    Polymer.dom.flush();

    // Accessibility learn more link should be hidden.
    assertFalse(isVisible(page.$$('setings-localized-link')));

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
});
