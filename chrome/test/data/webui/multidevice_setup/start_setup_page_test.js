// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * In the testing framework, a click on a select option does not cause a change
 * in the select tag's attribute or trigger a change event so this method
 * emulates that behavior.
 *
 * @param {!HTMLSelectElement} Dropdown menu to endow with emulated behavior.
 */
let emulateDropdownBehavior = function(dropdown) {
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
cr.define('multidevice_setup', () => {
  function registerStartSetupPageTests() {
    suite('MultiDeviceSetup', () => {
      /**
       * StartSetupPage created before each test. Defined in setUp.
       * @type {StartSetupPage|undefined}
       */
      let startSetupPageElement;

      const START = 'start-setup-page';

      // TODO(https://crbug.com/1019206): When v1 DeviceSync is turned off, all
      // devices should have an Instance ID.
      const DEVICES = [
        {
          remoteDevice: {deviceName: 'Pixel XL', deviceId: 'legacy-id-1'},
          connectivityStatus:
              chromeos.deviceSync.mojom.ConnectivityStatus.kOnline
        },
        {
          remoteDevice: {deviceName: 'Nexus 6P', instanceId: 'iid-2'},
          connectivityStatus:
              chromeos.deviceSync.mojom.ConnectivityStatus.kOffline
        },
        {
          remoteDevice: {
            deviceName: 'Nexus 5',
            deviceId: 'legacy-id-3',
            instanceId: 'iid-3'
          },
          connectivityStatus:
              chromeos.deviceSync.mojom.ConnectivityStatus.kUnknownConnectivity
        },
        {
          remoteDevice:
              {deviceName: 'Pixel 4', deviceId: 'legacy-id-4', instanceId: ''},
          connectivityStatus:
              chromeos.deviceSync.mojom.ConnectivityStatus.kOnline
        },
      ];

      setup(() => {
        startSetupPageElement = document.createElement('start-setup-page');
        document.body.appendChild(startSetupPageElement);
        startSetupPageElement.devices = DEVICES;
        Polymer.dom.flush();
        emulateDropdownBehavior(startSetupPageElement.$.deviceDropdown);
      });

      let selectOptionByTextContent = function(optionText) {
        const optionNodeList =
            startSetupPageElement.$.deviceDropdown.querySelectorAll('option');
        for (option of optionNodeList.values()) {
          if (option.textContent.trim() === optionText) {
            option.click();
            return;
          }
        }
      };

      // TODO(https://crbug.com/1019206): When v1 DeviceSync is turned off, all
      // selected IDs will be Instance IDs.
      test(
          'Finding devices populates dropdown and defines selected device',
          () => {
            assertEquals(
                startSetupPageElement.$.deviceDropdown
                    .querySelectorAll('option')
                    .length,
                DEVICES.length);
            assertEquals(
                startSetupPageElement.selectedInstanceIdOrLegacyDeviceId,
                'legacy-id-1');
          });

      // TODO(https://crbug.com/1019206): When v1 DeviceSync is turned off, all
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
  }
  return {registerStartSetupPageTests: registerStartSetupPageTests};
});
