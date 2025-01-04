// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DisableModuleEvent, DismissModuleInstanceEvent, MicrosoftAuthModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {microsoftAuthModuleDescriptor, MicrosoftAuthProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {MicrosoftAuthPageHandlerRemote} from 'chrome://new-tab-page/microsoft_auth.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../../test_support.js';

suite('MicrosoftAuthModule', () => {
  let handler: TestMock<MicrosoftAuthPageHandlerRemote>;
  let microsoftAuthModule: MicrosoftAuthModuleElement;
  const modulesMicrosoftAuthName = 'Microsoft Authentication';

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues(
        {modulesMicrosoftAuthName: modulesMicrosoftAuthName});
    handler = installMock(
        MicrosoftAuthPageHandlerRemote,
        mock => MicrosoftAuthProxyImpl.setInstance(
            new MicrosoftAuthProxyImpl(mock)));
    microsoftAuthModule = await microsoftAuthModuleDescriptor.initialize(0) as
        MicrosoftAuthModuleElement;
    assertTrue(!!microsoftAuthModule);
    document.body.append(microsoftAuthModule);
    await microtasksFinished();
  });

  test('clicking the disable button fires a disable module event', async () => {
    // Act.
    const whenFired = eventToPromise('disable-module', microsoftAuthModule);
    const disableButton =
        microsoftAuthModule.$.moduleHeaderElementV2.shadowRoot!
            .querySelector<HTMLElement>('#disable');
    assertTrue(!!disableButton);
    disableButton.click();

    // Assert.
    const event: DisableModuleEvent = await whenFired;
    assertEquals(
        ('You won\'t see ' + modulesMicrosoftAuthName + ' on this page again'),
        event.detail.message);
  });

  test('dismisses and restores module', async () => {
    // Act.
    const whenFired =
        eventToPromise('dismiss-module-instance', microsoftAuthModule);
    microsoftAuthModule.$.moduleHeaderElementV2.dispatchEvent(
        new Event('dismiss-button-click'));

    // Assert.
    const event: DismissModuleInstanceEvent = await whenFired;
    assertEquals((modulesMicrosoftAuthName + ' hidden'), event.detail.message);
    assertTrue(!!event.detail.restoreCallback);
    assertEquals(1, handler.getCallCount('dismissModule'));

    // Act.
    event.detail.restoreCallback!();

    // Assert.
    assertEquals(1, handler.getCallCount('restoreModule'));
  });
});
