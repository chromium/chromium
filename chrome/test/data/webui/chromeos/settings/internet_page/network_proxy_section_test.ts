// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {NetworkProxySectionElement} from 'chrome://os-settings/lazy_load.js';
import type {NetworkProxyElement} from 'chrome://resources/ash/common/network/network_proxy.js';
import type {ManagedProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType, OncSource, PolicySource, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('<network-proxy-section>', () => {
  let proxySection: NetworkProxySectionElement;
  let props: ManagedProperties;

  function initializeProps(): ManagedProperties {
    return proxySection.managedProperties = {
      connectionState: ConnectionStateType.MIN_VALUE,
      source: OncSource.MIN_VALUE,
      connectable: false,
      portalState: PortalState.MIN_VALUE,
      errorState: null,
      guid: '',
      ipAddressConfigType: {
        activeValue: '',
        policySource: PolicySource.MIN_VALUE,
        policyValue: null,
      },
      ipConfigs: null,
      metered: null,
      name: null,
      nameServersConfigType: {
        activeValue: '',
        policySource: PolicySource.MIN_VALUE,
        policyValue: null,
      },
      priority: null,
      proxySettings: null,
      staticIpConfig: null,
      savedIpConfig: null,
      type: NetworkType.MIN_VALUE,
      typeProperties: {
        cellular: undefined,
        ethernet: undefined,
        tether: undefined,
        vpn: undefined,
        wifi: undefined,
      },
      trafficCounterProperties: {
        lastResetTime: null,
        friendlyDate: null,
        autoReset: false,
        userSpecifiedResetDay: 0,
      },
    };
  }

  setup(() => {
    proxySection = document.createElement('network-proxy-section');
    proxySection.prefs = {
      // prefs.settings.use_shared_proxies is set by the proxy service in CrOS,
      // which does not run in the browser_tests environment.
      'settings': {
        'use_shared_proxies': {
          key: 'use_shared_proxies',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
      'proxy': {},
    };
    props = initializeProps();
    document.body.appendChild(proxySection);
    flush();
  });

  teardown(() => {
    proxySection.remove();
  });

  test('Visibility of Allow Shared toggle', () => {
    const allowSharedToggle = proxySection.$.allowShared;

    proxySection.managedProperties = {
      ...props,
      source: OncSource.kNone,
    };
    assertTrue(allowSharedToggle.hidden);

    proxySection.managedProperties = {
      ...props,
      source: OncSource.kDevice,
    };
    assertFalse(allowSharedToggle.hidden);

    proxySection.managedProperties = {
      ...props,
      source: OncSource.kDevicePolicy,
    };
    assertFalse(allowSharedToggle.hidden);

    proxySection.managedProperties = {
      ...props,
      source: OncSource.kUser,
    };
    assertTrue(allowSharedToggle.hidden);

    proxySection.managedProperties = {
      ...props,
      source: OncSource.kUserPolicy,
    };
    assertTrue(allowSharedToggle.hidden);
  });

  test('Disabled UI state', () => {
    const allowSharedToggle = proxySection.$.allowShared;
    const networkProxy =
        proxySection.shadowRoot!.querySelector<NetworkProxyElement>(
            'network-proxy');
    assertTrue(!!networkProxy);

    assertFalse(allowSharedToggle.disabled);
    assertTrue(networkProxy.editable);

    proxySection.disabled = true;

    assertTrue(allowSharedToggle.disabled);
    assertFalse(networkProxy.editable);
  });

  const kExtensionId = 'ext-id';
  const kExtensionName = 'ext-name';

  function setDirectProxyConfig() {
    props.proxySettings = {
      type: {
        activeValue: 'Direct',
        policySource: PolicySource.kActiveExtension,
        policyValue: null,
      },
      manual: null,
      excludeDomains: null,
      pac: null,
    };
    proxySection.managedProperties = {
      ...props,
      source: OncSource.kNone,
    };
    flush();
  }

  // Tests that the extension indicator is shown with the correct extension
  // metadata when the proxy is controlled by an extension in Ash. In this case,
  // the extension metadata is encapsulated with the proxy pref.
  test('Proxy set by Ash extension', () => {
    assertNull(proxySection.shadowRoot!.querySelector(
        'extension-controlled-indicator'));
    // Configure the proxy pref with the extension data.
    proxySection.prefs.proxy = {
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {},
      extensionId: kExtensionId,
      controlledByName: kExtensionName,
      extensionCanBeDisabled: false,
    };
    // Set the effective proxy value.
    setDirectProxyConfig();

    const extensionIndicator = proxySection.shadowRoot!.querySelector(
        'extension-controlled-indicator');
    assertTrue(!!extensionIndicator);
    assertEquals(kExtensionName, extensionIndicator.extensionName);
    assertEquals(kExtensionId, extensionIndicator.extensionId);
    assertFalse(extensionIndicator.extensionCanBeDisabled);
  });
});
