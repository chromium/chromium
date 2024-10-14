// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SharepointModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {sharepointModuleDescriptor} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('SharepointModule', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('clicking the info button opens the ntp info dialog box', async () => {
    // Arrange.
    const sharepointModule = await sharepointModuleDescriptor.initialize(0) as
        SharepointModuleElement;
    document.body.append(sharepointModule);
    await microtasksFinished();
    assertFalse(!!$$(sharepointModule, 'ntp-info-dialog'));

    // Act.
    const infoButton = sharepointModule.$.moduleHeaderElementV2.shadowRoot!
                           .querySelector<HTMLElement>('#info');
    assertTrue(!!infoButton);
    infoButton.click();
    await microtasksFinished();

    // Assert.
    assertTrue(!!$$(sharepointModule, 'ntp-info-dialog'));
  });
});
