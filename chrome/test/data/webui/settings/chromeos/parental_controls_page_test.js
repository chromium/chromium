// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {osPageVisibility, ParentalControlsBrowserProxy, ParentalControlsBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

/** @implements {ParentalControlsBrowserProxy} */
class TestParentalControlsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'showAddSupervisionDialog',
      'launchFamilyLinkSettings',
    ]);
  }

  /** @override */
  launchFamilyLinkSettings() {
    this.methodCalled('launchFamilyLinkSettings');
  }

  /** @override */
  showAddSupervisionDialog() {
    this.methodCalled('showAddSupervisionDialog');
  }
}

suite('Chrome OS parental controls page setup item tests', function() {
  /** @type {ParentalControlsPage} */
  let parentalControlsPage = null;

  /** @type {TestParentalControlsBrowserProxy} */
  let parentalControlsBrowserProxy = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      // Simulate parental controls.
      showParentalControls: true,
    });
  });

  setup(function() {
    parentalControlsBrowserProxy = new TestParentalControlsBrowserProxy();
    ParentalControlsBrowserProxyImpl.instance_ = parentalControlsBrowserProxy;

    PolymerTest.clearBody();
    parentalControlsPage =
        document.createElement('settings-parental-controls-page');
    parentalControlsPage.pageVisibility = osPageVisibility;
    document.body.appendChild(parentalControlsPage);
    flush();
  });

  teardown(function() {
    parentalControlsPage.remove();
  });

  test('parental controls page enabled when online', () => {
    // Setup button is shown and enabled.
    const setupButton =
        assert(parentalControlsPage.$$('#parental-controls-item cr-button'));

    setupButton.click();

    // Ensure that the request to launch the add supervision flow went
    // through.
    assertEquals(
        parentalControlsBrowserProxy.getCallCount('showAddSupervisionDialog'),
        1);
  });

  test('parental controls page disabled when offline', () => {
    // Simulate going offline
    window.dispatchEvent(new CustomEvent('offline'));
    // Setup button is shown but disabled.
    const setupButton =
        assert(parentalControlsPage.$$('#parental-controls-item cr-button'));
    assertTrue(setupButton.disabled);

    setupButton.click();

    // Ensure that the request to launch the add supervision flow does not
    // go through.
    assertEquals(
        parentalControlsBrowserProxy.getCallCount('showAddSupervisionDialog'),
        0);
  });

  test('parental controls page re-enabled when it comes back online', () => {
    // Simulate going offline
    window.dispatchEvent(new CustomEvent('offline'));
    // Setup button is shown but disabled.
    const setupButton =
        assert(parentalControlsPage.$$('#parental-controls-item cr-button'));
    assertTrue(setupButton.disabled);

    // Come back online.
    window.dispatchEvent(new CustomEvent('online'));
    // Setup button is shown and re-enabled.
    assertFalse(setupButton.disabled);
  });
});

suite('Chrome OS parental controls page child account tests', function() {
  /** @type {ParentalControlsPage} */
  let parentalControlsPage = null;

  /** @type {TestParentalControlsBrowserProxy} */
  let parentalControlsBrowserProxy = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      // Simulate parental controls.
      showParentalControls: true,
      // Simulate child account.
      isChild: true,
    });
  });

  setup(async function() {
    parentalControlsBrowserProxy = new TestParentalControlsBrowserProxy();
    ParentalControlsBrowserProxyImpl.instance_ = parentalControlsBrowserProxy;

    PolymerTest.clearBody();
    parentalControlsPage =
        document.createElement('settings-parental-controls-page');
    parentalControlsPage.pageVisibility = osPageVisibility;
    document.body.appendChild(parentalControlsPage);
    flush();
  });

  teardown(function() {
    parentalControlsPage.remove();
  });

  test('parental controls page child view shown to child account', () => {
    // Get the link row.
    const linkRow =
        assert(parentalControlsPage.$$('#parental-controls-item cr-link-row'));

    linkRow.click();
    // Ensure that the request to launch FLH went through.
    assertEquals(
        parentalControlsBrowserProxy.getCallCount('launchFamilyLinkSettings'),
        1);
  });
});
