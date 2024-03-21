// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import './cr_policy_strings.js';

import type {CrPolicyIndicatorElement} from 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_types.js';
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

  function getIconTooltipText(): string {
    const icon = indicator.shadowRoot!.querySelector('cr-tooltip-icon');
    assertTrue(!!icon);
    return icon.tooltipText;
  }

  test('none', function() {
    const icon = indicator.shadowRoot!.querySelector('cr-tooltip-icon')!;
    assertTrue(icon.hidden);
  });

  test('default indicator is blank', function() {
    assertEquals(CrPolicyIndicatorType.NONE, indicator.indicatorType);
    assertFalse(indicator.indicatorVisible);
  });

  test('policy-controlled indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.USER_POLICY;

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr20:domain', indicator.indicatorIcon);
    assertEquals('policy', getIconTooltipText());
  });

  test('parent-controlled indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.PARENT;

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr20:kite', indicator.indicatorIcon);
    assertEquals('parent', getIconTooltipText());
  });

  test('child-restriction indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.CHILD_RESTRICTION;

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr20:kite', indicator.indicatorIcon);
    assertEquals('Restricted for child', getIconTooltipText());
  });

  test('recommended indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.RECOMMENDED;

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr20:domain', indicator.indicatorIcon);
    assertEquals('differs', getIconTooltipText());
  });

  test('extension indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.EXTENSION;
    indicator.indicatorSourceName = 'Extension name';

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr:extension', indicator.indicatorIcon);
    assertEquals('extension: Extension name', getIconTooltipText());
  });

  test('extension indicator without extension name', function() {
    indicator.indicatorType = CrPolicyIndicatorType.EXTENSION;
    indicator.indicatorSourceName = '';

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr:extension', indicator.indicatorIcon);
    assertEquals('extension', getIconTooltipText());
  });

  // <if expr="chromeos_ash">
  test('primary-user controlled indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.PRIMARY_USER;
    indicator.indicatorSourceName = 'user@example.com';

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr:group', indicator.indicatorIcon);
    assertEquals('shared: user@example.com', getIconTooltipText());
  });
  // </if>

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
