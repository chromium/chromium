// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network/network_config_toggle.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NetworkConfigToggleTest', function() {
  /** @type {!NetworkConfigToggle|undefined} */
  let configToggle;

  setup(function() {
    configToggle = document.createElement('network-config-toggle');
    document.body.appendChild(configToggle);
    flush();
  });

  test('Policy on left', function() {
    assertFalse(configToggle.policyOnLeft);

    const rightIndicator = configToggle.$$('cr-policy-network-indicator-mojo');
    assertFalse(rightIndicator.classList.contains('left'));

    // Use sibling selector to assert correct position (on the right).
    let toggle =
        configToggle.$$('cr-toggle + cr-policy-network-indicator-mojo');
    assertTrue(!!toggle);

    configToggle.policyOnLeft = true;
    flush();

    const leftIndicator = configToggle.$$('cr-policy-network-indicator-mojo');
    assertTrue(leftIndicator.classList.contains('left'));

    // Use general sibling selector to assert the indicator is to the left of
    // the toggle.
    toggle = configToggle.$$('cr-policy-network-indicator-mojo ~ cr-toggle');
    assertTrue(!!toggle);

    // The right indicator exists but is not displayed.
    assertEquals('none', rightIndicator.style.display);
  });
});
