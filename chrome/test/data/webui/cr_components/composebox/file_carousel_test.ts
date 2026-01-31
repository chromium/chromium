// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/file_carousel.js';
import 'chrome://new-tab-page/strings.m.js';

import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import {FileUploadStatus} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('FileCarouselTest', function() {
  let fileCarousel: ComposeboxFileCarouselElement;
  let resizeObserverCallback: ResizeObserverCallback;
  let resizeObserverDisconnectCalled = false;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    resizeObserverDisconnectCalled = false;

    // Mock ResizeObserver
    window.ResizeObserver = class ResizeObserverMock implements ResizeObserver {
      constructor(callback: ResizeObserverCallback) {
        resizeObserverCallback = callback;
      }
      observe(_target: Element, _options?: ResizeObserverOptions): void {}
      unobserve(_target: Element): void {}
      disconnect(): void {
        resizeObserverDisconnectCalled = true;
      }
    } as any;

    fileCarousel = document.createElement('cr-composebox-file-carousel');
    document.body.appendChild(fileCarousel);
    await microtasksFinished();
  });

  test('renders files', async () => {
    const files: ComposeboxFile[] = [
      {
        uuid: {high: 0n, low: 1n} as any,
        name: 'file1.txt',
        dataUrl: null,
        objectUrl: null,
        type: 'text/plain',
        status: FileUploadStatus.kNotUploaded,
        url: null,
        tabId: null,
        isDeletable: true,
      },
      {
        uuid: {high: 0n, low: 2n} as any,
        name: 'file2.txt',
        dataUrl: null,
        objectUrl: null,
        type: 'text/plain',
        status: FileUploadStatus.kNotUploaded,
        url: null,
        tabId: null,
        isDeletable: true,
      },
    ];
    fileCarousel.files = files;
    await microtasksFinished();

    const thumbnails =
        fileCarousel.shadowRoot.querySelectorAll('cr-composebox-file-thumbnail');
    assertEquals(2, thumbnails.length);
  });

  test('getThumbnailElementByUuid returns correct element', async () => {
    const uuid1 = {high: 0n, low: 1n} as any;
    const uuid2 = {high: 0n, low: 2n} as any;
    const files: ComposeboxFile[] = [
      {
        uuid: uuid1,
        name: 'file1.txt',
        dataUrl: null,
        objectUrl: null,
        type: 'text/plain',
        status: FileUploadStatus.kNotUploaded,
        url: null,
        tabId: null,
        isDeletable: true,
      },
      {
        uuid: uuid2,
        name: 'file2.txt',
        dataUrl: null,
        objectUrl: null,
        type: 'text/plain',
        status: FileUploadStatus.kNotUploaded,
        url: null,
        tabId: null,
        isDeletable: true,
      },
    ];
    fileCarousel.files = files;
    await microtasksFinished();

    const thumbnail1 = fileCarousel.getThumbnailElementByUuid(uuid1);
    assertTrue(!!thumbnail1);
    assertEquals(files[0], (thumbnail1 as any).file);

    const thumbnail2 = fileCarousel.getThumbnailElementByUuid(uuid2);
    assertTrue(!!thumbnail2);
    assertEquals(files[1], (thumbnail2 as any).file);

    const uuid3 = {high: 0n, low: 3n} as any;
    const thumbnail3 = fileCarousel.getThumbnailElementByUuid(uuid3);
    assertEquals(null, thumbnail3);
  });

  test('fires carousel-resize event on resize', async () => {
    const eventPromise = eventToPromise('carousel-resize', fileCarousel);

    // Trigger the callback
    resizeObserverCallback([], window.ResizeObserver as any);

    // The callback inside file_carousel.ts is debounced (20ms).
    // So we wait.
    await new Promise(resolve => setTimeout(resolve, 30));

    const event = await eventPromise;
    assertTrue(!!event);
    assertEquals(0, event.detail.height);
  });

  test('disconnects resize observer on disconnectedCallback', async () => {
    fileCarousel.remove();
    await microtasksFinished();
    assertTrue(resizeObserverDisconnectCalled);
  });
});
