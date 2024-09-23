// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {NetworkAlwaysOnVpnElement} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement} from 'chrome://os-settings/os_settings.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert} from 'chrome://resources/js/assert.js';
import {AlwaysOnVpnMode} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<network-always-on-vpn>', () => {
  let alwaysOnVpnOptions: NetworkAlwaysOnVpnElement;

  /**
   * Sends a mouse click on the given HTML element.
   */
  function click(element: HTMLElement): Promise<void> {
    element.click();
    return flushTasks();
  }

  /**
   * Selects the provided value in the HTML element.
   */
  function select(element: HTMLSelectElement, value: string): Promise<void> {
    element.value = value;
    element.dispatchEvent(new CustomEvent('change'));
    return flushTasks();
  }

  function getEnableToggle(): CrToggleElement {
    const toggle =
        alwaysOnVpnOptions.shadowRoot!.querySelector<CrToggleElement>(
            '#alwaysOnVpnEnableToggle');
    assert(toggle);
    return toggle;
  }

  function getLockdownToggle(): CrToggleElement {
    const toggle =
        alwaysOnVpnOptions.shadowRoot!.querySelector<CrToggleElement>(
            '#alwaysOnVpnLockdownToggle');
    assert(toggle);
    return toggle;
  }

  function getServiceSelect(): HTMLSelectElement {
    const select =
        alwaysOnVpnOptions.shadowRoot!.querySelector<HTMLSelectElement>(
            '#alwaysOnVpnServiceSelect');
    assert(select);
    return select;
  }

  function setConfiguration(mode: AlwaysOnVpnMode, service: string) {
    assert(alwaysOnVpnOptions);
    alwaysOnVpnOptions.mode = mode;
    alwaysOnVpnOptions.service = service;
    return flushTasks();
  }

  function addVpnNetworks(): Promise<void> {
    assert(alwaysOnVpnOptions);
    alwaysOnVpnOptions.networks = [
      OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn1'),
      OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn2'),
    ];
    return flushTasks();
  }

  setup(() => {
    alwaysOnVpnOptions = document.createElement('network-always-on-vpn');
    assert(alwaysOnVpnOptions);
    alwaysOnVpnOptions.networks = [];
    alwaysOnVpnOptions.mode = AlwaysOnVpnMode.kOff;
    alwaysOnVpnOptions.service = '';
    document.body.appendChild(alwaysOnVpnOptions);
    flush();
  });

  teardown(() => {
    alwaysOnVpnOptions.remove();
  });

  test('Mode off without networks', () => {
    const toggle = getEnableToggle();
    // Toggle is not checked: OFF mode.
    assertFalse(toggle.checked);
    // Toggle is disabled: no compatible networks available.
    assertTrue(toggle.disabled);
  });

  test('Mode off with available networks', async () => {
    await addVpnNetworks();
    const toggle = getEnableToggle();
    // Enabled: networks are available.
    assertFalse(toggle.disabled);
    // Unchecked: mode is off.
    assertFalse(toggle.checked);
  });

  test('Mode best-effort with available networks', async () => {
    await setConfiguration(AlwaysOnVpnMode.kBestEffort, '');
    await addVpnNetworks();
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

  test('Mode strict with available networks', async () => {
    await setConfiguration(AlwaysOnVpnMode.kStrict, '');
    await addVpnNetworks();
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

  test('Mode best-effort with a selected network', async () => {
    await setConfiguration(AlwaysOnVpnMode.kBestEffort, 'vpn1_guid');
    await addVpnNetworks();
    // Best-effort mode
    assertTrue(getEnableToggle().checked);
    // 'vpn1' client must be selected
    assertEquals('vpn1_guid', getServiceSelect().value);
  });

  test('Mode best-effort: options count in the services menu', async () => {
    await setConfiguration(AlwaysOnVpnMode.kBestEffort, '');
    await addVpnNetworks();
    // No service is selected, the menu contains a blank item (the
    // placeholder) and two available networks.
    assertEquals(3, getServiceSelect().options.length);
    await setConfiguration(AlwaysOnVpnMode.kBestEffort, 'vpn1_guid');
    // A services is select, the placeholder is not required, there's only
    // two items in the menu.
    assertEquals(2, getServiceSelect().options.length);
  });

  test('Always-on VPN without service', async () => {
    await addVpnNetworks();
    await click(getEnableToggle());
    assertEquals(AlwaysOnVpnMode.kBestEffort, alwaysOnVpnOptions.mode);
    assertEquals('', alwaysOnVpnOptions.service);
  });

  test('Always-on VPN with lockdown but no service', async () => {
    await addVpnNetworks();
    await click(getEnableToggle());
    await click(getLockdownToggle());
    assertEquals(AlwaysOnVpnMode.kStrict, alwaysOnVpnOptions.mode);
    assertEquals('', alwaysOnVpnOptions.service);
  });

  test('Always-on VPN with a service', async () => {
    await addVpnNetworks();
    await click(getEnableToggle());
    await select(getServiceSelect(), 'vpn2_guid');
    assertEquals(AlwaysOnVpnMode.kBestEffort, alwaysOnVpnOptions.mode);
    assertEquals('vpn2_guid', alwaysOnVpnOptions.service);
  });
});
