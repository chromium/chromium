// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {NetworkProxySectionElement} from 'chrome://os-settings/lazy_load.js';
import type {NetworkProxyElement} from 'chrome://resources/ash/common/network/network_proxy.js';
import type {ManagedProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType, OncSource, PolicySource, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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
      'proxy_override_rules': {},
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

  function setExtensionProxyConfig() {
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

  function assertVisible(element: Element|null) {
    assertTrue(isVisible(element));
  }

  function assertNotVisible(element: Element|null) {
    assertFalse(isVisible(element));
  }

  function getExtensionElement(): Element|null {
    return proxySection.shadowRoot!.querySelector(
        'extension-controlled-indicator');
  }

  // Tests that the extension indicator is shown with the correct extension
  // metadata when the proxy is controlled by an extension in Ash. In this case,
  // the extension metadata is encapsulated with the proxy pref.
  test('Proxy set by Ash extension', () => {
    assertNotVisible(getExtensionElement());
    // Configure the proxy pref with the extension data.
    proxySection.prefs['proxy'] = {
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {},
      extensionId: kExtensionId,
      controlledByName: kExtensionName,
      extensionCanBeDisabled: false,
    };
    // Set the effective proxy value.
    setExtensionProxyConfig();

    const extensionIndicator = getExtensionElement();
    assertVisible(extensionIndicator);
    assertEquals(kExtensionName, (extensionIndicator as any).extensionName);
    assertEquals(kExtensionId, (extensionIndicator as any).extensionId);
    assertFalse((extensionIndicator as any).extensionCanBeDisabled);
  });

  function getOverrideRulesElement(): Element|null {
    return proxySection.shadowRoot!.querySelector('#overrideRulesPolicy');
  }

  function getOverrideRulesExtensionElement(): Element|null {
    return proxySection.shadowRoot!.querySelector(
        '#overrideRulesExtension extension-controlled-indicator');
  }

  function getProxyExtensionElement(): Element|null {
    return proxySection.shadowRoot!.querySelector(
        '#proxyExtension extension-controlled-indicator');
  }

  function getNetworkPolicyElement(): Element|null {
    return proxySection.shadowRoot!.querySelector('#networkPolicy');
  }

  function getCombinedPoliciesElement(): Element|null {
    return proxySection.shadowRoot!.querySelector('#combinedPolicies');
  }

  test('Visibility of standalone proxy override rules disclaimer', () => {
    // All hidden by default.
    assertNotVisible(getOverrideRulesElement());
    assertNotVisible(getNetworkPolicyElement());
    assertNotVisible(getCombinedPoliciesElement());

    // Hidden when the value array is empty.
    proxySection.setPrefValue('proxy_override_rules', []);
    flush();
    assertNotVisible(getOverrideRulesElement());
    assertNotVisible(getNetworkPolicyElement());
    assertNotVisible(getCombinedPoliciesElement());

    // Only override rules section is shown when there is a value in the array.
    proxySection.setPrefValue('proxy_override_rules', ['some_value']);
    flush();
    assertVisible(getOverrideRulesElement());
    assertNotVisible(getNetworkPolicyElement());
    assertNotVisible(getCombinedPoliciesElement());
  });

  function setPolicyProxySettings() {
    props.proxySettings = {
      type: {
        activeValue: 'Direct',
        policySource: PolicySource.kUserPolicyEnforced,
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

  function clearProxySettings() {
    props.proxySettings = null;
    proxySection.managedProperties = {
      ...props,
      source: OncSource.kNone,
    };
    flush();
  }

  test('Proxy settings source disclaimers change dynamically', () => {
    assertNotVisible(getNetworkPolicyElement());
    assertNotVisible(getExtensionElement());
    assertNotVisible(getOverrideRulesElement());
    assertNotVisible(getCombinedPoliciesElement());

    // Proxy settings by cloud user policy.
    setPolicyProxySettings();

    assertVisible(getNetworkPolicyElement());
    assertNotVisible(getExtensionElement());
    assertNotVisible(getOverrideRulesElement());
    assertNotVisible(getCombinedPoliciesElement());

    // Add override rules.
    proxySection.setPrefValue('proxy_override_rules', ['some_value']);
    flush();

    assertVisible(getCombinedPoliciesElement());
    assertNotVisible(getExtensionElement());
    assertNotVisible(getNetworkPolicyElement());
    assertNotVisible(getOverrideRulesElement());

    // Remove proxy settings policy.
    clearProxySettings();

    assertVisible(getOverrideRulesElement());
    assertNotVisible(getExtensionElement());
    assertNotVisible(getCombinedPoliciesElement());
    assertNotVisible(getNetworkPolicyElement());

    // Configure the proxy pref with the extension data.
    proxySection.prefs['proxy'] = {
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {},
      extensionId: kExtensionId,
      controlledByName: kExtensionName,
      extensionCanBeDisabled: false,
    };
    // Set the effective proxy value.
    setExtensionProxyConfig();

    assertVisible(getOverrideRulesElement());
    assertVisible(getExtensionElement());
    assertNotVisible(getCombinedPoliciesElement());
    assertNotVisible(getNetworkPolicyElement());

    // Remove override rules.
    proxySection.setPrefValue('proxy_override_rules', []);
    flush();

    assertVisible(getExtensionElement());
    assertNotVisible(getOverrideRulesElement());
    assertNotVisible(getCombinedPoliciesElement());
    assertNotVisible(getNetworkPolicyElement());

    // Remove extension settings.
    proxySection.prefs['proxy'] = {};
    clearProxySettings();

    assertNotVisible(getNetworkPolicyElement());
    assertNotVisible(getExtensionElement());
    assertNotVisible(getOverrideRulesElement());
    assertNotVisible(getCombinedPoliciesElement());
  });

  test('Proxy override rules set by extension', () => {
    const kOverrideExtensionId = 'override-ext-id';
    const kOverrideExtensionName = 'override-ext-name';

    assertNotVisible(getOverrideRulesElement());
    assertNotVisible(getOverrideRulesExtensionElement());
    assertNotVisible(getProxyExtensionElement());
    assertNotVisible(getNetworkPolicyElement());
    assertNotVisible(getCombinedPoliciesElement());

    // Case 1: Only proxy_override_rules is set by an extension.
    proxySection.set('prefs.proxy_override_rules', {
      key: 'proxy_override_rules',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: ['some_value'],
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      extensionId: kOverrideExtensionId,
      controlledByName: kOverrideExtensionName,
      extensionCanBeDisabled: true,
    });
    flush();

    const overrideExtensionIndicator = getOverrideRulesExtensionElement();
    assertVisible(overrideExtensionIndicator);
    assertEquals(
        kOverrideExtensionName,
        (overrideExtensionIndicator as any).extensionName);
    assertEquals(
        kOverrideExtensionId, (overrideExtensionIndicator as any).extensionId);
    assertTrue((overrideExtensionIndicator as any).extensionCanBeDisabled);
    assertEquals(
        `${kOverrideExtensionName} set proxy override rules`,
        overrideExtensionIndicator!.shadowRoot!.querySelector(
                                                   'span')!.textContent.trim());
    assertNotVisible(getOverrideRulesElement());
    assertNotVisible(getProxyExtensionElement());
    assertNotVisible(getNetworkPolicyElement());
    assertNotVisible(getCombinedPoliciesElement());

    // Case 2: proxy_override_rules is set by an extension and proxySettings is
    // enforced by policy.
    setPolicyProxySettings();

    assertVisible(getOverrideRulesExtensionElement());
    assertVisible(getNetworkPolicyElement());
    assertNotVisible(getOverrideRulesElement());
    assertNotVisible(getCombinedPoliciesElement());
    assertNotVisible(getProxyExtensionElement());

    // Case 3: Both proxy_override_rules and proxy are set by the same
    // extension. Only a single extension indicator should be shown.
    proxySection.set('prefs.proxy', {
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {},
      extensionId: kOverrideExtensionId,
      controlledByName: kOverrideExtensionName,
      extensionCanBeDisabled: true,
    });
    setExtensionProxyConfig();

    assertNotVisible(getOverrideRulesExtensionElement());
    assertVisible(getProxyExtensionElement());
    assertNotVisible(getOverrideRulesElement());
    assertNotVisible(getNetworkPolicyElement());
    assertNotVisible(getCombinedPoliciesElement());

    // Case 4: proxy_override_rules and proxy are set by different extensions
    // (updating prefs.proxy without changing managedProperties). Both
    // extension indicators should be shown with distinct labels.
    proxySection.set('prefs.proxy', {
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {},
      extensionId: kExtensionId,
      controlledByName: kExtensionName,
      extensionCanBeDisabled: false,
    });
    flush();

    assertVisible(getOverrideRulesExtensionElement());
    assertVisible(getProxyExtensionElement());
    assertEquals(
        `${kOverrideExtensionName} set proxy override rules`,
        getOverrideRulesExtensionElement()!.shadowRoot!.querySelector('span')!
            .textContent.trim());
    assertEquals(
        `${kExtensionName} is controlling this setting`,
        getProxyExtensionElement()!.shadowRoot!.querySelector(
                                                   'span')!.textContent.trim());
    assertNotVisible(getOverrideRulesElement());
    assertNotVisible(getNetworkPolicyElement());
    assertNotVisible(getCombinedPoliciesElement());

    // Clearing extension proxy_override_rules hides its indicator.
    proxySection.setPrefValue('proxy_override_rules', []);
    flush();

    assertNotVisible(getOverrideRulesExtensionElement());
    assertVisible(getProxyExtensionElement());
  });
});
