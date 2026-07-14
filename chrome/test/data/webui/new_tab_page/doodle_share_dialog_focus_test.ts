// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {DoodleShareDialogElement} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('NewTabPageDoodleShareDialogFocusTest', () => {
  let doodleShareDialog: DoodleShareDialogElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    let clipboardText = '';
    Object.defineProperty(navigator, 'clipboard', {
      value: {
        writeText: (text: string) => {
          clipboardText = text;
          return Promise.resolve();
        },
        readText: () => Promise.resolve(clipboardText),
      },
      configurable: true,
    });
    doodleShareDialog = document.createElement('ntp-doodle-share-dialog');
    document.body.appendChild(doodleShareDialog);
  });

  test('clicking copy copies URL', async () => {
    // Arrange.
    doodleShareDialog.url = 'https://bar.com';

    // Act.
    doodleShareDialog.$.copyButton.click();

    // Assert.
    const text = await navigator.clipboard.readText();
    assertEquals(text, 'https://bar.com');
  });
});
