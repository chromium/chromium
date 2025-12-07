// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxFileCarouselElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createComposeboxFile} from './test_support.js';

suite('NewTabPageComposeboxFileCarouselTest', () => {
  let fileCarouselElement: ComposeboxFileCarouselElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fileCarouselElement = new ComposeboxFileCarouselElement();
    document.body.appendChild(fileCarouselElement);
  });

  test('display files', async () => {
    // Assert no files.
    assertEquals(
        fileCarouselElement.shadowRoot
            .querySelectorAll('cr-composebox-file-thumbnail')
            .length,
        0);

    // Act.
    fileCarouselElement.files = [
      createComposeboxFile(0),
      createComposeboxFile(1, {type: 'image/jpeg', objectUrl: 'data:foo'}),
    ];
    await microtasksFinished();

    // Assert.
    const files = fileCarouselElement.shadowRoot.querySelectorAll(
        'cr-composebox-file-thumbnail');
    assertEquals(files.length, 2);
  });
});
