// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/text_badge.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {BadgeType, TextBadgeElement} from 'chrome://diagnostics/text_badge.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('textBadgeTestSuite', function() {
  /** @type {?TextBadgeElement} */
  let textBadgeElement = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
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
      const textBadge = textBadgeElement.shadowRoot.querySelector('#textBadge');
      assertEquals(badgeType, textBadge.getAttribute('class'));
      dx_utils.assertTextContains(textBadge.textContent, value);
    });
  });
});
