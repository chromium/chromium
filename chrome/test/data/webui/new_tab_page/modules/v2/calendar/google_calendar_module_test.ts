// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GoogleCalendarPageHandlerRemote} from 'chrome://new-tab-page/google_calendar.mojom-webui.js';
import type {DismissModuleEvent, GoogleCalendarModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {googleCalendarDescriptor, GoogleCalendarProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../../test_support.js';

suite('NewTabPageModulesGoogleCalendarModuleTest', () => {
  let handler: TestMock<GoogleCalendarPageHandlerRemote>;
  let module: GoogleCalendarModuleElement;

  const title = `Today's Calendar`;
  const dismissToast = `Today's Calendar hidden`;

  setup(async () => {
    loadTimeData.overrideValues({
      modulesTodayCalendarHeader: title,
      modulesTodayCalendarDismissToastMessage: dismissToast,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        GoogleCalendarPageHandlerRemote,
        mock => GoogleCalendarProxyImpl.setInstance(
            new GoogleCalendarProxyImpl(mock)));
    module = await googleCalendarDescriptor.initialize(0) as
        GoogleCalendarModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await waitAfterNextRender(module);
  });

  test('creates module', async () => {
    // Assert.
    assertTrue(isVisible(module.$.moduleHeaderElementV2));
    assertEquals(module.$.moduleHeaderElementV2.headerText, title);
  });

  test('dismisses and restores module', async () => {
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
});
