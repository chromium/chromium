// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {UpdaterProcessMap} from 'chrome://updater/event_history.js';
import type {AppCommandStartEvent, InstallStartEvent, MergedUpdateEvent, MergedUpdaterProcessEvent, Scope, UninstallStartEvent, UpdateEndEvent, UpdateStartEvent} from 'chrome://updater/event_history.js';
import {applyFilterSettings, createDefaultFilterSettings, createEmptyFilterSettings} from 'chrome://updater/event_list/filter_settings.js';
import {assertArrayEquals, assertDeepEquals, assertEquals, assertNull} from 'chrome://webui-test/chai_assert.js';

function createInstallStartEvent(
    uptime: number, appId: string = 'test-app'): InstallStartEvent {
  return {
    eventType: 'INSTALL',
    bound: 'START',
    eventId: `event-${uptime}`,
    deviceUptime: uptime,
    pid: 100,
    processToken: 'token-100',
    errors: [],
    appId,
  };
}

function createUpdateStartEvent(
    uptime: number, appId?: string): UpdateStartEvent {
  return {
    eventType: 'UPDATE',
    bound: 'START',
    eventId: `event-${uptime}`,
    deviceUptime: uptime,
    pid: 100,
    processToken: 'token-100',
    errors: [],
    appId,
  };
}

function createUpdateEndEvent(
    uptime: number, outcome?: string): UpdateEndEvent {
  return {
    eventType: 'UPDATE',
    bound: 'END',
    eventId: `event-${uptime}`,
    deviceUptime: uptime,
    pid: 100,
    processToken: 'token-100',
    errors: [],
    outcome,
  };
}

function createUninstallStartEvent(
    uptime: number, appId: string = 'test-app'): UninstallStartEvent {
  return {
    eventType: 'UNINSTALL',
    bound: 'START',
    eventId: `event-${uptime}`,
    deviceUptime: uptime,
    pid: 100,
    processToken: 'token-100',
    errors: [],
    appId,
    version: '1.0',
    reason: 'reason',
  };
}

function createAppCommandStartEvent(
    uptime: number, appId: string = 'test-app'): AppCommandStartEvent {
  return {
    eventType: 'APP_COMMAND',
    bound: 'START',
    eventId: `event-${uptime}`,
    deviceUptime: uptime,
    pid: 100,
    processToken: 'token-100',
    errors: [],
    appId,
  };
}

suite('FilterSettings', () => {
  setup(() => {
    loadTimeData.overrideValues({
      defaultAppFilters: '',
    });
  });

  suite('createDefaultFilterSettings', () => {
    test('produces defaults', () => {
      const settings = createDefaultFilterSettings();
      assertEquals(0, settings.apps.size);
      assertDeepEquals(
          new Set(['INSTALL', 'UPDATE', 'UNINSTALL']), settings.eventTypes);
      assertDeepEquals(
          new Set(['UPDATED', 'UPDATE_ERROR']), settings.updateOutcomes);
      assertEquals(0, settings.scopes.size);
      assertNull(settings.startDate);
      assertNull(settings.endDate);
    });

    test('sets app filters from loadTimeData', () => {
      loadTimeData.overrideValues({
        defaultAppFilters: 'app1,app2',
      });
      const settings = createDefaultFilterSettings();
      assertDeepEquals(new Set(['app1', 'app2']), settings.apps);
    });
  });

  suite('createEmptyFilterSettings', () => {
    test('produces empty settings', () => {
      const settings = createEmptyFilterSettings();
      assertEquals(0, settings.apps.size);
      assertEquals(0, settings.eventTypes.size);
      assertEquals(0, settings.updateOutcomes.size);
      assertEquals(0, settings.scopes.size);
      assertNull(settings.startDate);
      assertNull(settings.endDate);
    });
  });

  suite('applyFilterSettings', () => {
    test('filters empty array', () => {
      const processMap = new UpdaterProcessMap([]);
      const settings = createEmptyFilterSettings();
      const result = applyFilterSettings(processMap, [], settings);
      assertArrayEquals([], result);
    });

    test('produces empty array without processMap', () => {
      const settings = createEmptyFilterSettings();
      const result = applyFilterSettings(undefined, [], settings);
      assertArrayEquals([], result);
    });

    function createUpdaterProcessEvent(
        startTimestamp: number, scope?: Scope): MergedUpdaterProcessEvent {
      return {
        eventType: 'UPDATER_PROCESS',
        startEvent: {
          eventType: 'UPDATER_PROCESS',
          eventId: `process-100`,
          deviceUptime: 0,
          pid: 100,
          processToken: `token-100`,
          bound: 'START',
          errors: [],
          timestamp: new Date(startTimestamp),
          scope,
        },
        endEvent: {
          eventType: 'UPDATER_PROCESS',
          eventId: `process-100`,
          deviceUptime: 1000,
          pid: 100,
          processToken: `token-100`,
          bound: 'END',
          errors: [],
        },
      };
    }

    test('filters by event type', () => {
      const processEvent = createUpdaterProcessEvent(100000);
      const processMap = new UpdaterProcessMap([processEvent]);

      const event1 = createInstallStartEvent(100);
      const event2 = createUpdateStartEvent(200);
      const event3 = createUninstallStartEvent(300);
      const event4 = createAppCommandStartEvent(400);

      const events = [event1, event2, event3, event4];
      const settings = createEmptyFilterSettings();

      // Default includes INSTALL, UPDATE, UNINSTALL
      settings.eventTypes = new Set(['INSTALL', 'APP_COMMAND']);
      const result = applyFilterSettings(processMap, events, settings);
      assertArrayEquals([event1, event4], result);
    });

    test('filters by update outcome', () => {
      const processEvent = createUpdaterProcessEvent(100000);
      const processMap = new UpdaterProcessMap([processEvent]);

      const update1: MergedUpdateEvent = {
        eventType: 'UPDATE',
        startEvent: createUpdateStartEvent(100),
        endEvent: createUpdateEndEvent(200, 'UPDATED'),
      };
      const update2: MergedUpdateEvent = {
        eventType: 'UPDATE',
        startEvent: createUpdateStartEvent(300),
        endEvent: createUpdateEndEvent(400, 'UPDATE_ERROR'),
      };
      const update3: MergedUpdateEvent = {
        eventType: 'UPDATE',
        startEvent: createUpdateStartEvent(500),
        endEvent: createUpdateEndEvent(600, 'NO_UPDATE'),
      };
      const events = [update1, update2, update3];

      const settings = createEmptyFilterSettings();
      settings.updateOutcomes = new Set(['UPDATED', 'NO_UPDATE']);
      const result = applyFilterSettings(processMap, events, settings);

      assertArrayEquals([update1, update3], result);
    });

    suite('filters by date', () => {
      const processEvent = createUpdaterProcessEvent(1000000);
      const processMap = new UpdaterProcessMap([processEvent]);

      const event1 = createInstallStartEvent(1000 * 1000);
      const event2 = createInstallStartEvent(2000 * 1000);
      const event3 = createInstallStartEvent(3000 * 1000);

      const events = [event1, event2, event3];

      test('between', () => {
        const settings = createEmptyFilterSettings();

        settings.startDate = new Date(1001500);
        settings.endDate = new Date(1002500);

        const result = applyFilterSettings(processMap, events, settings);

        assertArrayEquals([event2], result);
      });

      test('after', () => {
        const settings = createEmptyFilterSettings();
        settings.startDate = new Date(1001500);
        settings.endDate = null;

        const result = applyFilterSettings(processMap, events, settings);

        assertArrayEquals([event2, event3], result);
      });

      test('before', () => {
        const settings = createEmptyFilterSettings();
        settings.startDate = null;
        settings.endDate = new Date(1002500);

        const result = applyFilterSettings(processMap, events, settings);

        assertArrayEquals([event1, event2], result);
      });
    });

    suite('filters by app', () => {
      const processEvent = createUpdaterProcessEvent(100000);
      const processMap = new UpdaterProcessMap([processEvent]);
      test('id', () => {
        const event1 = createInstallStartEvent(100, 'app1');
        const event2 = createInstallStartEvent(200, 'app2');
        const event3 = createInstallStartEvent(300, 'app3');
        const event4 = createInstallStartEvent(400, 'app1');

        const events = [event1, event2, event3, event4];
        const settings = createEmptyFilterSettings();
        settings.apps = new Set(['app1', 'app3']);

        const result = applyFilterSettings(processMap, events, settings);

        assertArrayEquals([event1, event3, event4], result);
      });

      test('id insensitive of case', () => {
        const event1 = createInstallStartEvent(100, 'app1');
        const event2 = createInstallStartEvent(200, 'app2');
        const event3 = createInstallStartEvent(300, 'app3');
        const event4 = createInstallStartEvent(400, 'APP1');

        const events = [event1, event2, event3, event4];
        const settings = createEmptyFilterSettings();
        settings.apps = new Set(['APP1', 'APP3']);

        const result = applyFilterSettings(processMap, events, settings);

        assertArrayEquals([event1, event3, event4], result);
      });

      test('name', () => {
        loadTimeData.overrideValues({
          numKnownApps: 1,
          knownAppName0: 'Test App',
          knownAppIds0: 'app1,app2',
        });

        const event1 = createInstallStartEvent(100, 'app1');
        const event2 = createInstallStartEvent(200, 'app2');
        const event3 = createInstallStartEvent(300, 'app3');
        const event4 = createInstallStartEvent(400, 'APP1');

        const events = [event1, event2, event3, event4];
        const settings = createEmptyFilterSettings();
        settings.apps = new Set(['Test App']);

        const result = applyFilterSettings(processMap, events, settings);

        assertArrayEquals([event1, event2, event4], result);
      });
    });

    suite('filters by scope', () => {
      function createProcessEvent(
          pid: number, token: string, scope: Scope): MergedUpdaterProcessEvent {
        return {
          eventType: 'UPDATER_PROCESS',
          startEvent: {
            eventType: 'UPDATER_PROCESS',
            eventId: `process-${pid}`,
            deviceUptime: 0,
            pid,
            processToken: token,
            bound: 'START',
            errors: [],
            timestamp: new Date(100000),
            scope,
          },
          endEvent: {
            eventType: 'UPDATER_PROCESS',
            eventId: `process-${pid}`,
            deviceUptime: 1000,
            pid,
            processToken: token,
            bound: 'END',
            errors: [],
          },
        };
      }

      const processUser = createProcessEvent(100, 'token-100', 'USER');
      const processSystem = createProcessEvent(200, 'token-200', 'SYSTEM');

      const processMap = new UpdaterProcessMap([processUser, processSystem]);

      function createEvent(
          uptime: number, pid: number, token: string): InstallStartEvent {
        return {
          eventType: 'INSTALL',
          bound: 'START',
          eventId: `event-${uptime}`,
          deviceUptime: uptime,
          pid,
          processToken: token,
          errors: [],
          appId: 'app1',
        };
      }

      const eventUser = createEvent(100, 100, 'token-100');
      const eventSystem = createEvent(200, 200, 'token-200');

      const events = [eventUser, eventSystem];

      test('user scope', () => {
        const settings = createEmptyFilterSettings();
        settings.scopes = new Set(['USER']);
        const result = applyFilterSettings(processMap, events, settings);
        assertArrayEquals([eventUser], result);
      });

      test('system scope', () => {
        const settings = createEmptyFilterSettings();
        settings.scopes = new Set(['SYSTEM']);
        const result = applyFilterSettings(processMap, events, settings);
        assertArrayEquals([eventSystem], result);
      });

      test('both scopes', () => {
        const settings = createEmptyFilterSettings();
        settings.scopes = new Set(['USER', 'SYSTEM']);
        const result = applyFilterSettings(processMap, events, settings);
        assertArrayEquals([eventUser, eventSystem], result);
      });
    });
  });
});
