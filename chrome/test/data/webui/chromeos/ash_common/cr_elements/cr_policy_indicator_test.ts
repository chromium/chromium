// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import './cr_policy_strings.js';

import {CrPolicyIndicatorElement} from 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import {CrPolicyIndicatorType} from 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator_mixin.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
// clang-format on

/** @fileoverview Suite of tests for cr-policy-indicator. */
suite('CrPolicyIndicator', function() {
  let indicator: CrPolicyIndicatorElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

    // <if expr="chromeos_ash">
    indicator.indicatorType = CrPolicyIndicatorType.OWNER;
    indicator.indicatorSourceName = 'foo@example.com';

    assertEquals('cr:person', icon.iconClass);
    assertEquals('owner: foo@example.com', icon.tooltipText);
    // </if>

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
