// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GoogleCalendarPageHandlerRemote} from 'chrome://new-tab-page/google_calendar.mojom-webui.js';
import type {DismissModuleEvent, GoogleCalendarModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {googleCalendarDescriptor, GoogleCalendarProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../../test_support.js';

import {createEvents} from './test_support.js';

suite('NewTabPageModulesGoogleCalendarModuleTest', () => {
  let handler: TestMock<GoogleCalendarPageHandlerRemote>;
  let module: GoogleCalendarModuleElement;

  const dismissTime = '6';
  const dismissToast = 'Google Calendar hidden';
  const title = 'Google Calendar';

  async function initializeModule(numEvents: number = 0) {
    handler.setResultFor(
        'getEvents', Promise.resolve({events: createEvents(numEvents)}));
    module = await googleCalendarDescriptor.initialize(0) as
        GoogleCalendarModuleElement;
    document.body.append(module);
  }

  setup(async () => {
    loadTimeData.overrideValues({
      modulesGoogleCalendarTitle: title,
      modulesGoogleCalendarDismissToastMessage: dismissToast,
      modulesDismissForHoursButtonText: 'Hide for $1 hours',
      calendarModuleDismissHours: dismissTime,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        GoogleCalendarPageHandlerRemote,
        mock => GoogleCalendarProxyImpl.setInstance(
            new GoogleCalendarProxyImpl(mock)));
  });

  test('creates module', async () => {
    await initializeModule(1);
    assertTrue(!!module);

    // Assert.
    assertTrue(isVisible(module.$.moduleHeaderElementV2));
    assertEquals(module.$.moduleHeaderElementV2.headerText, title);
  });

  test('does not creates module if no data', async () => {
    await initializeModule(0);

    // Assert.
    assertEquals(module, null);
  });

  test('dismisses and restores module', async () => {
    await initializeModule(1);
    assertTrue(!!module);

    // Act.
    const whenFired = eventToPromise('dismiss-module-instance', module);
    ($$(module, 'ntp-module-header-v2')!
     ).dispatchEvent(new Event('dismiss-button-click'));

    // Assert.
    const event: DismissModuleEvent = await whenFired;
    assertEquals(dismissToast, event.detail.message);
    assertTrue(!!event.detail.restoreCallback);
    assertEquals(1, handler.getCallCount('dismissModule'));

    // Act.
    event.detail.restoreCallback!();

    // Assert.
    assertEquals(1, handler.getCallCount('restoreModule'));
  });

  test('Calendar element created and passed event data', async () => {
    await initializeModule(2);
    assertTrue(!!module);

    assertTrue(isVisible(module.$.calendar));
    assertEquals(module.$.calendar.events.length, 2);
  });

  test('displays module info', async () => {
    await initializeModule(1);
    assertTrue(!!module);
    assertFalse(!!$$(module, 'ntp-info-dialog'));

    // Act.
    ($$(module, 'ntp-module-header-v2')!
     ).dispatchEvent(new Event('info-button-click'));
    await microtasksFinished();

    // Assert.
    assertTrue(!!$$(module, 'ntp-info-dialog'));
  });

  test('include time in dismiss text', async () => {
    await initializeModule(1);
    assertTrue(!!module);

    // Assert.
    const dismissButton = $$(module.$.moduleHeaderElementV2, '#dismiss');
    assertTrue(!!dismissButton);
    assertTrue(!!dismissButton!.textContent);
    assertEquals(
        dismissButton!.textContent!.trim(), `Hide for ${dismissTime} hours`);
  });
});
