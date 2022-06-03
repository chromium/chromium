// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_components/chromeos/network/network_config_input.m.js';

// #import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('NetworkConfigInputTest', function() {
  /** @type {!NetworkConfigInput|undefined} */
  let configInput;

  setup(function() {
    configInput = document.createElement('network-config-input');
    document.body.appendChild(configInput);
    Polymer.dom.flush();
  });

  test('Enter key propagates up enter event', function(done) {
    // Set up a listener to assert the 'enter' event fired.
    configInput.addEventListener('enter', function() {
      done();
    });

    // Simulate pressing enter on the input element.
    const inputEl = configInput.$$('cr-input');
    MockInteractions.keyEventOn(
        inputEl, 'keypress', /* keyCode */ 13, /* modifiers */ undefined,
        'Enter');
  });
});
