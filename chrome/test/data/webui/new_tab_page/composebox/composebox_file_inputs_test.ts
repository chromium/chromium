// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxFileInputsElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('NewTabPageComposeboxFileInputsTest', () => {
  let fileUploadSlot: HTMLElement;
  let fileInputsElement: ComposeboxFileInputsElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fileInputsElement = new ComposeboxFileInputsElement();
    fileUploadSlot = document.createElement('button');
    fileInputsElement.appendChild(fileUploadSlot);
    document.body.appendChild(fileInputsElement);
  });

  test('open image event clicks image input', async () => {
    const imageUploadClickEventPromise =
        eventToPromise('click', fileInputsElement.$.imageInput);
    fileUploadSlot.dispatchEvent(
        new CustomEvent('open-image-upload', {bubbles: true, composed: true}));
    await imageUploadClickEventPromise;
  });

  test('open file event clicks file input', async () => {
    const imageFileClickEventPromise =
        eventToPromise('click', fileInputsElement.$.fileInput);
    fileUploadSlot.dispatchEvent(
        new CustomEvent('open-file-upload', {bubbles: true, composed: true}));
    await imageFileClickEventPromise;
  });

  test('file change event is fired when file input changes', async () => {
    // Arrange.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(
        new File(['foo'], 'foo.pdf', {type: 'application/pdf'}));
    dataTransfer.items.add(
        new File(['foo2'], 'foo2.pdf', {type: 'application/pdf'}));

    // Act.
    const whenFileChange = eventToPromise('on-file-change', fileInputsElement);
    const mockFileChange = new Event('change', {bubbles: true});
    Object.defineProperty(mockFileChange, 'target', {
      writable: false,
      value: fileInputsElement.$.fileInput,
    });
    fileInputsElement.$.fileInput.files = dataTransfer.files;
    fileInputsElement.$.fileInput.dispatchEvent(mockFileChange);

    // Assert
    const event = await whenFileChange;
    assertTrue(!!event);
    assertEquals(event.detail.files, dataTransfer.files);
    assertEquals(fileInputsElement.$.fileInput.value, '');
  });
});
