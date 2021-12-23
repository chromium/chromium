// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.m.js';
import './cr_policy_strings.js';

import {CrPolicyIndicatorElement} from 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';
import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
// clang-format on

/** @fileoverview Suite of tests for cr-policy-indicator. */
suite('CrPolicyIndicator', function() {
  let indicator: CrPolicyIndicatorElement;

  setup(function() {
    document.body.innerHTML = '';
    indicator = document.createElement('cr-policy-indicator');
    document.body.appendChild(indicator);
  });

  test('none', function() {
    const icon = indicator.shadowRoot!.querySelector('cr-tooltip-icon')!;
    assertTrue(icon.hidden);
  });

  test('indicator', function() {
    const icon = indicator.shadowRoot!.querySelector('cr-tooltip-icon')!;
    indicator.indicatorType = CrPolicyIndicatorType.USER_POLICY;

    assertFalse(icon.hidden);
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals('policy', icon.tooltipText);

    if (isChromeOS) {
      indicator.indicatorType = CrPolicyIndicatorType.OWNER;
      indicator.indicatorSourceName = 'foo@example.com';

      assertEquals('cr:person', icon.iconClass);
      assertEquals('owner: foo@example.com', icon.tooltipText);
    }

    indicator.indicatorType = CrPolicyIndicatorType.PARENT;

    assertFalse(icon.hidden);
    assertEquals('cr20:kite', icon.iconClass);
    assertEquals('parent', icon.tooltipText);

    indicator.indicatorType = CrPolicyIndicatorType.CHILD_RESTRICTION;

    assertFalse(icon.hidden);
    assertEquals('cr20:kite', icon.iconClass);
    assertEquals('Restricted for child', icon.tooltipText);
  });
});
