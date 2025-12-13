// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DisableModuleEvent, DismissModuleInstanceEvent, OutlookCalendarModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {outlookCalendarDescriptor, OutlookCalendarProxyImpl, ParentTrustedDocumentProxy} from 'chrome://new-tab-page/lazy_load.js';
import {MicrosoftAuthUntrustedDocumentRemote} from 'chrome://new-tab-page/ntp_microsoft_auth_shared_ui.mojom-webui.js';
import {OutlookCalendarPageHandlerRemote} from 'chrome://new-tab-page/outlook_calendar.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

import {createEvents} from './test_support.js';

suite('NewTabPageModulesOutlookCalendarModuleTest', () => {
  const title = 'Outlook Calendar';
  let childDocument: TestMock<MicrosoftAuthUntrustedDocumentRemote>;
  let handler: TestMock<OutlookCalendarPageHandlerRemote>;
  let module: OutlookCalendarModuleElement;

  setup(() => {
    loadTimeData.overrideValues({
      modulesOutlookCalendarTitle: title,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        OutlookCalendarPageHandlerRemote,
        mock => OutlookCalendarProxyImpl.setInstance(
            new OutlookCalendarProxyImpl(mock)));
    childDocument = installMock(
        MicrosoftAuthUntrustedDocumentRemote,
        mock => ParentTrustedDocumentProxy.setInstance(mock));
  });

  test(`creates module`, async () => {
    handler.setResultFor(
        'getEvents', Promise.resolve({events: createEvents(1)}));
    module = await outlookCalendarDescriptor.initialize(0) as
        OutlookCalendarModuleElement;
    assertTrue(!!module);
    document.body.append(module);

    // Assert.
    assertTrue(isVisible(module.$.moduleHeaderElementV2));
    assertEquals(module.$.moduleHeaderElementV2.headerText, title);
  });

  test(`module not created when there are no events`, async () => {
    handler.setResultFor(
        'getEvents', Promise.resolve({events: createEvents(0)}));
    module = await outlookCalendarDescriptor.initialize(0) as
        OutlookCalendarModuleElement;
    assertEquals(module, null);
  });

  test('clicking the disable button fires a disable module event', async () => {
    // Arrange.
    handler.setResultFor(
        'getEvents', Promise.resolve({events: createEvents(1)}));
    module = await outlookCalendarDescriptor.initialize(0) as
        OutlookCalendarModuleElement;
    assertTrue(!!module);
    document.body.append(module);

    // Act.
    const whenFired = eventToPromise('disable-module', module);
    const disableButton =
        module.$.moduleHeaderElementV2.shadowRoot.querySelector<HTMLElement>(
            '#disable');
    assertTrue(!!disableButton);
    disableButton.click();

    // Assert.
    const event: DisableModuleEvent = await whenFired;
    assertEquals(
        ('You won\'t see Outlook Calendar on this page again'),
        event.detail.message);
  });

  test(`dismiss and restore module`, async () => {
    // Set up module.
    handler.setResultFor(
        'getEvents', Promise.resolve({events: createEvents(1)}));
    module = await outlookCalendarDescriptor.initialize(0) as
        OutlookCalendarModuleElement;
    assertTrue(!!module);
    document.body.append(module);

    // Dismiss module.
    const whenFired = eventToPromise('dismiss-module-instance', module);
    const dismissButton =
        module.$.moduleHeaderElementV2.shadowRoot.querySelector<HTMLElement>(
            '#dismiss');
    assertTrue(!!dismissButton);
    dismissButton.click();

    const event: DismissModuleInstanceEvent = await whenFired;
    assertEquals('Outlook Calendar hidden', event.detail.message);
    assertTrue(!!event.detail.restoreCallback);
    assertEquals(1, handler.getCallCount('dismissModule'));

    // Restore module.
    event.detail.restoreCallback();
    assertEquals(1, handler.getCallCount('restoreModule'));
  });

  test('clicking the sign out button sends sign out request', async () => {
    // Arrange.
    handler.setResultFor(
        'getEvents', Promise.resolve({events: createEvents(1)}));
    module = await outlookCalendarDescriptor.initialize(0) as
        OutlookCalendarModuleElement;
    assertTrue(!!module);
    document.body.append(module);

    // Act.
    const signoutButton =
        module.$.moduleHeaderElementV2.shadowRoot.querySelector<HTMLElement>(
            '#signout');
    assertTrue(!!signoutButton);
    signoutButton.click();

    // Assert.
    assertEquals(1, childDocument.getCallCount('signOut'));
  });
});
