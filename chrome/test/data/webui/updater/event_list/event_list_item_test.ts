// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://updater/event_list/event_list_item.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {MergedHistoryEvent, MergedInstallEvent, MergedUpdaterProcessEvent, PersistedDataEvent} from 'chrome://updater/event_history.js';
import {localizeEventType, UpdaterProcessMap} from 'chrome://updater/event_history.js';
import type {EventListItemElement} from 'chrome://updater/event_list/event_list_item.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('EventListItemElement', () => {
  let item: EventListItemElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    item = document.createElement('event-list-item');
    document.body.appendChild(item);

    return microtasksFinished();
  });

  test('renders nothing if event is undefined', () => {
    assertEquals(0, item.shadowRoot.children.length);
  });

  test('renders merged install event', async () => {
    const event: MergedHistoryEvent = {
      eventType: 'INSTALL',
      startEvent: {
        eventType: 'INSTALL',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
        appId: '{app1}',
      },
      endEvent: {
        eventType: 'INSTALL',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
        version: '1.0',
      },
    };
    item.event = event;
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent, localizeEventType('INSTALL'));
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getStringF('installSummary', '1.0'));
  });

  test('renders merged uninstall event', async () => {
    const event: MergedHistoryEvent = {
      eventType: 'UNINSTALL',
      startEvent: {
        eventType: 'UNINSTALL',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
        appId: '{app1}',
        version: '1.0',
        reason: 'SOME_REASON',
      },
      endEvent: {
        eventType: 'UNINSTALL',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
      },
    };
    item.event = event;
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent, localizeEventType('UNINSTALL'));
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getStringF('uninstallSummary', '1.0', 'SOME_REASON'));
  });

  test('renders merged qualify event', async () => {
    const event: MergedHistoryEvent = {
      eventType: 'QUALIFY',
      startEvent: {
        eventType: 'QUALIFY',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
      },
      endEvent: {
        eventType: 'QUALIFY',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
        qualified: true,
      },
    };
    item.event = event;
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent, localizeEventType('QUALIFY'));
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getString('qualificationSucceeded'));

    event.endEvent.qualified = false;
    item.event = {...event};
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getString('qualificationFailed'));
  });

  test('renders merged activate event', async () => {
    const event: MergedHistoryEvent = {
      eventType: 'ACTIVATE',
      startEvent: {
        eventType: 'ACTIVATE',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
      },
      endEvent: {
        eventType: 'ACTIVATE',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
        activated: true,
      },
    };
    item.event = event;
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent, localizeEventType('ACTIVATE'));
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getString('activationSucceeded'));

    event.endEvent.activated = false;
    item.event = {...event};
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getString('activationFailed'));
  });

  test('renders merged updater process event', async () => {
    const event: MergedHistoryEvent = {
      eventType: 'UPDATER_PROCESS',
      startEvent: {
        eventType: 'UPDATER_PROCESS',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
        scope: 'USER',
        commandLine: 'updater.exe --system',
      },
      endEvent: {
        eventType: 'UPDATER_PROCESS',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
        exitCode: 0,
      },
    };
    item.event = event;
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent, localizeEventType('UPDATER_PROCESS'));
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getStringF('processSummary', 0));
    assertStringContains(item.shadowRoot.textContent, 'updater.exe --system');
  });

  test('renders merged app command event', async () => {
    const event: MergedHistoryEvent = {
      eventType: 'APP_COMMAND',
      startEvent: {
        eventType: 'APP_COMMAND',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
        appId: '{app1}',
        commandLine: 'chrome.exe --do-stuff',
      },
      endEvent: {
        eventType: 'APP_COMMAND',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
        exitCode: 0,
      },
    };
    item.event = event;
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent, localizeEventType('APP_COMMAND'));
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getStringF('commandOutcome', 0));
    assertStringContains(item.shadowRoot.textContent, 'chrome.exe --do-stuff');
  });

  test('renders merged update event', async () => {
    const event: MergedHistoryEvent = {
      eventType: 'UPDATE',
      startEvent: {
        eventType: 'UPDATE',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
        appId: '{app1}',
      },
      endEvent: {
        eventType: 'UPDATE',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
        outcome: 'UPDATED',
        nextVersion: '2.0',
      },
    };
    item.event = event;
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent, localizeEventType('UPDATE'));
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getStringF('updatedTo', '2.0'));

    event.endEvent.outcome = 'NO_UPDATE';
    item.event = {...event};
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent, loadTimeData.getString('noUpdate'));

    event.endEvent.outcome = 'UPDATE_ERROR';
    item.event = {...event};
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent, loadTimeData.getString('updateError'));

    event.endEvent.outcome = 'UNKNOWN_OUTCOME';
    item.event = {...event};
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getStringF('outcome', 'UNKNOWN_OUTCOME'));
  });

  test('renders persisted data event', async () => {
    const event: PersistedDataEvent = {
      eventType: 'PERSISTED_DATA',
      bound: 'INSTANT',
      eventId: '1',
      deviceUptime: 0,
      pid: 0,
      processToken: '',
      eulaRequired: false,
      registeredApps: [],
      errors: [],
    };
    item.event = event;
    await microtasksFinished();
    assertStringContains(
        item.shadowRoot.textContent, localizeEventType('PERSISTED_DATA'));
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getStringF('persistedDataSummary', 0));
  });

  test('renders event with Omaha request and response', async () => {
    const event: MergedHistoryEvent = {
      eventType: 'POST_REQUEST',
      startEvent: {
        eventType: 'POST_REQUEST',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
        request: btoa('{"request":{"@os":"win"}}'),
      },
      endEvent: {
        eventType: 'POST_REQUEST',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
        response: btoa('{"response":{"protocol":"3.0"}}'),
      },
    };
    item.event = event;
    await microtasksFinished();

    item.shadowRoot.querySelector('cr-expand-button')!.click();
    await microtasksFinished();

    assertStringContains(
        item.shadowRoot.textContent, localizeEventType('POST_REQUEST'));
    assertStringContains(
        item.shadowRoot.textContent, loadTimeData.getString('omahaRequest'));

    const omahaRequestDetails =
        item.shadowRoot.querySelectorAll('raw-event-details')[0];
    assertNotEquals(undefined, omahaRequestDetails);
    assertEquals(
        loadTimeData.getString('omahaRequest'), omahaRequestDetails!.label);
    assertDeepEquals(
        {'request': {'@os': 'win'}}, omahaRequestDetails!.events[0]);

    const omahaResponseDetails =
        item.shadowRoot.querySelectorAll('raw-event-details')[1];
    assertNotEquals(undefined, omahaResponseDetails);
    assertEquals(
        loadTimeData.getString('omahaResponse'), omahaResponseDetails!.label);
    assertDeepEquals(
        {'response': {'protocol': '3.0'}}, omahaResponseDetails!.events[0]);
  });

  test('renders event with prefixed Omaha request and response', async () => {
    const event: MergedHistoryEvent = {
      eventType: 'POST_REQUEST',
      startEvent: {
        eventType: 'POST_REQUEST',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
        request: btoa(')]}\'{"request":{"@os":"win"}}'),
      },
      endEvent: {
        eventType: 'POST_REQUEST',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
        response: btoa(')]}\'{"response":{"protocol":"3.0"}}'),
      },
    };
    item.event = event;
    await microtasksFinished();

    item.shadowRoot.querySelector('cr-expand-button')!.click();
    await microtasksFinished();

    assertStringContains(
        item.shadowRoot.textContent, localizeEventType('POST_REQUEST'));
    assertStringContains(
        item.shadowRoot.textContent, loadTimeData.getString('omahaRequest'));

    const omahaRequestDetails =
        item.shadowRoot.querySelectorAll('raw-event-details')[0];
    assertNotEquals(undefined, omahaRequestDetails);
    assertEquals(
        loadTimeData.getString('omahaRequest'), omahaRequestDetails!.label);
    assertDeepEquals(
        {'request': {'@os': 'win'}}, omahaRequestDetails!.events[0]);

    const omahaResponseDetails =
        item.shadowRoot.querySelectorAll('raw-event-details')[1];
    assertNotEquals(undefined, omahaResponseDetails);
    assertEquals(
        loadTimeData.getString('omahaResponse'), omahaResponseDetails!.label);
    assertDeepEquals(
        {'response': {'protocol': '3.0'}}, omahaResponseDetails!.events[0]);
  });

  test('displays errors', async () => {
    const event: PersistedDataEvent = {
      eventType: 'PERSISTED_DATA',
      bound: 'INSTANT',
      eventId: '1',
      deviceUptime: 0,
      pid: 0,
      processToken: '',
      errors: [{category: 1, code: 2, extracode1: 3}],
      eulaRequired: false,
      registeredApps: [],
    };
    item.event = event;
    await microtasksFinished();
    assertTrue(item.error);
    assertNotEquals(
        null, item.shadowRoot.querySelector('.event-error-details'));

    const mergedEvent: MergedInstallEvent = {
      eventType: 'INSTALL',
      startEvent: {
        eventType: 'INSTALL',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [{category: 4, code: 5, extracode1: 6}],
        appId: '{app1}',
      },
      endEvent: {
        eventType: 'INSTALL',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [{category: 7, code: 8, extracode1: 9}],
        version: '1.0',
      },
    };
    item.event = mergedEvent;
    await microtasksFinished();
    assertTrue(item.error);
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getStringF('errorDetails', 4, 5, 6));
    assertStringContains(
        item.shadowRoot.textContent,
        loadTimeData.getStringF('errorDetails', 7, 8, 9));
  });

  test('toggles details', async () => {
    const event: MergedHistoryEvent = {
      eventType: 'INSTALL',
      startEvent: {
        eventType: 'INSTALL',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
        appId: '{app1}',
      },
      endEvent: {
        eventType: 'INSTALL',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
        version: '1.0',
      },
    };
    item.event = event;
    await microtasksFinished();

    const button = item.shadowRoot.querySelector('cr-expand-button')!;

    button.click();
    await microtasksFinished();
    assertTrue(item.expanded);

    button.click();
    await microtasksFinished();
    assertFalse(item.expanded);
  });

  test('displays updater version', async () => {
    const updaterProcessEvent: MergedUpdaterProcessEvent = {
      eventType: 'UPDATER_PROCESS',
      startEvent: {
        eventType: 'UPDATER_PROCESS',
        eventId: '1',
        deviceUptime: 0,
        pid: 1234,
        processToken: 'token',
        bound: 'START',
        errors: [],
        updaterVersion: '1.2.3.4',
      },
      endEvent: {
        eventType: 'UPDATER_PROCESS',
        eventId: '1',
        deviceUptime: 1000,
        pid: 1234,
        processToken: 'token',
        bound: 'END',
        errors: [],
      },
    };
    const processMap = new UpdaterProcessMap([updaterProcessEvent]);

    const event: MergedHistoryEvent = {
      eventType: 'INSTALL',
      startEvent: {
        eventType: 'INSTALL',
        eventId: '2',
        deviceUptime: 500,
        pid: 1234,
        processToken: 'token',
        bound: 'START',
        errors: [],
        appId: '{app1}',
      },
      endEvent: {
        eventType: 'INSTALL',
        eventId: '2',
        deviceUptime: 600,
        pid: 1234,
        processToken: 'token',
        bound: 'END',
        errors: [],
        version: '1.0',
      },
    };

    item.event = event;
    item.processMap = processMap;
    await microtasksFinished();
    assertStringContains(item.shadowRoot.textContent, '1.2.3.4');
  });

  test('displays event date', async () => {
    const date = new Date('2024-01-01T12:00:00');
    item.eventDate = date;

    const event: PersistedDataEvent = {
      eventType: 'PERSISTED_DATA',
      bound: 'INSTANT',
      eventId: '1',
      deviceUptime: 0,
      pid: 0,
      processToken: '',
      errors: [{category: 1, code: 2, extracode1: 3}],
      eulaRequired: false,
      registeredApps: [],
    };
    item.event = event;

    await microtasksFinished();
    assertStringContains(item.shadowRoot.textContent, '1/1, 12:00:00 PM');
  });

  test('displays event duration', async () => {
    const event: MergedHistoryEvent = {
      eventType: 'INSTALL',
      startEvent: {
        eventType: 'INSTALL',
        eventId: '1',
        deviceUptime: 1000000,  // 1 second
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
        appId: '{app1}',
      },
      endEvent: {
        eventType: 'INSTALL',
        eventId: '1',
        deviceUptime: 3661001002,  // 1h 1m 1ms 2μs later
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
        version: '1.0',
      },
    };
    item.event = event;
    await microtasksFinished();
    assertStringContains(item.shadowRoot.textContent, '1h 1m 1ms 2μs');
  });

  test('displays app name for known app', async () => {
    loadTimeData.overrideValues({
      numKnownApps: 1,
      knownAppName0: 'Chrome',
      knownAppIds0: '{app1}',
    });
    const event: MergedHistoryEvent = {
      eventType: 'INSTALL',
      startEvent: {
        eventType: 'INSTALL',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
        appId: '{APP1}',
      },
      endEvent: {
        eventType: 'INSTALL',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
        version: '1.0',
      },
    };
    item.event = event;
    await microtasksFinished();
    const appSpan = item.shadowRoot.querySelector('.event-app');
    assertTrue(!!appSpan);
    assertEquals('Chrome', appSpan.textContent.trim());
  });

  test('displays app id for unknown app', async () => {
    loadTimeData.overrideValues({
      numKnownApps: 0,
    });
    const event: MergedHistoryEvent = {
      eventType: 'INSTALL',
      startEvent: {
        eventType: 'INSTALL',
        eventId: '1',
        deviceUptime: 0,
        pid: 0,
        processToken: '',
        bound: 'START',
        errors: [],
        appId: '{unknown-app}',
      },
      endEvent: {
        eventType: 'INSTALL',
        eventId: '1',
        deviceUptime: 1000,
        pid: 0,
        processToken: '',
        bound: 'END',
        errors: [],
        version: '1.0',
      },
    };
    item.event = event;
    await microtasksFinished();
    const appSpan = item.shadowRoot.querySelector('.event-app');
    assertTrue(!!appSpan);
    assertEquals('{UNKNOWN-APP}', appSpan.textContent.trim());
  });
});
