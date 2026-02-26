// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://updater/event_list/event_list.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {EventListElement} from 'chrome://updater/event_list/event_list.js';
import type {EventListItemElement} from 'chrome://updater/event_list/event_list_item.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished, whenCheck} from 'chrome://webui-test/test_util.js';

suite('EventListElement', () => {
  let element: EventListElement;

  function clearFilters() {
    element.filterSettings.apps.clear();
    element.filterSettings.eventTypes.clear();
    element.filterSettings.updateOutcomes.clear();
    element.filterSettings.startDate = null;
    element.filterSettings.endDate = null;
    element.updateEventEntries();
    element.requestUpdate();
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('event-list');
    clearFilters();
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('initializes with default values', () => {
    assertEquals(0, element.messages.length);
    assertEquals(
        0, element.shadowRoot.querySelectorAll('event-list-item').length);
  });

  test('parses and displays events', async () => {
    const messages = [
      {
        'eventType': 'UPDATER_PROCESS',
        'eventId': 'p1',
        'deviceUptime': '0',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'START',
        'updaterVersion': '123.0.142.0',
        'timestamp': '13408459200000000',  // 2025-11-24T12:00:00Z
        'scope': 'USER',
      },
      {
        'eventType': 'UPDATER_PROCESS',
        'eventId': 'p1',
        'deviceUptime': '182165200000',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'END',
        'exitCode': '0',
      },
      {
        'eventType': 'INSTALL',
        'eventId': '1',
        'deviceUptime': '1000',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'START',
        'errors': [],
        'appId': '{app1}',
      },
      {
        'eventType': 'INSTALL',
        'eventId': '1',
        'deviceUptime': '2000',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'END',
        'errors': [],
        'version': '1.0',
      },
    ];

    element.messages = messages;
    await microtasksFinished();

    const items = element.shadowRoot.querySelectorAll('event-list-item');
    assertEquals(2, items.length);
    assertEquals('INSTALL', items[0]!.event?.eventType);

    assertFalse(
        !!element.shadowRoot.querySelector('.events-without-dates-label'));
    assertFalse(
        !!element.shadowRoot.querySelector('.events-with-parse-errors-label'));
  });

  test('handles parse errors', async () => {
    const messages = [
      {'invalid': 'data'},
    ];

    element.messages = messages;
    await microtasksFinished();

    return whenCheck(
        element,
        () => element.shadowRoot.querySelector(
                  '.events-with-parse-errors-label') !== null);
  });

  test('handles events without dates', async () => {
    const messages = [{
      'eventType': 'INSTALL',
      'eventId': '1',
      'deviceUptime': '1000',
      'pid': '100',
      'processToken': 'token1',
      'bound': 'START',
      'errors': [],
      'appId': '{app1}',
    }];

    element.messages = messages;
    await microtasksFinished();

    return whenCheck(
        element,
        () => element.shadowRoot.querySelector(
                  '.events-without-dates-label') !== null);
  });

  test('filters events', async () => {
    const messages = [
      {
        'eventType': 'UPDATER_PROCESS',
        'eventId': 'p1',
        'deviceUptime': '0',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'START',
        'updaterVersion': '123.0.142.0',
        'timestamp': '13408459200000000',  // 2025-11-24T12:00:00Z
        'scope': 'USER',
      },
      {
        'eventType': 'UPDATER_PROCESS',
        'eventId': 'p1',
        'deviceUptime': '182165200000',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'END',
        'exitCode': '0',
      },
      {
        'eventType': 'INSTALL',
        'eventId': '1',
        'deviceUptime': '1000',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'START',
        'errors': [],
        'appId': '{app1}',
      },
      {
        'eventType': 'INSTALL',
        'eventId': '1',
        'deviceUptime': '2000',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'END',
        'errors': [],
        'version': '1.0',
      },
      {
        'eventType': 'UPDATE',
        'eventId': '2',
        'deviceUptime': '3000',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'START',
        'errors': [],
        'appId': '{app2}',
      },
      {
        'eventType': 'UPDATE',
        'eventId': '2',
        'deviceUptime': '4000',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'END',
        'errors': [],
        'outcome': 'UPDATED',
        'nextVersion': '2.0',
      },
    ];

    element.messages = messages;
    await microtasksFinished();

    assertEquals(
        3, element.shadowRoot.querySelectorAll('event-list-item').length);

    element.filterSettings.apps.add('{app1}');
    element.updateEventEntries();
    element.requestUpdate();
    await microtasksFinished();

    const items = element.shadowRoot.querySelectorAll('event-list-item');
    assertEquals(1, items.length);
    assertEquals(
        'INSTALL', ((items[0] as EventListItemElement).event as any).eventType);
  });

  test('expands and collapses all', async () => {
    const messages = [
      {
        'eventType': 'UPDATER_PROCESS',
        'eventId': 'p1',
        'deviceUptime': '0',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'START',
        'updaterVersion': '123.0.142.0',
        'timestamp': '13408459200000000',  // 2025-11-24T12:00:00Z
        'scope': 'USER',
      },
      {
        'eventType': 'UPDATER_PROCESS',
        'eventId': 'p1',
        'deviceUptime': '182165200000',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'END',
        'exitCode': '0',
      },
      {
        'eventType': 'INSTALL',
        'eventId': '1',
        'deviceUptime': '1000',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'START',
        'errors': [],
        'appId': '{app1}',
      },
      {
        'eventType': 'INSTALL',
        'eventId': '1',
        'deviceUptime': '2000',
        'pid': '100',
        'processToken': 'token1',
        'bound': 'END',
        'errors': [],
        'version': '1.0',
      },
    ];

    element.messages = messages;
    await microtasksFinished();

    const expandButton =
        element.shadowRoot.querySelector<CrButtonElement>('#expand-all');
    assertTrue(!!expandButton);

    // Items should begin collapsed.
    assertFalse(
        Array.of(...element.shadowRoot.querySelectorAll('event-list-item'))
            .every(item => item.expanded));
    assertEquals(
        loadTimeData.getString('expandAll'), expandButton.textContent.trim());

    expandButton.click();
    await microtasksFinished();
    assertTrue(
        Array.of(...element.shadowRoot.querySelectorAll('event-list-item'))
            .every(item => item.expanded));
    assertEquals(
        loadTimeData.getString('collapseAll'), expandButton.textContent.trim());

    expandButton.click();
    await microtasksFinished();
    assertFalse(
        Array.of(...element.shadowRoot.querySelectorAll('event-list-item'))
            .every(item => item.expanded));
    assertEquals(
        loadTimeData.getString('expandAll'), expandButton.textContent.trim());
  });
});
