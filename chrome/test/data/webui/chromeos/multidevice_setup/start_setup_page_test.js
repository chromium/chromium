// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://multidevice-setup/strings.m.js';
import 'chrome://resources/ash/common/multidevice_setup/start_setup_page.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

/**
 * In the testing framework, a click on a select option does not cause a
 * change in the select tag's attribute or trigger a change event so this
 * method emulates that behavior.
 *
 * @param {!HTMLSelectElement} Dropdown menu to endow with emulated
 *     behavior.
 */
const emulateDropdownBehavior = function(dropdown) {
  for (let i = 0; i < dropdown.length; i++) {
    dropdown.options[i].addEventListener('click', function() {
      dropdown.selectedIndex = i;
      dropdown.dispatchEvent(new CustomEvent('change'));
    });
  }
};

/**
 * @fileoverview Suite of tests for page-specific behaviors of StartSetupPage.
 */
suite('MultiDeviceSetup', () => {
  /**
   * StartSetupPage created before each test. Defined in setUp.
   * @type {StartSetupPage|undefined}
   */
  let startSetupPageElement;

  const START = 'start-setup-page';

  // TODO(crbug.com/40105247): When v1 DeviceSync is turned off, all
  // devices should have an Instance ID.
  const DEVICES = [
    // TODO(crbug.com/40106510) Replace the hard-coded values with the
    // deviceSync enum. This is currently causing an import error where
    // chromeos is not defined.
    {
      remoteDevice: {deviceName: 'Pixel XL', deviceId: 'legacy-id-1'},
      connectivityStatus: 0,  // kOnline
    },
    {
      remoteDevice: {deviceName: 'Nexus 6P', instanceId: 'iid-2'},
      connectivityStatus: 1,  // kOffline
    },
    {
      remoteDevice:
          {deviceName: 'Nexus 5', deviceId: 'legacy-id-3', instanceId: 'iid-3'},
      connectivityStatus: 2,  // kUnknownConnectivity
    },
    {
      remoteDevice:
          {deviceName: 'Pixel 4', deviceId: 'legacy-id-4', instanceId: ''},
      connectivityStatus: 3,  // kOnline
    },
  ];

  setup(async () => {
    startSetupPageElement = document.createElement('start-setup-page');
    document.body.appendChild(startSetupPageElement);
    startSetupPageElement.devices = DEVICES;
    flush();
    emulateDropdownBehavior(startSetupPageElement.$.deviceDropdown);
  });

  const selectOptionByTextContent = function(optionText) {
    const optionNodeList =
        startSetupPageElement.$.deviceDropdown.querySelectorAll('option');
    for (const option of optionNodeList.values()) {
      if (option.textContent.trim() === optionText) {
        option.click();
        return;
      }
    }
  };

  // TODO(crbug.com/40105247): When v1 DeviceSync is turned off, all
  // selected IDs will be Instance IDs.
  test('Finding devices populates dropdown and defines selected device', () => {
    assertEquals(
        startSetupPageElement.$.deviceDropdown.querySelectorAll('option')
            .length,
        DEVICES.length);
    assertEquals(
        startSetupPageElement.selectedInstanceIdOrLegacyDeviceId,
        'legacy-id-1');
  });

  // TODO(crbug.com/40105247): When v1 DeviceSync is turned off, all
  // selected IDs will be Instance IDs.
  test('Selected ID changes when dropdown options are selected', () => {
    selectOptionByTextContent('Nexus 6P (offline)');
    assertEquals(
        startSetupPageElement.selectedInstanceIdOrLegacyDeviceId, 'iid-2');
    selectOptionByTextContent('Nexus 5');
    assertEquals(
        startSetupPageElement.selectedInstanceIdOrLegacyDeviceId, 'iid-3');
    selectOptionByTextContent('Pixel 4');
    assertEquals(
        startSetupPageElement.selectedInstanceIdOrLegacyDeviceId,
        'legacy-id-4');
    selectOptionByTextContent('Pixel XL');
    assertEquals(
        startSetupPageElement.selectedInstanceIdOrLegacyDeviceId,
        'legacy-id-1');
  });
});
