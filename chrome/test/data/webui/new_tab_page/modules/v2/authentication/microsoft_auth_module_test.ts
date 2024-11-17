// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DisableModuleEvent, MicrosoftAuthModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {microsoftAuthModuleDescriptor} from 'chrome://new-tab-page/lazy_load.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('MicrosoftAuthModule', () => {
  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('clicking the disable button fires a disable module event', async () => {
    // Arrange.
    const modulesMicrosoftAuthName = 'Microsoft Authentication';
    loadTimeData.overrideValues(
        {modulesMicrosoftAuthName: modulesMicrosoftAuthName});
    const microsoftAuthModule = await microsoftAuthModuleDescriptor.initialize(
                                    0) as MicrosoftAuthModuleElement;
    document.body.append(microsoftAuthModule);
    await microtasksFinished();

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
});
