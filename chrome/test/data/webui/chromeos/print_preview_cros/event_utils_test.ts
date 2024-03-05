// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/utils/event_utils.js';

import {createCustomEvent} from 'chrome://os-print/js/utils/event_utils.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('EventUtils', () => {
  test('createCustomEvent', () => {
    const expectedEventName = 'custom-event';
    const event: CustomEvent<void> = createCustomEvent(expectedEventName);

    assertEquals(
        expectedEventName, event.type,
        `Event type should be ${expectedEventName}`);
    assertTrue(event.bubbles, 'Event should bubble');
    assertTrue(event.composed, 'Event should be composed');
  });
});
