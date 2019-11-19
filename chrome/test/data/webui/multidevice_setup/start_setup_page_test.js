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
      const DEVICES = [
        {
          remoteDevice: {deviceName: 'Pixel XL', deviceId: 'abcdxl'},
          connectivityStatus:
              chromeos.deviceSync.mojom.ConnectivityStatus.kOnline
        },
        {
          remoteDevice: {deviceName: 'Nexus 6P', deviceId: 'PpPpPp'},
          connectivityStatus:
              chromeos.deviceSync.mojom.ConnectivityStatus.kOffline
        },
        {
          remoteDevice: {deviceName: 'Nexus 5', deviceId: '12345'},
          connectivityStatus:
              chromeos.deviceSync.mojom.ConnectivityStatus.kUnknownConnectivity
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
          if (option.textContent.trim() == optionText) {
            MockInteractions.tap(option);
            return;
          }
        }
      };

      test(
          'Finding devices populates dropdown and defines selected device',
          () => {
            assertEquals(
                startSetupPageElement.$.deviceDropdown
                    .querySelectorAll('option')
                    .length,
                DEVICES.length);
            assertEquals(startSetupPageElement.selectedDeviceId, 'abcdxl');
          });

      test(
          'selectedDeviceId changes when dropdown options are selected', () => {
            selectOptionByTextContent('Nexus 6P (offline)');
            assertEquals(startSetupPageElement.selectedDeviceId, 'PpPpPp');
            selectOptionByTextContent('Nexus 5');
            assertEquals(startSetupPageElement.selectedDeviceId, '12345');
            selectOptionByTextContent('Pixel XL');
            assertEquals(startSetupPageElement.selectedDeviceId, 'abcdxl');
          });
    });
  }
  return {registerStartSetupPageTests: registerStartSetupPageTests};
});
