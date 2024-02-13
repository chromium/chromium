// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip.top-chrome/alert_indicator.js';

import type {AlertIndicatorElement} from 'chrome://tab-strip.top-chrome/alert_indicator.js';
import {assertEquals, assertFalse, assertNotEquals} from 'chrome://webui-test/chai_assert.js';

suite('AlertIndicator', () => {
  let alertIndicatorElement: AlertIndicatorElement;

  let alertIndicatorStyle: CSSStyleDeclaration;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    alertIndicatorElement = document.createElement('tabstrip-alert-indicator');
    document.body.appendChild(alertIndicatorElement);

    alertIndicatorStyle = window.getComputedStyle(alertIndicatorElement);
  });

  test('fades in on show', () => {
    alertIndicatorElement.overrideFadeDurationForTesting(0);
    alertIndicatorElement.show();
    assertEquals(alertIndicatorStyle.opacity, '1');
    assertEquals(alertIndicatorStyle.maxWidth, '16px');
  });

  test('fades out on hide', async () => {
    alertIndicatorElement.overrideFadeDurationForTesting(0);
    const hideAnimation = alertIndicatorElement.hide();
    assertEquals(alertIndicatorStyle.opacity, '0');
    assertEquals(alertIndicatorStyle.maxWidth, '0px');
    await hideAnimation;
    assertFalse(alertIndicatorElement.isConnected);
  });

  test('multiple calls to show only animates once', () => {
    alertIndicatorElement.overrideFadeDurationForTesting(1000);
    alertIndicatorElement.show();

    alertIndicatorElement.overrideFadeDurationForTesting(0);
    alertIndicatorElement.show();
    assertNotEquals(alertIndicatorStyle.opacity, '1');
    assertNotEquals(alertIndicatorStyle.maxWidth, '16px');
  });

  test('multiple calls to hide only animates once', () => {
    alertIndicatorElement.overrideFadeDurationForTesting(1000);
    alertIndicatorElement.hide();

    alertIndicatorElement.overrideFadeDurationForTesting(0);
    alertIndicatorElement.hide();
    assertNotEquals(alertIndicatorStyle.opacity, '0');
    assertNotEquals(alertIndicatorStyle.maxWidth, '0px');
  });

  test(
      'calls to show the element while animating to hide cancels ' +
          'the hide animation',
      () => {
        alertIndicatorElement.overrideFadeDurationForTesting(1000);
        // This hide promise will be rejected, so catch the rejection.
        alertIndicatorElement.hide().then(() => {}, () => {});

        alertIndicatorElement.overrideFadeDurationForTesting(0);
        alertIndicatorElement.show();
        assertEquals(alertIndicatorStyle.opacity, '1');
        assertEquals(alertIndicatorStyle.maxWidth, '16px');
      });

  test(
      'calls to hide the element while animating to show cancels ' +
          'the show animation',
      async () => {
        alertIndicatorElement.overrideFadeDurationForTesting(1000);
        alertIndicatorElement.show();

        alertIndicatorElement.overrideFadeDurationForTesting(0);
        const hideAnimation = alertIndicatorElement.hide();
        assertEquals(alertIndicatorStyle.opacity, '0');
        assertEquals(alertIndicatorStyle.maxWidth, '0px');

        await hideAnimation;
        assertFalse(alertIndicatorElement.isConnected);
      });
});
