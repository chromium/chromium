// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for CrPolicyIndicatorBehavior. */

// clang-format off
import './cr_policy_strings.js';

import {CrPolicyIndicatorBehavior, CrPolicyIndicatorType} from 'chrome://resources/ash/common/cr_policy_indicator_behavior.js';
import {isChromeOS} from 'chrome://resources/js/platform.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

// clang-format on

suite('CrPolicyIndicatorBehavior', function() {
  const TestIndicatorElementBase =
      mixinBehaviors([CrPolicyIndicatorBehavior], PolymerElement) as
      {new (): PolymerElement & CrPolicyIndicatorBehavior};

  class TestIndicatorElement extends TestIndicatorElementBase {
    static get is() {
      return 'test-indicator';
    }
  }
  customElements.define(TestIndicatorElement.is, TestIndicatorElement);

  let indicator: TestIndicatorElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    indicator =
        document.createElement('test-indicator') as TestIndicatorElement;
    document.body.appendChild(indicator);
  });

  test('default indicator is blank', function() {
    assertEquals(CrPolicyIndicatorType.NONE, indicator.indicatorType);
    assertFalse(indicator.indicatorVisible);
  });

  test('policy-controlled indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.USER_POLICY;

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr20:domain', indicator.indicatorIcon);
    assertEquals(
        'policy',
        indicator.getIndicatorTooltip(
            indicator.indicatorType, indicator.indicatorSourceName));
  });

  test('parent-controlled indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.PARENT;

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr20:kite', indicator.indicatorIcon);
    assertEquals(
        'parent',
        indicator.getIndicatorTooltip(
            indicator.indicatorType, indicator.indicatorSourceName));
  });

  test('child-restriction indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.CHILD_RESTRICTION;

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr20:kite', indicator.indicatorIcon);
    assertEquals(
        'Restricted for child',
        indicator.getIndicatorTooltip(
            indicator.indicatorType, indicator.indicatorSourceName));
  });

  test('recommended indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.RECOMMENDED;

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr20:domain', indicator.indicatorIcon);
    assertEquals(
        'matches',
        indicator.getIndicatorTooltip(
            indicator.indicatorType, indicator.indicatorSourceName, true));
    assertEquals(
        'differs',
        indicator.getIndicatorTooltip(
            indicator.indicatorType, indicator.indicatorSourceName, false));
  });

  test('extension indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.EXTENSION;
    indicator.indicatorSourceName = 'Extension name';

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr:extension', indicator.indicatorIcon);
    assertEquals(
        'extension: Extension name',
        indicator.getIndicatorTooltip(
            indicator.indicatorType, indicator.indicatorSourceName));
  });

  test('extension indicator without extension name', function() {
    indicator.indicatorType = CrPolicyIndicatorType.EXTENSION;
    indicator.indicatorSourceName = '';

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr:extension', indicator.indicatorIcon);
    assertEquals(
        'extension',
        indicator.getIndicatorTooltip(
            indicator.indicatorType, indicator.indicatorSourceName));
  });

  if (isChromeOS) {
    test('primary-user controlled indicator', function() {
      indicator.indicatorType = CrPolicyIndicatorType.PRIMARY_USER;
      indicator.indicatorSourceName = 'user@example.com';

      assertTrue(indicator.indicatorVisible);
      assertEquals('cr:group', indicator.indicatorIcon);
      assertEquals(
          'shared: user@example.com',
          indicator.getIndicatorTooltip(
              indicator.indicatorType, indicator.indicatorSourceName));
    });
  }
});
