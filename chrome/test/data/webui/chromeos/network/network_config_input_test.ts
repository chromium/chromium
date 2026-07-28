// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network/network_config_input.js';

import type {NetworkConfigInputElement} from 'chrome://resources/ash/common/network/network_config_input.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyEventOn} from 'chrome://webui-test/keyboard_mock_interactions.js';

suite('NetworkConfigInputTest', function() {
  let configInput: NetworkConfigInputElement;

  setup(function() {
    configInput = document.createElement('network-config-input');
    document.body.appendChild(configInput);
    flush();
  });

  test('Enter key propagates up enter event', (done) => {
    // Set up a listener to assert the 'enter' event fired.
    configInput.addEventListener('enter', function() {
      done();
    });

    // Simulate pressing enter on the input element.
    const inputEl = configInput.shadowRoot!.querySelector('cr-input');
    assertTrue(!!inputEl);
    keyEventOn(
        inputEl, 'keypress', /* keyCode */ 13, /* modifiers */ undefined,
        'Enter');
  });

  test('Toggling hidden attribute updates element visibility', function() {
    configInput.hidden = false;
    flush();

    const innerInput = configInput.shadowRoot!.querySelector('cr-input');
    const innerPolicyIcon = configInput.shadowRoot!.querySelector(
        'cr-policy-network-indicator-mojo');
    assertTrue(!!innerInput);
    assertTrue(!!innerPolicyIcon);

    // Initially visible when hidden is false.
    assertTrue(configInput.checkVisibility());
    assertTrue(innerInput.checkVisibility());
    assertTrue(innerPolicyIcon.checkVisibility());

    // Setting hidden to true hides host and all shadow DOM children.
    configInput.hidden = true;
    flush();

    assertFalse(configInput.checkVisibility());
    assertFalse(innerInput.checkVisibility());
    assertFalse(innerPolicyIcon.checkVisibility());
  });
});
