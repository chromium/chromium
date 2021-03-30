// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/text_badge.js';

import {BadgeType} from 'chrome://diagnostics/text_badge.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function textBadgeTestSuite() {
  /** @type {?TextBadgeElement} */
  let textBadgeElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    if (textBadgeElement) {
      textBadgeElement.remove();
    }
    textBadgeElement = null;
  });

  /**
   * @param {!BadgeType} badgeType
   * @param {string} value
   */
  function initializeBadge(badgeType, value) {
    assertFalse(!!textBadgeElement);

    textBadgeElement =
        /** @type {!TextBadgeElement} */ (document.createElement('text-badge'));
    assertTrue(!!textBadgeElement);
    textBadgeElement.badgeType = badgeType;
    textBadgeElement.value = value;
    document.body.appendChild(textBadgeElement);

    return flushTasks();
  }

  test('InitializeBadge', () => {
    const badgeType = BadgeType.QUEUED;
    const value = 'Test value';
    return initializeBadge(badgeType, value).then(() => {
      const textBadge = textBadgeElement.$$('#textBadge');
      assertEquals(badgeType, textBadge.getAttribute('class'));
      dx_utils.assertTextContains(textBadge.textContent, value);
    });
  });
}
