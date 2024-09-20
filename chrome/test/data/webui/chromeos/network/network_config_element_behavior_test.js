// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NetworkConfigElementBehavior} from 'chrome://resources/ash/common/network/network_config_element_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {flush, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

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

  const TestElementBase = mixinBehaviors(
      [NetworkConfigElementBehavior, TestNetworkPolicyEnforcer],
      PolymerElement);

  class TestNetworkConfigElement extends TestElementBase {
    static get is() {
      return 'test-network-config-element';
    }

    static get properties() {
      return {
        showPolicyIndicator: {
          type: Boolean,
          value: false,
          computed: 'getDisabled_(disabled, property)',
        },
      };
    }
  }
  customElements.define(TestNetworkConfigElement.is, TestNetworkConfigElement);

  setup(function() {
    config = document.createElement('test-network-config-element');
    document.body.appendChild(config);
    flush();
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

  test('Pre-filled values behavior', function() {
    config.property = null;

    const testCases = [
      {prefilledValue: null, shouldBeValid: false},
      {prefilledValue: '', shouldBeValid: true},
      {prefilledValue: 0, shouldBeValid: true},
      {prefilledValue: 'something', shouldBeValid: true},
    ];
    for (const {prefilledValue, shouldBeValid} of testCases) {
      config.prefilledValue = prefilledValue;
      if (shouldBeValid) {
        assertTrue(config.readonly);
        assertEquals(config.value, config.prefilledValue);
      } else {
        assertFalse(config.readonly);
        assertNotEquals(config.value, config.prefilledValue);
      }
    }
  });
});
