// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ViewerThumbnailBarElement, ViewerThumbnailElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {ChangePageOrigin, PAINTED_ATTRIBUTE, PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, microtasksFinished, whenAttributeIs} from 'chrome://webui-test/test_util.js';

function createThumbnailBar(): ViewerThumbnailBarElement {
  document.body.innerHTML = '';
  const thumbnailBar = document.createElement('viewer-thumbnail-bar');
  thumbnailBar.inTest = true;
  document.body.appendChild(thumbnailBar);
  return thumbnailBar;
}

function getTestThumbnailBarHeight(): number {
  // Create a viewer-thumbnail element to get the standard height.
  document.body.innerHTML = '';
  const sizerThumbnail = document.createElement('viewer-thumbnail');
  sizerThumbnail.pageNumber = 1;
  document.body.appendChild(sizerThumbnail);
  return sizerThumbnail.offsetHeight;
}

function keydown(element: HTMLElement, key: string) {
  keyDownOn(element, 0, [], key);
}

function whenThumbnailPainted(thumbnail: ViewerThumbnailElement):
    Promise<void> {
  return whenAttributeIs(thumbnail, PAINTED_ATTRIBUTE, '');
}

function whenThumbnailCleared(thumbnail: ViewerThumbnailElement):
    Promise<void> {
  return whenAttributeIs(thumbnail, PAINTED_ATTRIBUTE, null);
}

// Unit tests for the viewer-thumbnail-bar element.
const tests = [
  // Test that the thumbnail bar has the correct number of thumbnails and
  // correspond to the right pages.
  async function testThumbnails() {
    const testDocLength = 10;
    const thumbnailBar = createThumbnailBar();
    thumbnailBar.docLength = testDocLength;

    await microtasksFinished();

    // Test that the correct number of viewer-thumbnail elements was created.
    const thumbnails =
        thumbnailBar.shadowRoot!.querySelectorAll('viewer-thumbnail');
    chrome.test.assertEq(testDocLength, thumbnails.length);

    function testNavigateThumbnail(
        thumbnail: ViewerThumbnailElement,
        expectedPageIndex: number): Promise<void> {
      const whenChanged = eventToPromise('change-page', thumbnailBar);
      thumbnail.getClickTarget().click();
      return whenChanged.then(e => {
        chrome.test.assertEq(expectedPageIndex, e.detail.page);
        chrome.test.assertEq(ChangePageOrigin.THUMBNAIL, e.detail.origin);
      });
    }

    // Test that each thumbnail has the correct page number and navigates to
    // the corresponding page.
    for (let i = 0; i < thumbnails.length; i++) {
      const thumbnail = thumbnails[i]!;
      chrome.test.assertEq(i + 1, thumbnail.pageNumber);
      await testNavigateThumbnail(thumbnail, i);
    }
    chrome.test.succeed();
  },
  async function testTriggerPaint() {
    const thumbnailBarHeight = getTestThumbnailBarHeight();

    // Clear HTML for just the thumbnail bar.
    const testDocLength = 8;
    const thumbnailBar = createThumbnailBar();
    thumbnailBar.docLength = testDocLength;

    // Set the height to one thumbnail. One thumbnail should be visible and
    // another should be hidden by intersecting the observer.
    thumbnailBar.style.height = `${thumbnailBarHeight}px`;
    thumbnailBar.style.display = 'block';

    // Remove any padding from the scroller.
    const scroller = thumbnailBar.$.thumbnails!;
    scroller.style.padding = '';

    await microtasksFinished();

    const thumbnails =
        thumbnailBar.shadowRoot!.querySelectorAll('viewer-thumbnail');

    // Only two thumbnails should be "painted" upon load.
    const whenRequestedPaintingFirst = [
      whenThumbnailPainted(thumbnails[0]!),
      whenThumbnailPainted(thumbnails[1]!),
    ];
    await Promise.all(whenRequestedPaintingFirst);

    chrome.test.assertEq(testDocLength, thumbnails.length);
    for (let i = 0; i < thumbnails.length; i++) {
      chrome.test.assertEq(i < 2, thumbnails[i]!.isPainted());
    }

    // Test that scrolling to the sixth thumbnail triggers 'paint-thumbnail'
    // for thumbnails 3 through 7. When on the sixth thumbnail, five
    // thumbnails above and one thumbnail below should also be painted because
    // of the 500% top and 100% bottom root margins.
    const whenRequestedPaintingNext = [];
    for (let i = 2; i < 7; i++) {
      whenRequestedPaintingNext.push(whenThumbnailPainted(thumbnails[i]!));
    }
    const thumbnailHeight = thumbnailBarHeight + 24;  // Including padding.
    scroller.scrollTop = 5 * thumbnailHeight;
    await Promise.all(whenRequestedPaintingNext);

    // First seven thumbnails should be painted.
    for (let i = 0; i < thumbnails.length; i++) {
      chrome.test.assertEq(i < 7, thumbnails[i]!.isPainted());
    }

    // Test that scrolling down to the eighth thumbnail will clear the
    // thumbnails outside the root margin, namely the first two. A paint
    // should also be triggered for the eighth thumbnail.
    const whenRequestedPaintingLast = [
      whenThumbnailPainted(thumbnails[7]!),
      whenThumbnailCleared(thumbnails[0]!),
      whenThumbnailCleared(thumbnails[1]!),
    ];
    scroller.scrollTop = 7 * thumbnailHeight;
    await Promise.all(whenRequestedPaintingLast);

    // Only first two thumbnails should not be painted.
    for (let i = 0; i < thumbnails.length; i++) {
      chrome.test.assertEq(i > 1, thumbnails[i]!.isPainted());
    }
    chrome.test.succeed();
  },
  async function testThumbnailForwardFocus() {
    const testDocLength = 10;
    const thumbnailBar = createThumbnailBar();
    thumbnailBar.docLength = testDocLength;

    await microtasksFinished();

    function waitForwardFocus(pageNumber: number): Promise<void> {
      // Reset focus.
      thumbnailBar.blur();

      const toThumbnail = thumbnailBar.getThumbnailForPage(pageNumber)!;
      const whenActiveThumbnailFocused = eventToPromise('focus', toThumbnail);

      // Calling focus() on `thumbnailBar` in this test doesn't trigger the
      // event listener, but manually dispatching an event does.
      thumbnailBar.dispatchEvent(new FocusEvent('focus'));
      return whenActiveThumbnailFocused;
    }

    // When there's no active page, focus should forward to the first
    // thumbnail.
    await waitForwardFocus(1);

    // When there's an active page, focus should forward to the thumbnail of
    // the active page.
    let activePage = 3;
    thumbnailBar.activePage = activePage;
    await waitForwardFocus(activePage);

    activePage = 10;
    thumbnailBar.activePage = activePage;
    await waitForwardFocus(activePage);
    chrome.test.succeed();
  },
  async function testThumbnailUpDownFocus() {
    const testDocLength = 2;
    const thumbnailBar = createThumbnailBar();
    thumbnailBar.docLength = testDocLength;

    await microtasksFinished();

    thumbnailBar.activePage = 1;
    let whenChanged = eventToPromise('change-page', thumbnailBar);
    keydown(thumbnailBar, 'ArrowDown');
    let event = await whenChanged;

    // The event contains the zero-based page index.
    chrome.test.assertEq(1, event.detail.page);

    thumbnailBar.activePage = 2;
    whenChanged = eventToPromise('change-page', thumbnailBar);
    keydown(thumbnailBar, 'ArrowUp');
    event = await whenChanged;

    // The event contains the zero-based page index.
    chrome.test.assertEq(0, event.detail.page);

    chrome.test.succeed();
  },
  async function testThumbnailLeftRightSelect() {
    const testDocLength = 2;
    const thumbnailBar = createThumbnailBar();
    thumbnailBar.docLength = testDocLength;

    await microtasksFinished();

    thumbnailBar.activePage = 1;
    let whenChanged = eventToPromise('change-page', thumbnailBar);
    keydown(thumbnailBar, 'ArrowRight');
    let event = await whenChanged;

    // The event contains the zero-based page index.
    chrome.test.assertEq(1, event.detail.page);

    thumbnailBar.activePage = 2;
    whenChanged = eventToPromise('change-page', thumbnailBar);
    keydown(thumbnailBar, 'ArrowLeft');
    event = await whenChanged;

    // The event contains the zero-based page index.
    chrome.test.assertEq(0, event.detail.page);

    chrome.test.succeed();
  },
  async function testReactToNoPlugin() {
    const thumbnailBar = createThumbnailBar();
    thumbnailBar.docLength = 1;

    // Deactivate the PluginController, causing the thumbnails to hide.
    const pluginController = PluginController.getInstance();
    pluginController.isActive = false;

    await microtasksFinished();

    const scroller = thumbnailBar.$.thumbnails;
    chrome.test.assertTrue(scroller.hidden);

    const thumbnail =
        thumbnailBar.shadowRoot!.querySelector('viewer-thumbnail')!;

    const whenPaintTriggered = whenThumbnailPainted(thumbnail).then(() => {
      // The thumbnail shouldn't paint when the controller is inactive.
      if (!pluginController.isActive) {
        chrome.test.fail();
      }
    });

    // Give the test a chance to fail.
    await microtasksFinished();

    // The thumbnail should paint when reactivating the plugin.
    pluginController.isActive = true;
    await microtasksFinished();
    chrome.test.assertFalse(scroller.hidden);
    await whenPaintTriggered;

    // The thumbnail should clear when deactivating the plugin.
    pluginController.isActive = false;
    await microtasksFinished();
    chrome.test.assertTrue(scroller.hidden);
    await whenThumbnailCleared(thumbnail);
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
