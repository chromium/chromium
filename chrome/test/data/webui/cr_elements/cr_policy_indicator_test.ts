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
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
// clang-format on

/** @fileoverview Suite of tests for cr-policy-indicator. */
suite('CrPolicyIndicator', function() {
  let indicator: CrPolicyIndicatorElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    indicator = document.createElement('cr-policy-indicator');
    document.body.appendChild(indicator);
    return microtasksFinished();
  });

  function getIconTooltipText(): string {
    const icon = indicator.shadowRoot!.querySelector('cr-tooltip-icon');
    assertTrue(!!icon);
    return icon.tooltipText;
  }

  function getIconVisible(): boolean {
    const icon = indicator.shadowRoot!.querySelector('cr-tooltip-icon');
    assertTrue(!!icon);
    return isVisible(icon);
  }

  function getIconClass(): string {
    const icon = indicator.shadowRoot!.querySelector('cr-tooltip-icon');
    assertTrue(!!icon);
    return icon.iconClass;
  }

  test('none', function() {
    const icon = indicator.shadowRoot!.querySelector('cr-tooltip-icon')!;
    assertTrue(icon.hidden);
  });

  test('default indicator is blank', function() {
    assertEquals(CrPolicyIndicatorType.NONE, indicator.indicatorType);
    assertFalse(getIconVisible());
  });

  test('policy-controlled indicator', async () => {
    indicator.indicatorType = CrPolicyIndicatorType.USER_POLICY;
    await microtasksFinished();

    assertTrue(getIconVisible());
    assertEquals('cr20:domain', getIconClass());
    assertEquals('policy', getIconTooltipText());
  });

  test('parent-controlled indicator', async () => {
    indicator.indicatorType = CrPolicyIndicatorType.PARENT;
    await microtasksFinished();

    assertTrue(getIconVisible());
    assertEquals('cr20:kite', getIconClass());
    assertEquals('parent', getIconTooltipText());
  });

  test('child-restriction indicator', async () => {
    indicator.indicatorType = CrPolicyIndicatorType.CHILD_RESTRICTION;
    await microtasksFinished();

    assertTrue(getIconVisible());
    assertEquals('cr20:kite', getIconClass());
    assertEquals('Restricted for child', getIconTooltipText());
  });

  test('recommended indicator', async () => {
    indicator.indicatorType = CrPolicyIndicatorType.RECOMMENDED;
    await microtasksFinished();

    assertTrue(getIconVisible());
    assertEquals('cr20:domain', getIconClass());
    assertEquals('differs', getIconTooltipText());
  });

  test('extension indicator', async () => {
    indicator.indicatorType = CrPolicyIndicatorType.EXTENSION;
    indicator.indicatorSourceName = 'Extension name';
    await microtasksFinished();

    assertTrue(getIconVisible());
    assertEquals('cr:extension', getIconClass());
    assertEquals('extension: Extension name', getIconTooltipText());
  });

  test('extension indicator without extension name', async () => {
    indicator.indicatorType = CrPolicyIndicatorType.EXTENSION;
    indicator.indicatorSourceName = '';
    await microtasksFinished();

    assertTrue(getIconVisible());
    assertEquals('cr:extension', getIconClass());
    assertEquals('extension', getIconTooltipText());
  });

  // <if expr="chromeos_ash">
  test('primary-user controlled indicator', async () => {
    indicator.indicatorType = CrPolicyIndicatorType.PRIMARY_USER;
    indicator.indicatorSourceName = 'user@example.com';
    await microtasksFinished();

    assertTrue(getIconVisible());
    assertEquals('cr:group', getIconClass());
    assertEquals('shared: user@example.com', getIconTooltipText());
  });
  // </if>

  test('indicator', async () => {
    const icon = indicator.shadowRoot!.querySelector('cr-tooltip-icon')!;
    indicator.indicatorType = CrPolicyIndicatorType.USER_POLICY;
    await microtasksFinished();

    assertFalse(icon.hidden);
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals('policy', icon.tooltipText);

    // <if expr="chromeos_ash">
    indicator.indicatorType = CrPolicyIndicatorType.OWNER;
    indicator.indicatorSourceName = 'foo@example.com';
    await microtasksFinished();

    assertEquals('cr:person', icon.iconClass);
    assertEquals('owner: foo@example.com', icon.tooltipText);
    // </if>

    indicator.indicatorType = CrPolicyIndicatorType.PARENT;
    await microtasksFinished();

    assertFalse(icon.hidden);
    assertEquals('cr20:kite', icon.iconClass);
    assertEquals('parent', icon.tooltipText);

    indicator.indicatorType = CrPolicyIndicatorType.CHILD_RESTRICTION;
    await microtasksFinished();

    assertFalse(icon.hidden);
    assertEquals('cr20:kite', icon.iconClass);
    assertEquals('Restricted for child', icon.tooltipText);
  });
});
