// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

suite('NewTabPageDoodleShareDialogFocusTest', () => {
  /** @type {!DoodleShareDialogElement} */
  let doodleShareDialog;

  setup(() => {
    PolymerTest.clearBody();
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
