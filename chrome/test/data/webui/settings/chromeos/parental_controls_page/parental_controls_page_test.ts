// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrButtonElement, CrLinkRowElement, ParentalControlsBrowserProxyImpl, SettingsParentalControlsPageElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestParentalControlsBrowserProxy} from './test_parental_controls_browser_proxy.js';

suite('Chrome OS parental controls page setup item tests', function() {
  let parentalControlsPage: SettingsParentalControlsPageElement;
  let parentalControlsBrowserProxy: TestParentalControlsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      // Simulate parental controls.
      showParentalControls: true,
    });
  });

  setup(function() {
    parentalControlsBrowserProxy = new TestParentalControlsBrowserProxy();
    ParentalControlsBrowserProxyImpl.setInstanceForTesting(
        parentalControlsBrowserProxy);

    parentalControlsPage =
        document.createElement('settings-parental-controls-page');
    document.body.appendChild(parentalControlsPage);
    flush();
  });

  teardown(function() {
    parentalControlsPage.remove();
  });

  test('parental controls page enabled when online', () => {
    // Setup button is shown and enabled.
    const setupButton =
        parentalControlsPage.shadowRoot!.querySelector<CrButtonElement>(
            '#parental-controls-item cr-button');
    assert(setupButton);

    setupButton.click();

    // Ensure that the request to launch the add supervision flow went
    // through.
    assertEquals(
        1,
        parentalControlsBrowserProxy.getCallCount('showAddSupervisionDialog'));
  });

  test('parental controls page disabled when offline', () => {
    // Simulate going offline
    window.dispatchEvent(new CustomEvent('offline'));
    // Setup button is shown but disabled.
    const setupButton =
        parentalControlsPage.shadowRoot!.querySelector<CrButtonElement>(
            '#parental-controls-item cr-button');
    assert(setupButton);
    assertTrue(setupButton.disabled);

    setupButton.click();

    // Ensure that the request to launch the add supervision flow does not
    // go through.
    assertEquals(
        0,
        parentalControlsBrowserProxy.getCallCount('showAddSupervisionDialog'));
  });

  test('parental controls page re-enabled when it comes back online', () => {
    // Simulate going offline
    window.dispatchEvent(new CustomEvent('offline'));
    // Setup button is shown but disabled.
    const setupButton =
        parentalControlsPage.shadowRoot!.querySelector<CrButtonElement>(
            '#parental-controls-item cr-button');
    assert(setupButton);
    assertTrue(setupButton.disabled);

    // Come back online.
    window.dispatchEvent(new CustomEvent('online'));
    // Setup button is shown and re-enabled.
    assertFalse(setupButton.disabled);
  });
});

suite('Chrome OS parental controls page child account tests', function() {
  let parentalControlsPage: SettingsParentalControlsPageElement;
  let parentalControlsBrowserProxy: TestParentalControlsBrowserProxy;

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
    ParentalControlsBrowserProxyImpl.setInstanceForTesting(
        parentalControlsBrowserProxy);

    parentalControlsPage =
        document.createElement('settings-parental-controls-page');
    document.body.appendChild(parentalControlsPage);
    flush();
  });

  teardown(function() {
    parentalControlsPage.remove();
  });

  test('parental controls page child view shown to child account', () => {
    // Get the link row.
    const linkRow =
        parentalControlsPage.shadowRoot!.querySelector<CrLinkRowElement>(
            '#parental-controls-item cr-link-row');
    assert(linkRow);

    linkRow.click();
    // Ensure that the request to launch FLH went through.
    assertEquals(
        parentalControlsBrowserProxy.getCallCount('launchFamilyLinkSettings'),
        1);
  });
});
