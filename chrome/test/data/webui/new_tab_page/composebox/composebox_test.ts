// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';


suite('NewTabPageComposeboxTest', () => {
  let composeboxElement: ComposeboxElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    composeboxElement = new ComposeboxElement();
    document.body.appendChild(composeboxElement);
  });

  test('upload image', async () => {
    // Assert no files.
    assertEquals(composeboxElement.$.carousel.files.length, 0);

    // Act.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    composeboxElement.$.imageUploader.files = dataTransfer.files;
    composeboxElement.$.imageUploader.dispatchEvent(new Event('change'));
    await microtasksFinished();

    // Assert one image file.
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 1);
    assertEquals(files[0]!.type, 'image/jpeg');
    assertEquals(files[0]!.name, 'foo.jpg');
    assertTrue(!!files[0]!.objectUrl);
  });

  test('upload pdf', async () => {
    // Assert no files.
    assertEquals(composeboxElement.$.carousel.files.length, 0);

    // Arrange.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(
        new File(['foo'], 'foo.pdf', {type: 'application/pdf'}));
    composeboxElement.$.attachmentUploader.files = dataTransfer.files;
    composeboxElement.$.attachmentUploader.dispatchEvent(new Event('change'));
    await microtasksFinished();

    // Assert one pdf file.
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 1);
    assertEquals(files[0]!.type, 'application/pdf');
    assertEquals(files[0]!.name, 'foo.pdf');
    assertFalse(!!files[0]!.objectUrl);
  });

  test('delete file', async () => {
    // Arrange.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(
        new File(['foo1'], 'foo1.pdf', {type: 'application/pdf'}));
    dataTransfer.items.add(
        new File(['foo2'], 'foo2.pdf', {type: 'application/pdf'}));
    composeboxElement.$.attachmentUploader.files = dataTransfer.files;
    composeboxElement.$.attachmentUploader.dispatchEvent(new Event('change'));
    await microtasksFinished();

    // Assert two files.
    assertEquals(composeboxElement.$.carousel.files.length, 2);

    // Act.
    composeboxElement.$.carousel.dispatchEvent(new CustomEvent('delete-file', {
      detail: {
        uuid: composeboxElement.$.carousel.files[0]!.uuid,
      },
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    // Assert.
    assertEquals(composeboxElement.$.carousel.files.length, 1);
  });
});
