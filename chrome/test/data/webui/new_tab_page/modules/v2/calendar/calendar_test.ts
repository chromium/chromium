// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CalendarElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {createEvents} from './test_support.js';

suite('NewTabPageModulesGoogleCalendarModuleTest', () => {
  let element: CalendarElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = new CalendarElement();
    document.body.append(element);
  });

  test('events are listed', async () => {
    const numEvents = 2;
    element.events = createEvents(numEvents);
    await waitAfterNextRender(element);

    // Assert.
    const eventElements = element.shadowRoot!.querySelectorAll('p');
    assertEquals(eventElements.length, numEvents);
    for (let i = 0; i < numEvents; i++) {
      assertEquals(eventElements[i]!.innerHTML, `Test Event ${i}`);
    }
  });
});
