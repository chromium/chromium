// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/file_carousel.js';
import 'chrome://new-tab-page/strings.m.js';

import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import {FileUploadStatus} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

  function getFileCarouselContainer(): HTMLElement {
    return fileCarousel.shadowRoot.querySelector('.file-carousel-container')!;
  }

  function createFile(n: number): ComposeboxFile {
    return {
      uuid: {high: 0n, low: BigInt(n)} as any,
      name: `file${n}.txt`,
      dataUrl: null,
      objectUrl: null,
      type: 'text/plain',
      status: FileUploadStatus.kUploadStarted,
      url: null,
      tabId: null,
      isDeletable: true,
    };
  }

  test('renders files', async () => {
    const files: ComposeboxFile[] = [
      createFile(1),
      createFile(2),
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
        status: FileUploadStatus.kUploadStarted,
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
        status: FileUploadStatus.kUploadStarted,
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

  test('toggles gradients based on scroll', async () => {
    fileCarousel.enableScrolling = true;
    fileCarousel.files = [createFile(1), createFile(2), createFile(3)];
    await microtasksFinished();

    const container = getFileCarouselContainer();
    Object.defineProperty(container, 'clientHeight', {value: 50});
    Object.defineProperty(container, 'scrollHeight', {value: 100});

    // Initial state: scrolled to top.
    Object.defineProperty(container, 'scrollTop', {value: 0, writable: true});
    container.dispatchEvent(new Event('scroll'));
    await microtasksFinished();
    assertTrue(fileCarousel.hasAttribute('gradient-top-hidden'));
    assertFalse(fileCarousel.hasAttribute('gradient-bottom-hidden'));

    // Scrolled to middle.
    container.scrollTop = 25;
    container.dispatchEvent(new Event('scroll'));
    await microtasksFinished();
    assertFalse(fileCarousel.hasAttribute('gradient-top-hidden'));
    assertFalse(fileCarousel.hasAttribute('gradient-bottom-hidden'));

    // Scrolled to bottom.
    container.scrollTop = 50;
    container.dispatchEvent(new Event('scroll'));
    await microtasksFinished();
    assertFalse(fileCarousel.hasAttribute('gradient-top-hidden'));
    assertTrue(fileCarousel.hasAttribute('gradient-bottom-hidden'));
  });

  test('hides gradients when no scrollbar', async () => {
    fileCarousel.enableScrolling = true;
    fileCarousel.files = [createFile(1)];
    await microtasksFinished();

    const container = getFileCarouselContainer();
    Object.defineProperty(container, 'clientHeight', {value: 100});
    Object.defineProperty(container, 'scrollHeight', {value: 50});

    container.dispatchEvent(new Event('scroll'));
    await microtasksFinished();

    assertTrue(fileCarousel.hasAttribute('gradient-top-hidden'));
    assertTrue(fileCarousel.hasAttribute('gradient-bottom-hidden'));
  });

  test('scrolls to bottom when files are added', async () => {
    fileCarousel.enableScrolling = true;
    fileCarousel.files = [createFile(1)];
    await microtasksFinished();

    const container = getFileCarouselContainer();
    Object.defineProperty(container, 'clientHeight', {value: 50});
    Object.defineProperty(container, 'scrollHeight', {value: 100});
    container.scrollTop = 0;
    let scrollToOptions: ScrollToOptions|undefined;
    container.scrollTo = (options) => {
      if (typeof options === 'object') {
        scrollToOptions = options;
      }
    };

    fileCarousel.files = [createFile(1), createFile(2)];
    await microtasksFinished();

    assertTrue(!!scrollToOptions);
    assertEquals(100, scrollToOptions.top);
    assertEquals('smooth', scrollToOptions.behavior);
  });

  test('does not scroll to bottom when scrolling disabled', async () => {
    fileCarousel.enableScrolling = false;
    fileCarousel.files = [createFile(1)];
    await microtasksFinished();

    const container = getFileCarouselContainer();
    let scrollToCalled = false;
    container.scrollTo = () => {
      scrollToCalled = true;
    };

    fileCarousel.files = [createFile(1), createFile(2)];
    await microtasksFinished();

    assertFalse(scrollToCalled);
  });
});
