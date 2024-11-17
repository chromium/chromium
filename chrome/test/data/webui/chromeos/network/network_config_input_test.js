// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network/network_config_input.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {keyEventOn} from 'chrome://webui-test/keyboard_mock_interactions.js';

suite('NetworkConfigInputTest', function() {
  /** @type {!NetworkConfigInput|undefined} */
  let configInput;

  setup(function() {
    configInput = document.createElement('network-config-input');
    document.body.appendChild(configInput);
    flush();
  });

  test('Enter key propagates up enter event', function(done) {
    // Set up a listener to assert the 'enter' event fired.
    configInput.addEventListener('enter', function() {
      done();
    });

    // Simulate pressing enter on the input element.
    const inputEl = configInput.$$('cr-input');
    keyEventOn(
        inputEl, 'keypress', /* keyCode */ 13, /* modifiers */ undefined,
        'Enter');
  });
});
