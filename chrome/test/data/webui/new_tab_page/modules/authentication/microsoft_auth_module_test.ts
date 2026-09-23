// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DisableModuleEvent, MicrosoftAuthModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {microsoftAuthModuleDescriptor, ParentTrustedDocumentProxy} from 'chrome://new-tab-page/lazy_load.js';
import {AuthType, MicrosoftAuthUntrustedDocumentRemote} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

suite('MicrosoftAuthModule', () => {
  let childDocument: TestMock<MicrosoftAuthUntrustedDocumentRemote>;
  let metrics: MetricsTracker;
  let microsoftAuthModule: MicrosoftAuthModuleElement;
  const modulesMicrosoftAuthName = 'Microsoft Authentication';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      modulesMicrosoftAuthName: modulesMicrosoftAuthName,
    });

    childDocument = installMock(
        MicrosoftAuthUntrustedDocumentRemote,
        mock => ParentTrustedDocumentProxy.setInstance(mock));

    metrics = fakeMetricsPrivate();
  });

  async function createMicrosoftAuthElement() {
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
});
