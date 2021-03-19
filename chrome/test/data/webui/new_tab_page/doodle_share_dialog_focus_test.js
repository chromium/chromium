// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {createTestProxy} from 'chrome://test/new_tab_page/test_support.js';

suite('NewTabPageDoodleShareDialogFocusTest', () => {
  /** @type {!DoodleShareDialogElement} */
  let doodleShareDialog;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    BrowserProxy.setInstance(testProxy);

    doodleShareDialog = document.createElement('ntp-doodle-share-dialog');
    document.body.appendChild(doodleShareDialog);
  });

  test('clicking copy copies URL', async () => {
    // Arrange.
    doodleShareDialog.url = {url: 'https://bar.com'};

    // Act.
    doodleShareDialog.$.copyButton.click();

    // Assert.
    const text = await navigator.clipboard.readText();
    assertEquals(text, 'https://bar.com');
  });
});
