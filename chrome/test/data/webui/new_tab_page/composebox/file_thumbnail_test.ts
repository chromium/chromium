// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxFileThumbnailElement} from 'chrome://new-tab-page/lazy_load.js';
import {FileUploadStatus} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createComposeboxFile} from './test_support.js';

suite('NewTabPageComposeboxFileThumbnailTest', () => {
  let fileThumbnailElement: ComposeboxFileThumbnailElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fileThumbnailElement = new ComposeboxFileThumbnailElement();
    document.body.appendChild(fileThumbnailElement);
  });

  test('display loading spinner', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(1, {
      type: 'image/jpeg',
      objectUrl: 'data:foo',
      status: FileUploadStatus.kUploadStarted,
    });
    await microtasksFinished();

    // Assert.
    const spinner = fileThumbnailElement.shadowRoot.querySelector('.spinner');
    assertTrue(!!spinner);
  });

  test('display image file', async () => {
    // Arrange.
    fileThumbnailElement.file =
        createComposeboxFile(1, {type: 'image/jpeg', objectUrl: 'data:foo'});
    await microtasksFinished();

    // Assert one image file.
    const thumbnail =
        fileThumbnailElement.shadowRoot.querySelector('.img-thumbnail');
    assertTrue(!!thumbnail);
    assertEquals(thumbnail.tagName, 'IMG');
    assertEquals(
        (thumbnail as HTMLImageElement).src,
        fileThumbnailElement.file.objectUrl);
  });

  test('display pdf file', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(0);
    await microtasksFinished();

    // Assert one image file.
    const thumbnail =
        fileThumbnailElement.shadowRoot.querySelector('.pdf-title');
    assertTrue(!!thumbnail);
    assertEquals(thumbnail.tagName, 'P');
    assertEquals(thumbnail.textContent, fileThumbnailElement.file.name);
  });

  test('clicking image delete button sends event', async () => {
    // Arrange.
    fileThumbnailElement.file =
        createComposeboxFile(1, {type: 'image/jpeg', objectUrl: 'data:foo'});
    await microtasksFinished();

    // Act.
    const deleteEventPromise =
        eventToPromise('delete-file', fileThumbnailElement) as
        Promise<CustomEvent>;
    fileThumbnailElement.$.removeImgButton.click();

    // Assert.
    const deleteEvent = await deleteEventPromise;
    assertEquals(deleteEvent.detail.uuid, '1');
  });

  test('clicking pdf delete button sends event', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(0);
    await microtasksFinished();

    // Act.
    const deleteEventPromise =
        eventToPromise('delete-file', fileThumbnailElement) as
        Promise<CustomEvent>;
    fileThumbnailElement.$.removePdfButton.click();

    // Assert.
    const deleteEvent = await deleteEventPromise;
    assertEquals(deleteEvent.detail.uuid, '0');
  });
});
