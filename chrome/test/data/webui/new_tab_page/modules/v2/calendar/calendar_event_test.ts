// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CalendarEventElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

// Microseconds between windows and unix epoch.
const kWindowsToUnixEpochOffset: bigint = 11644473600000000n;

suite('NewTabPageModulesCalendaEventTest', () => {
  let element: CalendarEventElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = new CalendarEventElement();
    document.body.append(element);
  });

  test('event is visible', async () => {
    const title = 'Test Event 1';

    // Create a test startTime and turn it into microseconds from
    // Windows epoch.
    const startTime = (new Date('02 Feb 2024 03:04')).valueOf();
    const convertedStartTime =
        (BigInt(startTime) * 1000n) + kWindowsToUnixEpochOffset;

    element.event = {
      title,
      startTime: {internalValue: convertedStartTime},
    };
    await waitAfterNextRender(element);

    // Assert.
    assertTrue(isVisible(element));
    assertTrue(isVisible(element.$.title));
    assertTrue(isVisible(element.$.startTime));
    assertEquals(element.$.title.innerHTML, title);
    assertEquals(element.$.startTime.innerHTML, '3:04am');
  });
});
