// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DisableModuleEvent, MicrosoftAuthModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {microsoftAuthModuleDescriptor, MicrosoftAuthProxyImpl, ParentTrustedDocumentProxy} from 'chrome://new-tab-page/lazy_load.js';
import {AuthType, MicrosoftAuthPageHandlerRemote, MicrosoftAuthUntrustedDocumentRemote} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

suite('MicrosoftAuthModule', () => {
  let handler: TestMock<MicrosoftAuthPageHandlerRemote>;
  let childDocument: TestMock<MicrosoftAuthUntrustedDocumentRemote>;
  let metrics: MetricsTracker;
  let microsoftAuthModule: MicrosoftAuthModuleElement;
  const modulesMicrosoftAuthName = 'Microsoft Authentication';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      modulesMicrosoftAuthName: modulesMicrosoftAuthName,
    });

    handler = installMock(
        MicrosoftAuthPageHandlerRemote,
        mock => MicrosoftAuthProxyImpl.setInstance(
            new MicrosoftAuthProxyImpl(mock)));

    childDocument = installMock(
        MicrosoftAuthUntrustedDocumentRemote,
        mock => ParentTrustedDocumentProxy.setInstance(mock));

    metrics = fakeMetricsPrivate();
  });

  async function createMicrosoftAuthElement() {
    handler.setPromiseResolveFor('shouldShowModule', {show: true});
    microsoftAuthModule = await microsoftAuthModuleDescriptor.initialize(0) as
        MicrosoftAuthModuleElement;
    assertTrue(!!microsoftAuthModule);
    document.body.append(microsoftAuthModule);
    await microtasksFinished();
  }

  test('clicking the disable button fires a disable module event', async () => {
    // Arrange.
    await createMicrosoftAuthElement();

    // Act.
    const whenFired = eventToPromise('disable-module', microsoftAuthModule);
    const disableButton = microsoftAuthModule.$.moduleHeader.shadowRoot
                              .querySelector<HTMLElement>('#disable');
    assertTrue(!!disableButton);
    disableButton.click();

    // Assert.
    const event: DisableModuleEvent = await whenFired;
    assertEquals(
        ('You won\'t see ' + modulesMicrosoftAuthName + ' on this page again'),
        event.detail.message);
  });

  test('clicking sign in sends message to child document', async () => {
    // Arrange.
    await createMicrosoftAuthElement();

    // Act.
    microsoftAuthModule.$.signInButton.click();

    // Assert.
    assertEquals(1, childDocument.getCallCount('acquireTokenPopup'));
    assertEquals(
        1,
        metrics.count('NewTabPage.MicrosoftAuth.AuthStarted', AuthType.kPopup));
  });

  test('does not populate module if handler says not to', async () => {
    // Arrange/Act.
    handler.setPromiseResolveFor('shouldShowModule', {show: false});
    microsoftAuthModule = await microsoftAuthModuleDescriptor.initialize(0) as
        MicrosoftAuthModuleElement;

    // Assert.
    assertFalse(!!microsoftAuthModule);
  });
});
