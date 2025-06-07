// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ComposeboxFile} from 'chrome://new-tab-page/lazy_load.js';
import {ComposeboxFileCarouselElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

function createComposeboxFile(
    index: number, override: Partial<ComposeboxFile> = {}): ComposeboxFile {
  return Object.assign(
      {
        name: `file${index}`,
        type: 'application/pdf',
        objectUrl: null,
        uuid: `${index}`,
      },
      override);
}

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
        fileCarouselElement.shadowRoot.querySelectorAll('.file').length, 0);

    // Act.
    fileCarouselElement.files = [
      createComposeboxFile(0),
      createComposeboxFile(1, {type: 'image/jpeg', objectUrl: 'data:foo'}),
    ];
    await microtasksFinished();

    // Assert one image file.
    const files = fileCarouselElement.shadowRoot.querySelectorAll('.file');
    assertEquals(files.length, 2);
    assertEquals(files[0]!.tagName, 'P');
    assertEquals(files[0]!.id, fileCarouselElement.files[0]!.uuid);
    assertEquals(files[0]!.textContent, fileCarouselElement.files[0]!.name);
    assertEquals(files[1]!.tagName, 'IMG');
    assertEquals(files[1]!.id, fileCarouselElement.files[1]!.uuid);
    assertEquals(
        (files[1]! as HTMLImageElement).src,
        fileCarouselElement.files[1]!.objectUrl);
  });
});
