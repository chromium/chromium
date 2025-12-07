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

  test('display image file from dataUrl', async () => {
    // Arrange.
    fileThumbnailElement.file =
        createComposeboxFile(1, {type: 'image/jpeg', dataUrl: 'data:foo'});
    await microtasksFinished();

    // Assert one image file.
    const thumbnail =
        fileThumbnailElement.shadowRoot.querySelector('.img-thumbnail');
    assertTrue(!!thumbnail);
    assertEquals(thumbnail.tagName, 'IMG');
    assertEquals(
        (thumbnail as HTMLImageElement).src, fileThumbnailElement.file.dataUrl);
  });

  test('display pdf file', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(0);
    await microtasksFinished();

    // Assert one image file.
    const thumbnail =
        fileThumbnailElement.shadowRoot.querySelector('#pdfTitle');
    assertTrue(!!thumbnail);
    assertEquals(thumbnail.tagName, 'P');
    assertEquals(thumbnail.textContent, fileThumbnailElement.file.name);
  });

  test('display tab file', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(2, {
      url: {url: 'https://example.com/some/path'},
      name: 'some tab',
    });
    await microtasksFinished();

    // Assert.
    const thumbnail = fileThumbnailElement.shadowRoot.querySelector('#tabChip');
    assertTrue(!!thumbnail);
    const title =
        fileThumbnailElement.shadowRoot.querySelector<HTMLElement>('.title');
    assertTrue(!!title);
    assertEquals(title.innerText, 'some tab');
    const favicon =
        fileThumbnailElement.shadowRoot.querySelector('cr-composebox-tab-favicon');
    assertTrue(!!favicon);
    assertEquals(favicon.url, 'https://example.com/some/path');
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
    assertTrue(!!fileThumbnailElement.$.removeImgButton);
    fileThumbnailElement.$.removeImgButton.click();

    // Assert.
    const deleteEvent = await deleteEventPromise;
    assertEquals(deleteEvent.detail.uuid, '1');
  });

  test('hides image delete button when not deletable', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(1, {
      type: 'image/jpeg',
      objectUrl: 'data:foo',
      isDeletable: false,
    });
    await microtasksFinished();

    // Assert.
    const removeButton =
        fileThumbnailElement.shadowRoot.querySelector('#removeImgButton');
    assertEquals(null, removeButton);
  });

  test('clicking pdf delete button sends event', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(0);
    await microtasksFinished();

    // Act.
    const deleteEventPromise =
        eventToPromise('delete-file', fileThumbnailElement) as
        Promise<CustomEvent>;
    assertTrue(!!fileThumbnailElement.$.removePdfButton);
    fileThumbnailElement.$.removePdfButton.click();

    // Assert.
    const deleteEvent = await deleteEventPromise;
    assertEquals(deleteEvent.detail.uuid, '0');
  });

  test('hides pdf delete button when not deletable', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(0, {isDeletable: false});
    await microtasksFinished();

    // Assert.
    const removeButton =
        fileThumbnailElement.shadowRoot.querySelector('#removePdfButton');
    assertEquals(null, removeButton);
  });

  test('clicking tab delete button sends event', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(2, {
      url: {url: 'https://example.com/some/path'},
      name: 'some tab',
    });
    await microtasksFinished();

    // Act.
    const deleteEventPromise =
        eventToPromise('delete-file', fileThumbnailElement) as
        Promise<CustomEvent>;
    assertTrue(!!fileThumbnailElement.$.removeTabButton);
    fileThumbnailElement.$.removeTabButton.click();

    // Assert.
    const deleteEvent = await deleteEventPromise;
    assertEquals(deleteEvent.detail.uuid, '2');
  });

  test('hides tab delete button when not deletable', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(2, {
      url: {url: 'https://example.com/some/path'},
      name: 'some tab',
      isDeletable: false,
    });
    await microtasksFinished();

    // Assert.
    const removeButton =
        fileThumbnailElement.shadowRoot.querySelector('#removeTabButton');
    assertEquals(null, removeButton);
  });
});
