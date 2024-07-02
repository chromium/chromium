// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CalendarEvent} from 'chrome://new-tab-page/google_calendar.mojom-webui.js';
import {CalendarElement} from 'chrome://new-tab-page/lazy_load.js';
import {WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../../test_support.js';

import {createEvent, createEvents, toTime} from './test_support.js';

suite('NewTabPageModulesCalendarTest', () => {
  let element: CalendarElement;
  let windowProxy: TestMock<WindowProxy>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    windowProxy = installMock(WindowProxy);
    element = new CalendarElement();
    document.body.append(element);
  });

  test('events are listed', async () => {
    const numEvents = 2;
    element.events = createEvents(numEvents);
    await waitAfterNextRender(element);

    // Assert.
    const eventElements =
        element.shadowRoot!.querySelectorAll('ntp-calendar-event');
    assertEquals(eventElements.length, numEvents);
    eventElements.forEach((element) => {
      assertTrue(isVisible(element));
    });
  });

  test('first event that is not over is expanded', async () => {
    const events: CalendarEvent[] = [];
    // Create 3 concurrent events, each one 30 minutes long.
    // The first event starts 30 minutes ago and ends now.
    const mockTime = (new Date('2024-07-01T03:00:00')).valueOf();
    windowProxy.setResultFor('now', mockTime);
    for (let i = 0; i < 3; ++i) {
      const startTimeMs = mockTime + ((i - 1) * 30 * 60000);
      const endTimeMs = mockTime + (i * 30 * 60000);
      events.push(createEvent(i, {
        startTime: toTime(new Date(startTimeMs)),
        endTime: toTime(new Date(endTimeMs)),
      }));
    }
    element.events = events;
    await waitAfterNextRender(element);

    // Assert.
    const eventElements =
        element.shadowRoot!.querySelectorAll('ntp-calendar-event');
    assertEquals(eventElements.length, 3);
    const expandedEvent = eventElements[1];
    assertTrue(expandedEvent!.hasAttribute('expanded'));
  });

  test('see more link is displayed', async () => {
    element.calendarLink = 'https://foo.com/';

    // Assert.
    assertTrue(isVisible(element.$.seeMore));
    const anchor = element.$.seeMore.querySelector<HTMLAnchorElement>('a');
    assertTrue(!!anchor);
    assertEquals(anchor!.href, 'https://foo.com/');
    assertEquals(anchor!.innerText, 'See more');
  });
});
