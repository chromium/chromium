// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {AlwaysOnVpnMode} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('NetworkAlwaysOnVpn', function() {
  /** @type {!NetworkAlwaysOnVpnElement|undefined} */
  let alwaysOnVpnOptions;

  /**
   * @return {!Promise<unknown>}
   * @private
   */
  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * Sends a mouse click on the given HTML element.
   * @param {!HTMLElement} element
   * @return {!Promise<unknown>}
   * @private
   */
  function click(element) {
    element.click();
    return flushAsync();
  }

  /**
   * Selects the provided value in the HTML element.
   * @param {!HTMLSelectElement} select
   * @param {!value} string
   * @return {!Promise<unknown>}
   * @private
   */
  function select(element, value) {
    element.value = value;
    element.dispatchEvent(new CustomEvent('change'));
    return flushAsync();
  }

  /**
   * @return {!CrToggleElement}
   * @private
   */
  function getEnableToggle() {
    const toggle =
        alwaysOnVpnOptions.shadowRoot.querySelector('#alwaysOnVpnEnableToggle');
    assert(!!toggle);
    return toggle;
  }

  /**
   * @return {!CrToggleElement}
   * @private
   */
  function getLockdownToggle() {
    const toggle = alwaysOnVpnOptions.shadowRoot.querySelector(
        '#alwaysOnVpnLockdownToggle');
    assert(!!toggle);
    return toggle;
  }

  /**
   * @return {!HTMLSelectElement}
   * @private
   */
  function getServiceSelect() {
    const select = alwaysOnVpnOptions.shadowRoot.querySelector(
        '#alwaysOnVpnServiceSelect');
    assert(!!select);
    return select;
  }

  /**
   * @param {!AlwaysOnVpnMode} mode
   * @param {!string} service
   * @private
   */
  function setConfiguration(mode, service) {
    assert(alwaysOnVpnOptions);
    alwaysOnVpnOptions.mode = mode;
    alwaysOnVpnOptions.service = service;
    return flushAsync();
  }

  /**
   * @return {!Promise<unknown>}
   * @private
   */
  function addVpnNetworks() {
    assert(alwaysOnVpnOptions);
    alwaysOnVpnOptions.networks = [
      OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn1'),
      OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn2'),
    ];
    return flushAsync();
  }

  setup(function() {
    alwaysOnVpnOptions = /** @type {!NetworkAlwaysOnVpnElement} */
        document.createElement('network-always-on-vpn');
    assert(alwaysOnVpnOptions);
    alwaysOnVpnOptions.networks = [];
    alwaysOnVpnOptions.mode = AlwaysOnVpnMode.kOff;
    alwaysOnVpnOptions.service = '';
    document.body.appendChild(alwaysOnVpnOptions);
    flush();
  });

  test('Mode off without networks', () => {
    const toggle = getEnableToggle();
    // Toggle is not checked: OFF mode.
    assertFalse(toggle.checked);
    // Toggle is disabled: no compatible networks available.
    assertTrue(toggle.disabled);
  });

  test('Mode off with available networks', () => {
    return addVpnNetworks().then(() => {
      const toggle = getEnableToggle();
      // Enabled: networks are available.
      assertFalse(toggle.disabled);
      // Unchecked: mode is off.
      assertFalse(toggle.checked);
    });
  });

  test('Mode best-effort with available networks', () => {
    return setConfiguration(AlwaysOnVpnMode.kBestEffort, '')
        .then(addVpnNetworks)
        .then(() => {
          const enableToggle = getEnableToggle();
          // Enabled: networks are available.
          assertFalse(enableToggle.disabled);
          // Checked: mode is best-effort.
          assertTrue(enableToggle.checked);
          const lockdownToggle = getLockdownToggle();
          // Enabled: we should be able to enable lockdown.
          assertFalse(lockdownToggle.disabled);
          // Unchecked: mode is best-effort.
          assertFalse(lockdownToggle.checked);
        });
  });

  test('Mode strict with available networks', () => {
    return setConfiguration(AlwaysOnVpnMode.kStrict, '')
        .then(addVpnNetworks)
        .then(() => {
          const enableToggle = getEnableToggle();
          // Enabled: networks are available.
          assertFalse(enableToggle.disabled);
          // Checked: mode is strict.
          assertTrue(enableToggle.checked);
          const lockdownToggle = getLockdownToggle();
          // Enabled: we should be able to toggle lockdown.
          assertFalse(lockdownToggle.disabled);
          // Checked: mode is strict.
          assertTrue(lockdownToggle.checked);
        });
  });

  test('Mode best-effort with a selected network', () => {
    return setConfiguration(AlwaysOnVpnMode.kBestEffort, 'vpn1_guid')
        .then(addVpnNetworks)
        .then(() => {
          // Best-effort mode
          assertTrue(getEnableToggle().checked);
          // 'vpn1' client must be selected
          assertEquals('vpn1_guid', getServiceSelect().value);
        });
  });

  test('Mode best-effort: options count in the services menu', () => {
    return setConfiguration(AlwaysOnVpnMode.kBestEffort, '')
        .then(addVpnNetworks)
        .then(() => {
          // No service is selected, the menu contains a blank item (the
          // placeholder) and two available networks.
          assertEquals(3, getServiceSelect().options.length);
        })
        .then(() => {
          return setConfiguration(AlwaysOnVpnMode.kBestEffort, 'vpn1_guid');
        })
        .then(() => {
          // A services is select, the placeholder is not required, there's only
          // two items in the menu.
          assertEquals(2, getServiceSelect().options.length);
        });
  });

  test('Always-on VPN without service', () => {
    return addVpnNetworks().then(() => click(getEnableToggle())).then(() => {
      assertEquals(AlwaysOnVpnMode.kBestEffort, alwaysOnVpnOptions.mode);
      assertEquals('', alwaysOnVpnOptions.service);
    });
  });

  test('Always-on VPN with lockdown but no service', () => {
    return addVpnNetworks()
        .then(() => click(getEnableToggle()))
        .then(() => click(getLockdownToggle()))
        .then(() => {
          assertEquals(AlwaysOnVpnMode.kStrict, alwaysOnVpnOptions.mode);
          assertEquals('', alwaysOnVpnOptions.service);
        });
  });

  test('Always-on VPN with a service', () => {
    return addVpnNetworks()
        .then(() => click(getEnableToggle()))
        .then(() => select(getServiceSelect(), 'vpn2_guid'))
        .then(() => {
          assertEquals(AlwaysOnVpnMode.kBestEffort, alwaysOnVpnOptions.mode);
          assertEquals('vpn2_guid', alwaysOnVpnOptions.service);
        });
  });
});
