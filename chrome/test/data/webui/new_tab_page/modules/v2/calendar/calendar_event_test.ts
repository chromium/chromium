// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CalendarEventElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createEvent} from './test_support.js';

// Microseconds between windows and unix epoch.
const kWindowsToUnixEpochOffset: bigint = 11644473600000000n;

suite('NewTabPageModulesCalendaEventTest', () => {
  let element: CalendarEventElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = new CalendarEventElement();
    element.event = createEvent(1);
    element.expanded = false;
    document.body.append(element);
    await waitAfterNextRender(element);
  });

  test('event is visible', async () => {
    // Create a test startTime and turn it into microseconds from
    // Windows epoch.
    const startTime = (new Date('02 Feb 2024 03:04')).valueOf();
    const convertedStartTime =
        (BigInt(startTime) * 1000n) + kWindowsToUnixEpochOffset;

    element.event =
        createEvent(1, {startTime: {internalValue: convertedStartTime}});
    await waitAfterNextRender(element);

    // Assert.
    assertTrue(isVisible(element));
    assertTrue(isVisible(element.$.title));
    assertTrue(isVisible(element.$.startTime));
    assertEquals(element.$.title.innerHTML, 'Test Event 1');
    assertEquals(element.$.startTime.innerHTML, '3:04am');
    assertEquals(element.$.header.href, element.event.url.url);
  });

  test('time status hidden if not expanded', async () => {
    const startTime = Date.now().valueOf();
    const startTimeFromUnixEpoch =
        (BigInt(startTime) * 1000n) + kWindowsToUnixEpochOffset;

    element.event =
        createEvent(1, {startTime: {internalValue: startTimeFromUnixEpoch}});
    await waitAfterNextRender(element);

    // Assert.
    assertFalse(isVisible(element.$.timeStatus));
    assertEquals(element.$.timeStatus.innerText, '');
  });

  test('time status displays correctly', async () => {
    const startTime = Date.now().valueOf();
    let startTimeFromUnixEpoch =
        (BigInt(startTime) * 1000n) + kWindowsToUnixEpochOffset;

    element.event =
        createEvent(1, {startTime: {internalValue: startTimeFromUnixEpoch}});
    element.expanded = true;
    await waitAfterNextRender(element);

    assertTrue(isVisible(element.$.timeStatus));
    assertEquals(element.$.timeStatus.innerText, 'In Progress');

    // Make the event 5 minutes in the future.
    startTimeFromUnixEpoch += (1000000n * 60n * 5n);
    element.event =
        createEvent(1, {startTime: {internalValue: startTimeFromUnixEpoch}});
    await waitAfterNextRender(element);

    assertEquals(element.$.timeStatus.innerText, 'In 5 min');

    // Make the event 1 hour in the future.
    startTimeFromUnixEpoch += (1000000n * 60n * 60n);
    element.event =
        createEvent(1, {startTime: {internalValue: startTimeFromUnixEpoch}});
    await waitAfterNextRender(element);

    assertEquals(element.$.timeStatus.innerText, 'In 1 hr');
  });

  test('Expanded event shows extra info', async () => {
    element.expanded = true;
    await waitAfterNextRender(element);

    // Assert.
    const locationElement = $$(element, '#location');
    const attachmentsElement = $$(element, '#attachments');
    const conferenceElement = $$(element, '#conference');
    assertTrue(!!locationElement);
    assertTrue(!!attachmentsElement);
    assertTrue(!!conferenceElement);
    assertTrue(isVisible(locationElement));
    assertTrue(isVisible(attachmentsElement));
    assertTrue(isVisible(conferenceElement));

    const attachmentChips = element.shadowRoot!.querySelectorAll('cr-chip');
    assertEquals(attachmentChips.length, 3);
  });

  test('Non-expanded event hides extra info', async () => {
    // Assert.
    const locationElement = $$(element, '#location');
    const attachmentsElement = $$(element, '#attachments');
    const conferenceElement = $$(element, '#conference');
    assertTrue(!locationElement);
    assertTrue(!attachmentsElement);
    assertTrue(!conferenceElement);
  });

  test('location hidden if empty', async () => {
    element.expanded = true;
    element.event = createEvent(1, {location: ''});
    await waitAfterNextRender(element);

    // Assert.
    const locationElement = $$(element, '#location');
    assertTrue(!!locationElement);
    assertFalse(isVisible(locationElement));
  });

  test('attachments hidden if empty', async () => {
    element.expanded = true;
    element.event = createEvent(1, {attachments: []});
    await waitAfterNextRender(element);

    // Assert.
    const attachmentsElement = $$(element, '#attachments');
    assertTrue(!!attachmentsElement);
    assertFalse(isVisible(attachmentsElement));
  });

  test('conference button hidden if empty', async () => {
    element.expanded = true;
    element.event = createEvent(1, {conferenceUrl: {url: ''}});
    await waitAfterNextRender(element);

    // Assert.
    const conferenceElement = $$(element, '#conference');
    assertTrue(!!conferenceElement);
    assertFalse(isVisible(conferenceElement));
  });
});
