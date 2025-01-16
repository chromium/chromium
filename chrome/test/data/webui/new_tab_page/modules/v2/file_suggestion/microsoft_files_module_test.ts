// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DisableModuleEvent, MicrosoftFilesModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {microsoftFilesModuleDescriptor} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('MicrosoftFilesModule', () => {
  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('clicking the info button opens the ntp info dialog box', async () => {
    // Arrange.
    const microsoftFilesModule =
        await microsoftFilesModuleDescriptor.initialize(0) as
        MicrosoftFilesModuleElement;
    document.body.append(microsoftFilesModule);
    await microtasksFinished();
    assertFalse(!!$$(microsoftFilesModule, 'ntp-info-dialog'));

    // Act.
    const infoButton = microsoftFilesModule.$.moduleHeaderElementV2.shadowRoot!
                           .querySelector<HTMLElement>('#info');
    assertTrue(!!infoButton);
    infoButton.click();
    await microtasksFinished();

    // Assert.
    assertTrue(!!$$(microsoftFilesModule, 'ntp-info-dialog'));
  });

  test('clicking the disable button fires a disable module event', async () => {
    // Arrange.
    const modulesSharepointName = 'SharePoint';
    loadTimeData.overrideValues({modulesSharepointName: modulesSharepointName});
    const microsoftFilesModule =
        await microsoftFilesModuleDescriptor.initialize(0) as
        MicrosoftFilesModuleElement;
    document.body.append(microsoftFilesModule);
    await microtasksFinished();

    // Act.
    const whenFired = eventToPromise('disable-module', microsoftFilesModule);
    const disableButton =
        microsoftFilesModule.$.moduleHeaderElementV2.shadowRoot!
            .querySelector<HTMLElement>('#disable');
    assertTrue(!!disableButton);
    disableButton.click();

    // Assert.
    const event: DisableModuleEvent = await whenFired;
    assertEquals(
        ('You won\'t see ' + modulesSharepointName + ' on this page again'),
        event.detail.message);
  });
});
