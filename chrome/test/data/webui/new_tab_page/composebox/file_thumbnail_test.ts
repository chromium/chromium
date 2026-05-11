// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxFileThumbnailElement} from 'chrome://new-tab-page/lazy_load.js';
import {ContextUploadStatus} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
      status: ContextUploadStatus.kUploadStarted,
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

  test('display document file (flag disabled)', async () => {
    loadTimeData.overrideValues({lensSendRawFileMediaTypesEnabled: false});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fileThumbnailElement = new ComposeboxFileThumbnailElement();
    document.body.appendChild(fileThumbnailElement);

    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(0);
    await microtasksFinished();

    // Assert one document file.
    const title =
        fileThumbnailElement.shadowRoot.querySelector('#documentTitle');
    assertTrue(!!title);
    assertEquals(title.tagName, 'P');
    assertEquals(title.textContent, fileThumbnailElement.file.name);

    // Assert pdf icon is shown.
    const icon = fileThumbnailElement.shadowRoot.querySelector('.pdf-icon');
    assertTrue(!!icon);
    assertEquals((icon as any).icon, 'thumbnail:pdf');
  });

  test('display document file (flag enabled) for non-pdf', async () => {
    loadTimeData.overrideValues({lensSendRawFileMediaTypesEnabled: true});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fileThumbnailElement = new ComposeboxFileThumbnailElement();
    document.body.appendChild(fileThumbnailElement);

    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(0, {type: 'text/plain'});
    await microtasksFinished();

    // Assert one document file.
    const title =
        fileThumbnailElement.shadowRoot.querySelector('#documentTitle');
    assertTrue(!!title);
    assertEquals(title.tagName, 'P');
    assertEquals(title.textContent, fileThumbnailElement.file.name);

    // Assert document icon is shown.
    const icon =
        fileThumbnailElement.shadowRoot.querySelector('.document-icon');
    assertTrue(!!icon);
    assertEquals((icon as any).icon, 'thumbnail:document');
  });

  test('display pdf file (flag enabled)', async () => {
    loadTimeData.overrideValues({lensSendRawFileMediaTypesEnabled: true});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fileThumbnailElement = new ComposeboxFileThumbnailElement();
    document.body.appendChild(fileThumbnailElement);

    // Arrange.
    fileThumbnailElement.file =
        createComposeboxFile(0, {type: 'application/pdf'});
    await microtasksFinished();

    // Assert one document file.
    const title =
        fileThumbnailElement.shadowRoot.querySelector('#documentTitle');
    assertTrue(!!title);
    assertEquals(title.tagName, 'P');
    assertEquals(title.textContent, fileThumbnailElement.file.name);

    // Assert pdf icon is shown.
    const icon = fileThumbnailElement.shadowRoot.querySelector('.pdf-icon');
    assertTrue(!!icon);
    assertEquals((icon as any).icon, 'thumbnail:pdf');
  });

  test('display tab file', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(2, {
      url: 'https://example.com/some/path',
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
    const favicon = fileThumbnailElement.shadowRoot.querySelector(
        'cr-composebox-tab-favicon');
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

  test('clicking document delete button sends event', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(0);
    await microtasksFinished();

    // Act.
    const deleteEventPromise =
        eventToPromise('delete-file', fileThumbnailElement) as
        Promise<CustomEvent>;
    assertTrue(!!fileThumbnailElement.$.removeDocumentButton);
    fileThumbnailElement.$.removeDocumentButton.click();

    // Assert.
    const deleteEvent = await deleteEventPromise;
    assertEquals(deleteEvent.detail.uuid, '0');
  });

  test('hides document delete button when not deletable', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(0, {isDeletable: false});
    await microtasksFinished();

    // Assert.
    const removeButton =
        fileThumbnailElement.shadowRoot.querySelector('#removeDocumentButton');
    assertEquals(null, removeButton);
  });

  test('clicking tab delete button sends event', async () => {
    // Arrange.
    fileThumbnailElement.file = createComposeboxFile(2, {
      url: 'https://example.com/some/path',
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
      url: 'https://example.com/some/path',
      name: 'some tab',
      isDeletable: false,
    });
    await microtasksFinished();

    // Assert.
    const removeButton =
        fileThumbnailElement.shadowRoot.querySelector('#removeTabButton');
    assertEquals(null, removeButton);
  });

  test('shows animation for entering attachment', async () => {
    // Arrange.
    fileThumbnailElement = new ComposeboxFileThumbnailElement();
    document.body.appendChild(fileThumbnailElement);

    let resolveAnimation: (value: any) => void;
    fileThumbnailElement.getAnimations = () => {
      return [{
        finished: new Promise(resolve => {
          resolveAnimation = resolve;
        }),
      } as Animation];
    };

    fileThumbnailElement.file =
        createComposeboxFile(1, {type: 'image/jpeg', objectUrl: 'data:foo'});
    await microtasksFinished();

    await new Promise(resolve => requestAnimationFrame(resolve));
    assertTrue(fileThumbnailElement.classList.contains('entering'));

    // Simulate all animations finishing.
    resolveAnimation!(undefined);
    await microtasksFinished();

    assertFalse(fileThumbnailElement.classList.contains('entering'));
  });

  test('shows animation for exiting attachment', async () => {
    fileThumbnailElement.file =
        createComposeboxFile(1, {type: 'image/jpeg', objectUrl: 'data:foo'});
    await microtasksFinished();
    // Ensure the entering is completed before setting up the exiting mock.
    await new Promise(resolve => requestAnimationFrame(resolve));

    let resolveAnimation: (value: any) => void;
    fileThumbnailElement.getAnimations = () => {
      return [{
        finished: new Promise(resolve => {
          resolveAnimation = resolve;
        }),
      } as Animation];
    };

    let eventFired = false;
    fileThumbnailElement.addEventListener('delete-file', () => {
      eventFired = true;
    });

    fileThumbnailElement.$.removeImgButton.click();

    assertTrue(fileThumbnailElement.classList.contains('exiting'));
    assertFalse(eventFired);

    // Simulate all animations finishing.
    resolveAnimation!(undefined);
    await microtasksFinished();

    assertTrue(eventFired);
    assertFalse(fileThumbnailElement.classList.contains('exiting'));
  });

  test('ignores delete button clicks while already exiting', async () => {
    fileThumbnailElement.file =
        createComposeboxFile(1, {type: 'image/jpeg', objectUrl: 'data:foo'});
    await microtasksFinished();
    // Ensure the entering is completed before setting up the exiting mock.
    await new Promise(resolve => requestAnimationFrame(resolve));

    let resolveAnimation: (value: any) => void;
    fileThumbnailElement.getAnimations = () => {
      return [{
        finished: new Promise(resolve => {
          resolveAnimation = resolve;
        }),
      } as Animation];
    };

    let eventCount = 0;
    fileThumbnailElement.addEventListener('delete-file', () => {
      eventCount++;
    });

    // First click initiates exiting animation.
    fileThumbnailElement.$.removeImgButton.click();
    assertTrue(fileThumbnailElement.classList.contains('exiting'));

    // Second click should be ignored by the early return.
    fileThumbnailElement.$.removeImgButton.click();

    // Complete the animation.
    resolveAnimation!(undefined);
    await microtasksFinished();

    // Only one delete-file event should have been fired.
    assertEquals(1, eventCount);
  });
});
