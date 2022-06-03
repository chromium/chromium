// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/services/network/public/mojom/ip_address.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-lite.js';
// #import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-lite.js';

// #import {NetworkConfigElementBehavior} from 'chrome://resources/cr_components/chromeos/network/network_config_element_behavior.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('CrComponentsNetworkConfigElementBehaviorTest', function() {
  /** @type {!NetworkConfigElementBehavior} */
  let config;

  /** @polymerBehavior */
  const TestNetworkPolicyEnforcer = {
    properties: {
      /** True if the policy should be enforced. */
      enforced: {
        type: Boolean,
        value: false,
      },
    },

    /** @override */
    isNetworkPolicyEnforced(policy) {
      // In tests, toggle |policy| and |enforced| to elicit different behaviors.
      return !!policy && this.enforced;
    },
  };

  suiteSetup(function() {
    Polymer({
      is: 'test-network-config-element',

      behaviors: [
        NetworkConfigElementBehavior,
        TestNetworkPolicyEnforcer,
      ],

      properties: {
        showPolicyIndicator: {
          type: Boolean,
          value: false,
          computed: 'getDisabled_(disabled, property)',
        }
      },
    });
  });

  setup(function() {
    config = document.createElement('test-network-config-element');
    document.body.appendChild(config);
    Polymer.dom.flush();
  });

  test('Policy indicator states', function() {
    config.disabled = false;
    config.enforced = false;
    config.property = null;
    assertFalse(config.showPolicyIndicator);

    config.disabled = false;
    config.enforced = true;
    config.property = null;
    assertFalse(config.showPolicyIndicator);

    config.disabled = true;
    config.enforced = false;
    config.property = null;
    assertTrue(config.showPolicyIndicator);

    config.disabled = true;
    config.enforced = true;
    config.property = null;
    assertTrue(config.showPolicyIndicator);

    config.disabled = false;
    config.enforced = false;
    config.property = OncMojo.createManagedString('policy');
    assertFalse(config.showPolicyIndicator);

    config.disabled = false;
    config.enforced = true;
    config.property = OncMojo.createManagedString('policy');
    assertTrue(config.showPolicyIndicator);

    config.disabled = true;
    config.enforced = false;
    config.property = OncMojo.createManagedString('policy');
    assertTrue(config.showPolicyIndicator);

    config.disabled = true;
    config.enforced = true;
    config.property = OncMojo.createManagedString('policy');
    assertTrue(config.showPolicyIndicator);
  });
});
