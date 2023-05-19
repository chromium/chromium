// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('NetworkProxySection', function() {
  /** @type {!NetworkProxySectionElement|undefined} */
  let proxySection;

  setup(function() {
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
      'ash': {
        'lacros_proxy_controlling_extension': {
          key: 'ash.lacros_proxy_controlling_extension',
          type: chrome.settingsPrivate.PrefType.DICTIONARY,
          value: {},
        },
      },
    };
    document.body.appendChild(proxySection);
    flush();
  });

  test('Visibility of Allow Shared toggle', function() {
    const allowSharedToggle = proxySection.$.allowShared;
    assertTrue(!!allowSharedToggle);

    proxySection.managedProperties = {
      source: OncSource.kNone,
    };
    assertTrue(allowSharedToggle.hidden);

    proxySection.managedProperties = {
      source: OncSource.kDevice,
    };
    assertFalse(allowSharedToggle.hidden);

    proxySection.managedProperties = {
      source: OncSource.kDevicePolicy,
    };
    assertFalse(allowSharedToggle.hidden);

    proxySection.managedProperties = {
      source: OncSource.kUser,
    };
    assertTrue(allowSharedToggle.hidden);

    proxySection.managedProperties = {
      source: OncSource.kUserPolicy,
    };
    assertTrue(allowSharedToggle.hidden);
  });

  test('Disabled UI state', function() {
    const allowSharedToggle = proxySection.$.allowShared;
    const networkProxy = proxySection.shadowRoot.querySelector('network-proxy');

    assertFalse(allowSharedToggle.disabled);
    assertTrue(networkProxy.editable);

    proxySection.disabled = true;

    assertTrue(allowSharedToggle.disabled);
    assertFalse(networkProxy.editable);
  });
});
