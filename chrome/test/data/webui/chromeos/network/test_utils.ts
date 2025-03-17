// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NetworkConfigElement} from 'chrome://resources/ash/common/network/network_config.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import type {ConfigProperties, ManagedProperties, SecurityType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import type {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import type {FakeNetworkConfig} from '../fake_network_config_mojom.js';

export function clearBody(): void {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
}

export function createNetworkConfigWithProperties(
    mojoApi: FakeNetworkConfig, properties: ManagedProperties,
    prefilledProperties: ConfigProperties|undefined =
        undefined): NetworkConfigElement {
  mojoApi.setManagedPropertiesForTest(properties);
  clearBody();
  const networkConfig = document.createElement('network-config');
  assertTrue(!!properties.guid);
  networkConfig.guid = properties.guid;
  networkConfig.setManagedPropertiesForTesting(properties);

  if (prefilledProperties !== undefined) {
    networkConfig.prefilledProperties = prefilledProperties;
  }
  return networkConfig;
}

export function createNetworkConfigWithNetworkType(
    type: NetworkType,
    security: SecurityType|undefined = undefined): NetworkConfigElement {
  clearBody();
  const networkConfig = document.createElement('network-config');
  networkConfig.type = OncMojo.getNetworkTypeString(type);
  if (security !== undefined) {
    networkConfig.setSecurityTypeForTesting(security);
  }

  return networkConfig;
}

export function simulateEnterPressedInElement(
    networkConfig: NetworkConfigElement, elementId: string) {
  const element = networkConfig.shadowRoot!.querySelector(`#${elementId}`);
  networkConfig.connectOnEnter = true;
  assertTrue(!!element);
  const event = new CustomEvent('enter', {
    bubbles: true,
    composed: true,
    detail: {path: [element]},
  });
  element.dispatchEvent(event);
}
