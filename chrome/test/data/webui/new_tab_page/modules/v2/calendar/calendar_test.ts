// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CalendarEvent} from 'chrome://new-tab-page/google_calendar.mojom-webui.js';
import {CalendarElement} from 'chrome://new-tab-page/lazy_load.js';
import {WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
    await microtasksFinished();

    // Assert.
    const eventElements =
        element.shadowRoot!.querySelectorAll('ntp-calendar-event');
    assertEquals(eventElements.length, numEvents);
    eventElements.forEach((element) => {
      assertTrue(isVisible(element));
    });
  });

  test('first event that is not over is expanded when no overlap', async () => {
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
    await microtasksFinished();

    // Assert.
    const eventElements =
        element.shadowRoot!.querySelectorAll('ntp-calendar-event');
    assertEquals(eventElements.length, 3);
    const expandedEvent = eventElements[1];
    assertTrue(expandedEvent!.hasAttribute('expanded'));
  });

  test('prioritize expanding concurrent event', async () => {
    const events: CalendarEvent[] = [];
    const mockTime = (new Date('2024-07-01T03:00:00')).valueOf();
    windowProxy.setResultFor('now', mockTime);
    // Event that ends in the next 5 minutes.
    events.push(createEvent(0, {
      startTime: toTime(new Date(mockTime - (30 * 60000))),
      endTime: toTime(new Date(mockTime + (5 * 60000))),
    }));
    // Event that starts in 5 minutes.
    events.push(createEvent(1, {
      startTime: toTime(new Date(mockTime + (5 * 60000))),
      endTime: toTime(new Date(mockTime + (30 * 60000))),
    }));
    element.events = events;
    await microtasksFinished();

    // Assert.
    const eventElements =
        element.shadowRoot!.querySelectorAll('ntp-calendar-event');
    assertEquals(eventElements.length, 2);
    const expandedEvent = eventElements[1];
    assertTrue(expandedEvent!.hasAttribute('expanded'));
  });

  test('prioritize accepted event', async () => {
    const events: CalendarEvent[] = [];
    const mockTime = (new Date('2024-07-01T03:00:00')).valueOf();
    windowProxy.setResultFor('now', mockTime);
    events.push(createEvent(0, {
      startTime: toTime(new Date(mockTime + (30 * 60000))),
      endTime: toTime(new Date(mockTime + (60 * 60000))),
      isAccepted: false,
    }));
    events.push(createEvent(1, {
      startTime: toTime(new Date(mockTime + (30 * 60000))),
      endTime: toTime(new Date(mockTime + (60 * 60000))),
      isAccepted: true,
    }));
    element.events = events;
    await microtasksFinished();

    // Assert.
    const eventElements =
        element.shadowRoot!.querySelectorAll('ntp-calendar-event');
    assertEquals(eventElements.length, 2);
    const expandedEvent = eventElements[1];
    assertTrue(expandedEvent!.hasAttribute('expanded'));
  });

  test('prioritize event with other attendee', async () => {
    const events: CalendarEvent[] = [];
    const mockTime = (new Date('2024-07-01T03:00:00')).valueOf();
    windowProxy.setResultFor('now', mockTime);
    events.push(createEvent(0, {
      startTime: toTime(new Date(mockTime + (30 * 60000))),
      endTime: toTime(new Date(mockTime + (60 * 60000))),
      hasOtherAttendee: false,
    }));
    events.push(createEvent(1, {
      startTime: toTime(new Date(mockTime + (30 * 60000))),
      endTime: toTime(new Date(mockTime + (60 * 60000))),
    }));
    element.events = events;
    await microtasksFinished();

    // Assert.
    const eventElements =
        element.shadowRoot!.querySelectorAll('ntp-calendar-event');
    assertEquals(eventElements.length, 2);
    const expandedEvent = eventElements[1];
    assertTrue(expandedEvent!.hasAttribute('expanded'));
  });

  test('do not expand any meetings if they are all over', async () => {
    const events: CalendarEvent[] = [];
    const mockTime = (new Date('2024-07-01T03:00:00')).valueOf();
    windowProxy.setResultFor('now', mockTime);
    events.push(createEvent(0, {
      startTime: toTime(new Date(mockTime - (30 * 60000))),
      endTime: toTime(new Date(mockTime)),
    }));
    element.events = events;
    await microtasksFinished();

    // Assert.
    const eventElements =
        element.shadowRoot!.querySelectorAll('ntp-calendar-event');
    assertEquals(eventElements.length, 1);
    assertFalse(eventElements[0]!.hasAttribute('expanded'));
  });

  test('see more link is displayed', async () => {
    element.calendarLink = 'https://foo.com/';
    await microtasksFinished();

    // Assert.
    assertTrue(isVisible(element.$.seeMore));
    const anchor = element.$.seeMore.querySelector<HTMLAnchorElement>('a');
    assertTrue(!!anchor);
    assertEquals('https://foo.com/', anchor!.href);
    assertEquals('See more', anchor!.innerText);
  });

  test('double booked events are marked', async () => {
    const events: CalendarEvent[] = [];
    const mockTime = (new Date('2024-07-01T03:00:00')).valueOf();
    windowProxy.setResultFor('now', mockTime);
    // Create 3 events with the same start time, but each ends 30 minutes
    // later.
    for (let i = 0; i < 3; ++i) {
      const endTimeMs = mockTime + ((i + 1) * 30 * 60000);
      events.push(createEvent(i, {
        startTime: toTime(new Date(mockTime)),
        endTime: toTime(new Date(endTimeMs)),
        isAccepted: i === 1,
      }));
    }
    element.events = events;
    await microtasksFinished();

    // Assert.
    const eventElements =
        element.shadowRoot!.querySelectorAll('ntp-calendar-event');
    assertEquals(3, eventElements.length);
    assertTrue(eventElements[1]!.hasAttribute('expanded'));
    assertTrue(eventElements[0]!.hasAttribute('double-booked'));
    assertTrue(eventElements[2]!.hasAttribute('double-booked'));
  });
});
