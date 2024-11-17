// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Point, Rect, Viewport} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {FittingType, PAGE_SHADOW, SwipeDirection} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {isMac} from 'chrome://resources/js/platform.js';

import type {MockPdfPluginElement} from './test_util.js';
import {createMockPdfPluginForTest, getZoomableViewport, MockDocumentDimensions, MockElement, MockSizer, MockViewportChangedCallback} from './test_util.js';

const SCROLLBAR_WIDTH: number = 15;

class ScrollEventCounter {
  count: number = 0;

  constructor() {
    window.addEventListener('scroll', () => ++this.count);
  }
}

/**
 * Simulates acknowledgements to all "syncScrollToRemote" messages.
 */
function ackAllScrollToRemoteMessages(
    viewport: Viewport, plugin: MockPdfPluginElement) {
  for (const message of plugin.messages) {
    if (message.type === 'syncScrollToRemote') {
      viewport.ackScrollToRemote(message);
    }
  }
}

function assertRoughlyEquals(
    expected: number, actual: number, tolerance: number) {
  chrome.test.assertTrue(
      Math.abs(expected - actual) <= tolerance,
      `|${expected} - ${actual}| > ${tolerance}`);
}

function setPluginPosition(x: number, y: number) {
  const plugin = document.querySelector<HTMLElement>('#plugin')!;
  plugin.style.position = 'absolute';
  plugin.style.left = x + 'px';
  plugin.style.top = y + 'px';
}

function whenRequestAnimationFrame(): Promise<void> {
  return new Promise(resolve => window.requestAnimationFrame(() => resolve()));
}

const tests = [
  function testScrollbarWidth() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 43, 1);

    chrome.test.assertEq(43, viewport.scrollbarWidth);
    chrome.test.succeed();
  },

  function testOverlayScrollbarWidthLocal() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 43, 1);

    chrome.test.assertEq(16, viewport.overlayScrollbarWidth);
    chrome.test.succeed();
  },

  function testOverlayScrollbarWidthRemote() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 43, 1);
    viewport.setRemoteContent(createMockPdfPluginForTest());

    chrome.test.assertEq(isMac ? 16 : 43, viewport.overlayScrollbarWidth);
    chrome.test.succeed();
  },

  function testDocumentNeedsScrollbars() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 10, 1);

    viewport.setDocumentDimensions(new MockDocumentDimensions(90, 90));
    let scrollbars = viewport.documentNeedsScrollbars(1);
    chrome.test.assertFalse(scrollbars.vertical);
    chrome.test.assertFalse(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(100.49, 100.49));
    scrollbars = viewport.documentNeedsScrollbars(1);
    chrome.test.assertFalse(scrollbars.vertical);
    chrome.test.assertFalse(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(100.5, 100.5));
    scrollbars = viewport.documentNeedsScrollbars(1);
    chrome.test.assertTrue(scrollbars.vertical);
    chrome.test.assertTrue(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(110, 110));
    scrollbars = viewport.documentNeedsScrollbars(1);
    chrome.test.assertTrue(scrollbars.vertical);
    chrome.test.assertTrue(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(90, 101));
    scrollbars = viewport.documentNeedsScrollbars(1);
    chrome.test.assertTrue(scrollbars.vertical);
    chrome.test.assertFalse(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(101, 90));
    scrollbars = viewport.documentNeedsScrollbars(1);
    chrome.test.assertFalse(scrollbars.vertical);
    chrome.test.assertTrue(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(40, 51));
    scrollbars = viewport.documentNeedsScrollbars(2);
    chrome.test.assertTrue(scrollbars.vertical);
    chrome.test.assertFalse(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(51, 40));
    scrollbars = viewport.documentNeedsScrollbars(2);
    chrome.test.assertFalse(scrollbars.vertical);
    chrome.test.assertTrue(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(101, 202));
    scrollbars = viewport.documentNeedsScrollbars(0.5);
    chrome.test.assertTrue(scrollbars.vertical);
    chrome.test.assertFalse(scrollbars.horizontal);
    chrome.test.succeed();
  },

  function testSetZoom() {
    const mockSizer = new MockSizer();
    const mockWindow = new MockElement(100, 100, mockSizer);
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);

    // Test setting the zoom without the document dimensions set. The sizer
    // shouldn't change size.
    mockCallback.reset();
    viewport.setZoom(0.5);
    chrome.test.assertEq(0.5, viewport.getZoom());
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('0px', mockSizer.style.width);
    chrome.test.assertEq('0px', mockSizer.style.height);
    chrome.test.assertEq(0, mockWindow.scrollLeft);
    chrome.test.assertEq(0, mockWindow.scrollTop);

    viewport.setZoom(1);
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 200));

    // Test zooming out.
    mockCallback.reset();
    viewport.setZoom(0.5);
    chrome.test.assertEq(0.5, viewport.getZoom());
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('100px', mockSizer.style.width);
    chrome.test.assertEq('100px', mockSizer.style.height);

    // Test zooming in.
    mockCallback.reset();
    viewport.setZoom(2);
    chrome.test.assertEq(2, viewport.getZoom());
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('400px', mockSizer.style.width);
    chrome.test.assertEq('400px', mockSizer.style.height);

    // Test that the scroll position scales correctly. It scales relative to the
    // top-left of the page.
    viewport.setZoom(1);
    mockWindow.scrollLeft = 50;
    mockWindow.scrollTop = 50;
    viewport.setZoom(2);
    chrome.test.assertEq('400px', mockSizer.style.width);
    chrome.test.assertEq('400px', mockSizer.style.height);
    chrome.test.assertEq(100, mockWindow.scrollLeft);
    chrome.test.assertEq(100, mockWindow.scrollTop);
    mockWindow.scrollTo(250, 250);
    viewport.setZoom(1);
    chrome.test.assertEq('200px', mockSizer.style.width);
    chrome.test.assertEq('200px', mockSizer.style.height);
    chrome.test.assertEq(100, mockWindow.scrollLeft);
    chrome.test.assertEq(100, mockWindow.scrollTop);

    const documentDimensions = new MockDocumentDimensions(0, 0);
    documentDimensions.addPage(200, 200);
    viewport.setDocumentDimensions(documentDimensions);
    mockWindow.scrollTo(0, 0);
    viewport.fitToPage();
    viewport.setZoom(1);
    chrome.test.assertEq(FittingType.NONE, viewport.fittingType);
    chrome.test.assertEq('200px', mockSizer.style.width);
    chrome.test.assertEq('200px', mockSizer.style.height);
    chrome.test.assertEq(0, mockWindow.scrollLeft);
    chrome.test.assertEq(0, mockWindow.scrollTop);

    viewport.fitToWidth();
    viewport.setZoom(1);
    chrome.test.assertEq(FittingType.NONE, viewport.fittingType);
    chrome.test.assertEq('200px', mockSizer.style.width);
    chrome.test.assertEq('200px', mockSizer.style.height);
    chrome.test.assertEq(0, mockWindow.scrollLeft);
    chrome.test.assertEq(0, mockWindow.scrollTop);

    chrome.test.succeed();
  },

  // Regression test for https://crbug.com/1202725.
  function testGetMostVisiblePageZeroSize() {
    // This happens when the PDF is in a hidden iframe.
    const mockWindow = new MockElement(0, 0, null);
    const viewport = getZoomableViewport(mockWindow, new MockSizer(), 0, 1);

    const documentDimensions = new MockDocumentDimensions(100, 100);
    documentDimensions.addPage(50, 100);
    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(100, 200);
    viewport.setDocumentDimensions(documentDimensions);

    // Zoom is computed as 0.
    chrome.test.assertEq(0, viewport.getZoom());
    // This call should not crash.
    chrome.test.assertEq(0, viewport.getMostVisiblePage());
    chrome.test.succeed();
  },

  function testGetMostVisiblePage() {
    const mockWindow = new MockElement(100, 100, null);
    const viewport = getZoomableViewport(mockWindow, new MockSizer(), 0, 1);

    const documentDimensions = new MockDocumentDimensions(100, 100);
    documentDimensions.addPage(50, 100);
    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(100, 200);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    // Scrolled to the start of the first page.
    mockWindow.scrollTo(0, 0);
    chrome.test.assertEq(0, viewport.getMostVisiblePage());

    // Scrolled to the start of the second page.
    mockWindow.scrollTo(0, 100);
    chrome.test.assertEq(1, viewport.getMostVisiblePage());

    // Scrolled half way through the first page.
    mockWindow.scrollTo(0, 50);
    chrome.test.assertEq(0, viewport.getMostVisiblePage());

    // Scrolled just over half way through the first page.
    mockWindow.scrollTo(0, 51);
    chrome.test.assertEq(1, viewport.getMostVisiblePage());

    // Scrolled most of the way through the second page.
    mockWindow.scrollTo(0, 180);
    chrome.test.assertEq(2, viewport.getMostVisiblePage());

    // Scrolled just past half way through the second page.
    mockWindow.scrollTo(0, 160);
    chrome.test.assertEq(1, viewport.getMostVisiblePage());

    // Scrolled just over half way through the first page with 2x zoom.
    // Despite having a larger intersection height, the proportional
    // intersection area for the second page is less than the proportional
    // intersection area for the first page.
    viewport.setZoom(2);
    mockWindow.scrollTo(0, 151);
    chrome.test.assertEq(0, viewport.getMostVisiblePage());

    // After scrolling further down, we reach a point where proportionally
    // more area of the second page intersects with the viewport than the first.
    mockWindow.scrollTo(0, 170);
    chrome.test.assertEq(1, viewport.getMostVisiblePage());

    // Zoom out so that more than one page fits and scroll to the bottom.
    viewport.setZoom(0.4);
    mockWindow.scrollTo(0, 160);
    chrome.test.assertEq(2, viewport.getMostVisiblePage());

    // Zoomed out with the entire document visible.
    viewport.setZoom(0.25);
    mockWindow.scrollTo(0, 0);
    chrome.test.assertEq(0, viewport.getMostVisiblePage());
    chrome.test.succeed();
  },

  function testGetMostVisiblePageForTwoUpView() {
    const mockWindow = new MockElement(400, 500, null);
    const viewport = getZoomableViewport(mockWindow, new MockSizer(), 0, 1);

    const documentDimensions = new MockDocumentDimensions(
        100, 100,
        {direction: 0, defaultPageOrientation: 0, twoUpViewEnabled: true});
    documentDimensions.addPageForTwoUpView(100, 0, 300, 400);
    documentDimensions.addPageForTwoUpView(400, 0, 400, 300);
    documentDimensions.addPageForTwoUpView(0, 400, 400, 250);
    documentDimensions.addPageForTwoUpView(400, 400, 200, 400);
    documentDimensions.addPageForTwoUpView(50, 800, 350, 200);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    // Scrolled to the start of the first page.
    mockWindow.scrollTo(0, 0);
    chrome.test.assertEq(0, viewport.getMostVisiblePage());

    // Scrolled such that only the first and third pages are visible.
    mockWindow.scrollTo(0, 200);
    chrome.test.assertEq(2, viewport.getMostVisiblePage());

    // Scrolled such that only the second and fourth pages are visible.
    mockWindow.scrollTo(400, 200);
    chrome.test.assertEq(3, viewport.getMostVisiblePage());

    // Scroll such that first to fourth pages are visible.
    mockWindow.scrollTo(200, 200);
    chrome.test.assertEq(3, viewport.getMostVisiblePage());

    // Zoomed out with the entire document visible.
    viewport.setZoom(0.25);
    mockWindow.scrollTo(0, 0);
    chrome.test.assertEq(0, viewport.getMostVisiblePage());
    chrome.test.succeed();
  },

  function testSetFittingType() {
    const viewport = getZoomableViewport(
        new MockElement(400, 500, null), new MockSizer(), 0, 1);

    viewport.setFittingType(FittingType.FIT_TO_PAGE);
    chrome.test.assertEq(FittingType.FIT_TO_PAGE, viewport.fittingType);

    viewport.setFittingType(FittingType.FIT_TO_WIDTH);
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, viewport.fittingType);

    viewport.setFittingType(FittingType.FIT_TO_HEIGHT);
    chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, viewport.fittingType);

    const documentDimensions = new MockDocumentDimensions();
    documentDimensions.addPage(0, 0);
    viewport.setDocumentDimensions(documentDimensions);

    const params = {
      page: 0,
      boundingBox: {x: 0, y: 0, width: 1, height: 1},
      fitToWidth: true,
    };
    viewport.setFittingType(FittingType.FIT_TO_BOUNDING_BOX, params);
    chrome.test.assertEq(FittingType.FIT_TO_BOUNDING_BOX, viewport.fittingType);

    viewport.setFittingType(FittingType.FIT_TO_BOUNDING_BOX_WIDTH, params);
    chrome.test.assertEq(
        FittingType.FIT_TO_BOUNDING_BOX_WIDTH, viewport.fittingType);

    params.fitToWidth = false;
    viewport.setFittingType(FittingType.FIT_TO_BOUNDING_BOX_HEIGHT, params);
    chrome.test.assertEq(
        FittingType.FIT_TO_BOUNDING_BOX_HEIGHT, viewport.fittingType);

    viewport.setFittingType(FittingType.NONE);
    chrome.test.assertEq(FittingType.NONE, viewport.fittingType);

    chrome.test.succeed();
  },

  function testFitToWidth() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    let viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();

    function assertZoomed(
        expectedMockWidth: number, expectedMockHeight: number,
        expectedZoom: number) {
      chrome.test.assertEq(FittingType.FIT_TO_WIDTH, viewport.fittingType);
      chrome.test.assertTrue(mockCallback.wasCalled);
      chrome.test.assertEq(`${expectedMockWidth}px`, mockSizer.style.width);
      chrome.test.assertEq(`${expectedMockHeight}px`, mockSizer.style.height);
      chrome.test.assertEq(expectedZoom, viewport.getZoom());
    }

    function testForSize(
        pageWidth: number, pageHeight: number, expectedMockWidth: number,
        expectedMockHeight: number, expectedZoom: number) {
      documentDimensions.reset();
      documentDimensions.addPage(pageWidth, pageHeight);
      viewport.setDocumentDimensions(documentDimensions);
      viewport.setZoom(0.1);
      mockCallback.reset();
      viewport.fitToWidth();
      assertZoomed(expectedMockWidth, expectedMockHeight, expectedZoom);
    }

    function assertPositionAndZoom(
        expectedPosition: Point, expectedZoom: number) {
      chrome.test.assertEq(FittingType.FIT_TO_WIDTH, viewport.fittingType);
      chrome.test.assertTrue(mockCallback.wasCalled);
      chrome.test.assertEq(expectedPosition, viewport.position);
      chrome.test.assertEq(expectedZoom, viewport.getZoom());
    }

    function testForPosition(
        expectedX: number, expectedY: number, expectedZoom: number,
        page?: number, viewPosition?: number) {
      viewport.setZoom(0.1);
      mockCallback.reset();
      viewport.fitToWidth({page, viewPosition});
      assertPositionAndZoom({x: expectedX, y: expectedY}, expectedZoom);
    }

    // Document width which matches the window width.
    testForSize(100, 100, 100, 100, 1);

    // Document width which matches the window width, but taller.
    testForSize(100, 200, 100, 200, 1);

    // Document width which matches the window width, but shorter.
    testForSize(100, 50, 100, 50, 1);

    // Document width which is twice the size of the window width.
    testForSize(200, 100, 100, 50, 0.5);

    // Document width which is half the size of the window width.
    testForSize(50, 100, 100, 200, 2);

    // Test params.
    documentDimensions.reset();
    documentDimensions.addPage(50, 400);
    documentDimensions.addPage(100, 600);
    documentDimensions.addPage(200, 800);
    viewport.setDocumentDimensions(documentDimensions);

    testForPosition(0, 0, 0.5, 0, undefined);
    testForPosition(0, 200, 0.5, 1, undefined);
    testForPosition(0, 500, 0.5, 2, undefined);

    testForPosition(0, 0, 0.5, 0, 0);
    testForPosition(0, 200, 0.5, 1, 0);
    testForPosition(0, 500, 0.5, 2, 0);

    testForPosition(0, 5.5, 0.5, 0, 11);
    testForPosition(0, 211, 0.5, 1, 22);
    testForPosition(0, 527.5, 0.5, 2, 55);

    // Check that the viewPosition offset uses the current page if page is not
    // provided.
    viewport.goToPage(0);
    testForPosition(0, 5.5, 0.5, undefined, 11);
    viewport.goToPage(1);
    testForPosition(0, 211, 0.5, undefined, 22);
    viewport.goToPage(2);
    testForPosition(0, 527.5, 0.5, undefined, 55);

    // Test that the scroll position stays the same relative to the page after
    // fit to width is called.
    documentDimensions.reset();
    documentDimensions.addPage(50, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);
    mockWindow.scrollTo(0, 100);
    mockCallback.reset();
    viewport.fitToWidth();
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, viewport.fittingType);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(2, viewport.getZoom());
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(200, viewport.position.y);

    // Test fitting works with scrollbars. The page will need to be zoomed to
    // fit to width, which will cause the page height to span outside of the
    // viewport, triggering 15px scrollbars to be shown.
    viewport = getZoomableViewport(mockWindow, mockSizer, SCROLLBAR_WIDTH, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    documentDimensions.reset();
    documentDimensions.addPage(50, 100);
    viewport.setDocumentDimensions(documentDimensions);
    mockCallback.reset();
    viewport.fitToWidth();
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, viewport.fittingType);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('85px', mockSizer.style.width);
    chrome.test.assertEq(1.7, viewport.getZoom());
    chrome.test.succeed();
  },

  function testFitToPage() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();

    function assertZoomed(
        expectedMockWidth: number, expectedMockHeight: number,
        expectedZoom: number) {
      chrome.test.assertEq(FittingType.FIT_TO_PAGE, viewport.fittingType);
      chrome.test.assertTrue(mockCallback.wasCalled);
      chrome.test.assertEq(`${expectedMockWidth}px`, mockSizer.style.width);
      chrome.test.assertEq(`${expectedMockHeight}px`, mockSizer.style.height);
      chrome.test.assertEq(expectedZoom, viewport.getZoom());
    }

    function testForSize(
        pageWidth: number, pageHeight: number, expectedMockWidth: number,
        expectedMockHeight: number, expectedZoom: number) {
      documentDimensions.reset();
      documentDimensions.addPage(pageWidth, pageHeight);
      viewport.setDocumentDimensions(documentDimensions);
      viewport.setZoom(0.1);
      mockCallback.reset();
      viewport.fitToPage();
      assertZoomed(expectedMockWidth, expectedMockHeight, expectedZoom);
    }

    function assertPositionAndZoom(
        expectedPosition: Point, expectedZoom: number) {
      chrome.test.assertEq(FittingType.FIT_TO_PAGE, viewport.fittingType);
      chrome.test.assertTrue(mockCallback.wasCalled);
      chrome.test.assertEq(expectedPosition, viewport.position);
      chrome.test.assertEq(expectedZoom, viewport.getZoom());
    }

    function testForPosition(
        expectedX: number, expectedY: number, expectedZoom: number,
        page?: number, scrollToTop?: boolean) {
      viewport.setZoom(0.1);
      mockCallback.reset();
      viewport.fitToPage({page, scrollToTop});
      assertPositionAndZoom({x: expectedX, y: expectedY}, expectedZoom);
    }

    // Page size which matches the window size.
    testForSize(100, 100, 100, 100, 1);

    // Page size whose width is larger than its height.
    testForSize(200, 100, 100, 50, 0.5);

    // Page size whose height is larger than its width.
    testForSize(100, 200, 50, 100, 0.5);

    // Page size smaller than the window size in width but not height.
    testForSize(50, 100, 50, 100, 1);

    // Page size smaller than the window size in height but not width.
    testForSize(100, 50, 100, 50, 1);

    // Page size smaller than the window size in both width and height.
    testForSize(25, 50, 50, 100, 2);

    // Page size smaller in one dimension and bigger in another.
    testForSize(50, 200, 25, 100, 0.5);

    // Test params.
    documentDimensions.reset();
    documentDimensions.addPage(50, 400);
    documentDimensions.addPage(100, 500);
    documentDimensions.addPage(200, 1000);
    viewport.setDocumentDimensions(documentDimensions);

    testForPosition(0, 0, 0.25, 0);
    testForPosition(0, 80, 0.2, 1);
    testForPosition(0, 90, 0.1, 2);

    // Check that the current scroll position is maintained if `page` is
    // undefined and `scrollToTop` is false.
    viewport.goToPageAndXy(0, 10, 20);
    testForPosition(2.5, 5, 0.25, undefined, false);
    viewport.goToPageAndXy(1, 10, 20);
    testForPosition(2, 84, 0.2, undefined, false);
    viewport.goToPageAndXy(1, 30, 50);
    testForPosition(6, 90, 0.2, undefined, false);

    // Check that `scrollToTop` value is ignored if `page` is defined.
    testForPosition(0, 80, 0.2, 1, false);

    // Test that when there are multiple pages the height of the most visible
    // page and the width of the widest page are sized to.
    documentDimensions.reset();
    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(200, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);
    mockWindow.scrollTo(0, 0);
    mockCallback.reset();
    viewport.fitToPage();
    assertZoomed(100, 250, 0.5);

    viewport.setZoom(1);
    mockWindow.scrollTo(0, 100);
    mockCallback.reset();
    viewport.fitToPage();
    assertZoomed(50, 125, 0.25);

    // Test that the top of the most visible page is scrolled to.
    documentDimensions.reset();
    documentDimensions.addPage(200, 200);
    documentDimensions.addPage(100, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);
    mockWindow.scrollTo(0, 0);
    viewport.fitToPage();
    chrome.test.assertEq(FittingType.FIT_TO_PAGE, viewport.fittingType);
    chrome.test.assertEq(0.5, viewport.getZoom());
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);
    viewport.setZoom(1);
    mockWindow.scrollTo(0, 175);
    viewport.fitToPage();
    chrome.test.assertEq(0.25, viewport.getZoom());
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(50, viewport.position.y);

    // Test that when the window size changes, fit-to-page occurs but does not
    // scroll to the top of the page (it should stay at the scaled scroll
    // position).
    mockWindow.scrollTo(0, 0);
    viewport.fitToPage();
    chrome.test.assertEq(FittingType.FIT_TO_PAGE, viewport.fittingType);
    chrome.test.assertEq(0.5, viewport.getZoom());
    mockWindow.scrollTo(0, 10);
    mockWindow.setSize(50, 50);
    chrome.test.assertEq(0.25, viewport.getZoom());
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(5, viewport.position.y);

    chrome.test.succeed();
  },

  function testFitToHeight() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();

    function assertZoomed(
        expectedMockWidth: number, expectedMockHeight: number,
        expectedZoom: number) {
      chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, viewport.fittingType);
      chrome.test.assertTrue(mockCallback.wasCalled);
      chrome.test.assertEq(`${expectedMockWidth}px`, mockSizer.style.width);
      chrome.test.assertEq(`${expectedMockHeight}px`, mockSizer.style.height);
      chrome.test.assertEq(expectedZoom, viewport.getZoom());
    }

    function testForSize(
        pageWidth: number, pageHeight: number, expectedMockWidth: number,
        expectedMockHeight: number, expectedZoom: number) {
      documentDimensions.reset();
      documentDimensions.addPage(pageWidth, pageHeight);
      viewport.setDocumentDimensions(documentDimensions);
      viewport.setZoom(0.1);
      mockCallback.reset();
      viewport.fitToHeight({page: viewport.getMostVisiblePage()});
      assertZoomed(expectedMockWidth, expectedMockHeight, expectedZoom);
    }

    function assertPositionAndZoom(
        expectedPosition: Point, expectedZoom: number) {
      chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, viewport.fittingType);
      chrome.test.assertTrue(mockCallback.wasCalled);
      chrome.test.assertEq(expectedPosition, viewport.position);
      chrome.test.assertEq(expectedZoom, viewport.getZoom());
    }

    function testForPosition(
        expectedX: number, expectedY: number, expectedZoom: number,
        page?: number, viewPosition?: number) {
      viewport.setZoom(0.1);
      mockCallback.reset();
      viewport.fitToHeight({page, viewPosition});
      assertPositionAndZoom({x: expectedX, y: expectedY}, expectedZoom);
    }

    // Page size which matches the window size.
    testForSize(100, 100, 100, 100, 1);

    // Page size wider than window but same height.
    testForSize(200, 100, 200, 100, 1);

    // Page size narrower than window but same height.
    testForSize(50, 100, 50, 100, 1);

    // Page size shorter than window.
    testForSize(100, 50, 200, 100, 2);

    // Page size taller than window.
    testForSize(100, 200, 50, 100, 0.5);

    // Test params.
    documentDimensions.reset();
    documentDimensions.addPage(50, 400);
    documentDimensions.addPage(100, 500);
    documentDimensions.addPage(200, 1000);
    viewport.setDocumentDimensions(documentDimensions);

    testForPosition(0, 0, 0.25, 0);
    testForPosition(0, 80, 0.2, 1);
    testForPosition(0, 90, 0.1, 2);

    testForPosition(0, 0, 0.25, 0, 0);
    testForPosition(0, 80, 0.2, 1, 0);
    testForPosition(0, 90, 0.1, 2, 0);

    testForPosition(2.75, 0, 0.25, 0, 11);
    testForPosition(4, 80, 0.2, 1, 20);
    testForPosition(5.5, 90, 0.1, 2, 55);

    // Check that the viewPosition offset uses the current page if page is not
    // provided.
    viewport.goToPageAndXy(0, 10, 0);
    testForPosition(2.75, 0, 0.25, undefined, 11);
    viewport.goToPageAndXy(1, 10, 0);
    testForPosition(4, 80, 0.2, undefined, 20);
    viewport.goToPageAndXy(2, 10, 0);
    testForPosition(5.5, 90, 0.1, undefined, 55);

    // Check that the current scroll position is maintained if the page and
    // viewPosition params are missing.
    viewport.goToPageAndXy(1, 10, 0);
    testForPosition(2, 80, 0.2);
    viewport.goToPageAndXy(1, 20, 10);
    testForPosition(4, 82, 0.2);
    viewport.goToPageAndXy(1, 50, 50);
    testForPosition(10, 90, 0.2);

    // Test that when there are multiple pages the height of the most visible
    // page and the width of the widest page are sized to.
    documentDimensions.reset();
    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(200, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);
    mockWindow.scrollTo(0, 0);
    chrome.test.assertEq(0, viewport.getMostVisiblePage());
    mockCallback.reset();
    viewport.fitToHeight({page: viewport.getMostVisiblePage()});
    assertZoomed(200, 500, 1);

    viewport.setZoom(1);
    mockWindow.scrollTo(0, 100);
    chrome.test.assertEq(1, viewport.getMostVisiblePage());
    mockCallback.reset();
    viewport.fitToHeight({page: viewport.getMostVisiblePage()});
    assertZoomed(50, 125, 0.25);

    // Test that the top of the most visible page is scrolled to.
    documentDimensions.reset();
    documentDimensions.addPage(200, 200);
    documentDimensions.addPage(100, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);
    mockWindow.scrollTo(0, 0);
    chrome.test.assertEq(0, viewport.getMostVisiblePage());
    viewport.fitToHeight({page: viewport.getMostVisiblePage()});
    chrome.test.assertEq(0, viewport.getMostVisiblePage());
    chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, viewport.fittingType);
    chrome.test.assertEq(0.5, viewport.getZoom());
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);
    viewport.setZoom(1);
    mockWindow.scrollTo(0, 175);
    chrome.test.assertEq(1, viewport.getMostVisiblePage());
    viewport.fitToHeight({page: viewport.getMostVisiblePage()});
    chrome.test.assertEq(1, viewport.getMostVisiblePage());
    chrome.test.assertEq(0.25, viewport.getZoom());
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(50, viewport.position.y);

    // Test that when the window size changes, fit-to-height occurs but does not
    // scroll to the top of the page (it should stay at the scaled scroll
    // position).
    mockWindow.scrollTo(0, 0);
    viewport.fitToHeight({page: viewport.getMostVisiblePage()});
    chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, viewport.fittingType);
    chrome.test.assertEq(0.5, viewport.getZoom());
    mockWindow.scrollTo(0, 10);
    mockWindow.setSize(50, 50);
    chrome.test.assertEq(0.25, viewport.getZoom());
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(5, viewport.position.y);

    chrome.test.succeed();
  },

  function testFitToBoundingBox() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();
    documentDimensions.addPage(50, 50);
    documentDimensions.addPage(50, 100);
    documentDimensions.addPage(100, 50);
    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(200, 200);
    viewport.setDocumentDimensions(documentDimensions);

    function assertPositionAndZoom(
        expectedPosition: Point, expectedZoom: number) {
      chrome.test.assertEq(
          FittingType.FIT_TO_BOUNDING_BOX, viewport.fittingType);
      chrome.test.assertTrue(mockCallback.wasCalled);
      chrome.test.assertEq(expectedPosition, viewport.position);
      chrome.test.assertEq(expectedZoom, viewport.getZoom());
    }

    function testForVisibleBoundingBox(
        page: number, boundingBox: Rect, expectedX: number, expectedY: number,
        expectedZoom: number) {
      viewport.setZoom(0.1);
      mockCallback.reset();
      viewport.fitToBoundingBox({boundingBox, page});
      assertPositionAndZoom({x: expectedX, y: expectedY}, expectedZoom);
    }

    // Bounding box is smaller than window size and square.
    let boundingBox: Rect = {x: 25, y: 25, width: 50, height: 50};
    testForVisibleBoundingBox(0, boundingBox, 60, 56, 2);
    testForVisibleBoundingBox(1, boundingBox, 60, 156, 2);
    testForVisibleBoundingBox(2, boundingBox, 60, 356, 2);
    testForVisibleBoundingBox(3, boundingBox, 60, 456, 2);
    testForVisibleBoundingBox(4, boundingBox, 60, 656, 2);

    // Bounding box is smaller than window size with larger width.
    boundingBox = {x: 20, y: 25, width: 80, height: 50};
    testForVisibleBoundingBox(2, boundingBox, 31.25, 203.75, 1.25);
    testForVisibleBoundingBox(3, boundingBox, 31.25, 266.25, 1.25);
    testForVisibleBoundingBox(4, boundingBox, 31.25, 391.25, 1.25);

    // Bounding box is smaller than window size with larger height.
    boundingBox = {x: 25, y: 20, width: 50, height: 80};
    testForVisibleBoundingBox(1, boundingBox, 18.75, 91.25, 1.25);
    testForVisibleBoundingBox(3, boundingBox, 18.75, 278.75, 1.25);
    testForVisibleBoundingBox(4, boundingBox, 18.75, 403.75, 1.25);

    // Bounding box is the same size as window size.
    boundingBox = {x: 0, y: 0, width: 100, height: 100};
    testForVisibleBoundingBox(3, boundingBox, 5, 203, 1);
    testForVisibleBoundingBox(4, boundingBox, 5, 303, 1);

    // Bounding box is larger than window size.
    boundingBox = {x: 10, y: 20, width: 150, height: 150};
    testForVisibleBoundingBox(
        4, boundingBox, 10, 215.33333333333331, 0.6666666666666666);

    chrome.test.succeed();
  },

  function testFitToBoundingBoxDimensionWidth() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();
    documentDimensions.addPage(200, 200);
    documentDimensions.addPage(200, 200);
    viewport.setDocumentDimensions(documentDimensions);

    function assertPositionAndZoom(
        expectedPosition: Point, expectedZoom: number) {
      chrome.test.assertEq(
          FittingType.FIT_TO_BOUNDING_BOX_WIDTH, viewport.fittingType);
      chrome.test.assertTrue(mockCallback.wasCalled);
      chrome.test.assertEq(expectedPosition, viewport.position);
      chrome.test.assertEq(expectedZoom, viewport.getZoom());
    }

    function testForVisibleBoundingBoxWidth(
        boundingBox: Rect, page: number, viewPosition: number|undefined,
        expectedX: number, expectedY: number, expectedZoom: number) {
      viewport.setZoom(0.1);
      viewport.setPosition({
        x: 0,
        y: 0,
      });
      mockCallback.reset();
      viewport.fitToBoundingBoxDimension(
          {boundingBox, page, viewPosition, fitToWidth: true});
      assertPositionAndZoom({x: expectedX, y: expectedY}, expectedZoom);
    }

    // Test that the zoom matches the height of the bounding box and not the
    // width.

    // Bounding box is smaller than window size with larger height.
    let boundingBox = {x: 20, y: 25, width: 50, height: 80};
    testForVisibleBoundingBoxWidth(boundingBox, 0, undefined, 50, 6, 2);
    testForVisibleBoundingBoxWidth(boundingBox, 0, 20, 50, 46, 2);
    testForVisibleBoundingBoxWidth(boundingBox, 1, undefined, 50, 406, 2);
    testForVisibleBoundingBoxWidth(boundingBox, 1, 20, 50, 446, 2);

    // Bounding box is smaller than window size with larger width.
    boundingBox = {x: 25, y: 20, width: 80, height: 50};
    testForVisibleBoundingBoxWidth(boundingBox, 0, undefined, 37.5, 3.75, 1.25);
    testForVisibleBoundingBoxWidth(boundingBox, 0, 30, 37.5, 41.25, 1.25);
    testForVisibleBoundingBoxWidth(
        boundingBox, 1, undefined, 37.5, 253.75, 1.25);
    testForVisibleBoundingBoxWidth(boundingBox, 1, 30, 37.5, 291.25, 1.25);

    // Bounding box height is the same size as window size with larger height.
    boundingBox = {x: 0, y: 0, width: 100, height: 120};
    testForVisibleBoundingBoxWidth(boundingBox, 0, undefined, 5, 3, 1);
    testForVisibleBoundingBoxWidth(boundingBox, 0, 97, 5, 100, 1);
    testForVisibleBoundingBoxWidth(boundingBox, 1, undefined, 5, 203, 1);
    testForVisibleBoundingBoxWidth(boundingBox, 1, 97, 5, 300, 1);

    // Bounding box height is the same size as window size with larger width.
    boundingBox = {x: 0, y: 0, width: 100, height: 80};
    testForVisibleBoundingBoxWidth(boundingBox, 0, undefined, 5, 3, 1);
    testForVisibleBoundingBoxWidth(boundingBox, 0, 20, 5, 23, 1);
    testForVisibleBoundingBoxWidth(boundingBox, 1, undefined, 5, 203, 1);
    testForVisibleBoundingBoxWidth(boundingBox, 1, 20, 5, 223, 1);

    // Bounding box height is larger than window size with larger height.
    boundingBox = {x: 10, y: 20, width: 120, height: 150};
    testForVisibleBoundingBoxWidth(
        boundingBox, 0, undefined, 12.5, 2.5, 0.8333333333333334);
    testForVisibleBoundingBoxWidth(
        boundingBox, 0, 100, 12.5, 85.83333333333334, 0.8333333333333334);
    testForVisibleBoundingBoxWidth(
        boundingBox, 1, undefined, 12.5, 169.16666666666669,
        0.8333333333333334);
    testForVisibleBoundingBoxWidth(
        boundingBox, 1, 100, 12.5, 252.5, 0.8333333333333334);

    // Bounding box height is larger than window size with larger width.
    boundingBox = {x: 10, y: 20, width: 120, height: 20};
    testForVisibleBoundingBoxWidth(
        boundingBox, 0, undefined, 12.5, 2.5, 0.8333333333333334);
    testForVisibleBoundingBoxWidth(
        boundingBox, 0, 100, 12.5, 85.83333333333334, 0.8333333333333334);
    testForVisibleBoundingBoxWidth(
        boundingBox, 1, undefined, 12.5, 169.16666666666669,
        0.8333333333333334);
    testForVisibleBoundingBoxWidth(
        boundingBox, 1, 100, 12.5, 252.5, 0.8333333333333334);


    chrome.test.succeed();
  },

  function testFitToBoundingBoxDimensionHeight() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();
    documentDimensions.addPage(200, 200);
    documentDimensions.addPage(200, 200);
    viewport.setDocumentDimensions(documentDimensions);

    function assertPositionAndZoom(
        expectedPosition: Point, expectedZoom: number) {
      chrome.test.assertEq(
          FittingType.FIT_TO_BOUNDING_BOX_HEIGHT, viewport.fittingType);
      chrome.test.assertTrue(mockCallback.wasCalled);
      chrome.test.assertEq(expectedPosition, viewport.position);
      chrome.test.assertEq(expectedZoom, viewport.getZoom());
    }

    function testForVisibleBoundingBoxHeight(
        boundingBox: Rect, page: number, viewPosition: number|undefined,
        expectedX: number, expectedY: number, expectedZoom: number) {
      viewport.setZoom(0.1);
      viewport.setPosition({
        x: 0,
        y: 0,
      });
      mockCallback.reset();
      viewport.fitToBoundingBoxDimension(
          {boundingBox, page, viewPosition, fitToWidth: false});
      assertPositionAndZoom({x: expectedX, y: expectedY}, expectedZoom);
    }

    // Test that the zoom matches the height of the bounding box and not the
    // width.

    // Bounding box is smaller than window size with larger width.
    let boundingBox = {x: 20, y: 25, width: 80, height: 50};
    testForVisibleBoundingBoxHeight(boundingBox, 0, undefined, 10, 56, 2);
    testForVisibleBoundingBoxHeight(boundingBox, 0, 20, 50, 56, 2);
    testForVisibleBoundingBoxHeight(boundingBox, 1, undefined, 10, 456, 2);
    testForVisibleBoundingBoxHeight(boundingBox, 1, 20, 50, 456, 2);

    // Bounding box is smaller than window size with larger height.
    boundingBox = {x: 25, y: 20, width: 50, height: 80};
    testForVisibleBoundingBoxHeight(
        boundingBox, 0, undefined, 6.25, 28.75, 1.25);
    testForVisibleBoundingBoxHeight(boundingBox, 0, 30, 43.75, 28.75, 1.25);
    testForVisibleBoundingBoxHeight(
        boundingBox, 1, undefined, 6.25, 278.75, 1.25);
    testForVisibleBoundingBoxHeight(boundingBox, 1, 30, 43.75, 278.75, 1.25);

    // Bounding box height is the same size as window size with larger width.
    boundingBox = {x: 0, y: 0, width: 120, height: 100};
    testForVisibleBoundingBoxHeight(boundingBox, 0, undefined, 5, 3, 1);
    testForVisibleBoundingBoxHeight(boundingBox, 0, 95, 100, 3, 1);
    testForVisibleBoundingBoxHeight(boundingBox, 1, undefined, 5, 203, 1);
    testForVisibleBoundingBoxHeight(boundingBox, 1, 95, 100, 203, 1);

    // Bounding box height is the same size as window size with larger height.
    boundingBox = {x: 0, y: 0, width: 80, height: 100};
    testForVisibleBoundingBoxHeight(boundingBox, 0, undefined, 5, 3, 1);
    testForVisibleBoundingBoxHeight(boundingBox, 0, 20, 25, 3, 1);
    testForVisibleBoundingBoxHeight(boundingBox, 1, undefined, 5, 203, 1);
    testForVisibleBoundingBoxHeight(boundingBox, 1, 20, 25, 203, 1);

    // Bounding box height is larger than window size with larger width.
    boundingBox = {x: 10, y: 20, width: 200, height: 150};
    testForVisibleBoundingBoxHeight(
        boundingBox, 0, undefined, 3.333333333333333, 15.333333333333332,
        0.6666666666666666);
    testForVisibleBoundingBoxHeight(
        boundingBox, 0, 100, 70, 15.333333333333332, 0.6666666666666666);
    testForVisibleBoundingBoxHeight(
        boundingBox, 1, undefined, 3.333333333333333, 148.66666666666666,
        0.6666666666666666);
    testForVisibleBoundingBoxHeight(
        boundingBox, 1, 100, 70, 148.66666666666666, 0.6666666666666666);

    // Bounding box height is larger than window size with larger height.
    boundingBox = {x: 10, y: 20, width: 100, height: 150};
    testForVisibleBoundingBoxHeight(
        boundingBox, 0, undefined, 3.333333333333333, 15.333333333333332,
        0.6666666666666666);
    testForVisibleBoundingBoxHeight(
        boundingBox, 0, 100, 70, 15.333333333333332, 0.6666666666666666);
    testForVisibleBoundingBoxHeight(
        boundingBox, 1, undefined, 3.333333333333333, 148.66666666666666,
        0.6666666666666666);
    testForVisibleBoundingBoxHeight(
        boundingBox, 1, 100, 70, 148.66666666666666, 0.6666666666666666);

    chrome.test.succeed();
  },

  async function testPinchZoomInWithGestureEvent() {
    const mockWindow = new MockElement(100, 100, null);
    const viewport = getZoomableViewport(mockWindow, new MockSizer(), 0, 1);
    const documentDimensions = new MockDocumentDimensions();
    documentDimensions.addPage(200, 300);
    viewport.setDocumentDimensions(documentDimensions);
    setPluginPosition(10, 20);

    viewport.setZoom(1.2);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    // Pinch-zoom using gesture events.
    const pinchCenter = {x: 25, y: 50};
    const scaleChange = 1.25;
    const gestureEventTarget =
        viewport.getGestureDetectorForTesting().getEventTarget();
    gestureEventTarget.dispatchEvent(new CustomEvent('pinchstart', {
      detail: {
        center: pinchCenter,
      },
    }));
    gestureEventTarget.dispatchEvent(new CustomEvent('pinchupdate', {
      detail: {
        scaleRatio: scaleChange,
        direction: 'in',
        startScaleRatio: scaleChange,
        center: pinchCenter,
      },
    }));
    gestureEventTarget.dispatchEvent(new CustomEvent('pinchend', {
      detail: {
        startScaleRatio: scaleChange,
        center: pinchCenter,
      },
    }));

    // Pinch updates are throttled by rAF, so we schedule the rest of the test
    // after the pinch takes effect.
    await whenRequestAnimationFrame();
    assertRoughlyEquals(1.5, viewport.getZoom(), 0.001);
    assertRoughlyEquals(6.25, viewport.position.x, 0.001);
    assertRoughlyEquals(12.50, viewport.position.y, 0.001);

    chrome.test.succeed();
  },

  async function testPageNavigationWithDispatchSwipe() {
    const mockWindow = new MockElement(100, 100, null);
    const viewport = getZoomableViewport(mockWindow, new MockSizer(), 0, 1);
    const documentDimensions = new MockDocumentDimensions();

    // Add 2 pages to the document.
    documentDimensions.addPage(200, 300);
    documentDimensions.addPage(200, 300);
    viewport.setDocumentDimensions(documentDimensions);
    chrome.test.assertEq(0, viewport.getMostVisiblePage());

    // When fullscreen is not enabled, swiping doesn't turn the pages.
    viewport.dispatchSwipe(SwipeDirection.RIGHT_TO_LEFT);
    await whenRequestAnimationFrame();
    chrome.test.assertEq(0, viewport.getMostVisiblePage());

    // Turn on fullscreen (Presentation) mode.
    viewport.enableFullscreenForTesting();

    // Test swiping in RTL.
    {
      document.documentElement.dir = 'rtl';
      // Swiping from left to right navigates to the next page.
      viewport.dispatchSwipe(SwipeDirection.LEFT_TO_RIGHT);
      await whenRequestAnimationFrame();
      chrome.test.assertEq(1, viewport.getMostVisiblePage());

      // Swiping from right to left navigates to the previous page.
      viewport.dispatchSwipe(SwipeDirection.RIGHT_TO_LEFT);
      await whenRequestAnimationFrame();
      chrome.test.assertEq(0, viewport.getMostVisiblePage());
    }

    // Test swiping in LTR.
    {
      // Note: Make sure text direction is reset to LTR before finishing this
      // test or it's going to affect other tests in this test suite.
      document.documentElement.dir = 'ltr';

      // Swiping from right to left navigates to the next page.
      viewport.dispatchSwipe(SwipeDirection.RIGHT_TO_LEFT);
      await whenRequestAnimationFrame();
      chrome.test.assertEq(1, viewport.getMostVisiblePage());

      // Swiping from left to right navigates to the previous page.
      viewport.dispatchSwipe(SwipeDirection.LEFT_TO_RIGHT);
      await whenRequestAnimationFrame();
      chrome.test.assertEq(0, viewport.getMostVisiblePage());
    }

    chrome.test.succeed();
  },

  async function testPinchZoomInWithDispatchGesture() {
    const mockWindow = new MockElement(100, 100, null);
    const viewport = getZoomableViewport(mockWindow, new MockSizer(), 0, 1);
    const documentDimensions = new MockDocumentDimensions();
    documentDimensions.addPage(200, 300);
    viewport.setDocumentDimensions(documentDimensions);
    setPluginPosition(10, 20);

    viewport.setZoom(1.2);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    // Pinch-zoom using dispatchGesture().
    const pinchCenter = {x: 25, y: 50};
    const scaleChange = 1.25;
    viewport.dispatchGesture({
      type: 'pinchstart',
      detail: {
        center: pinchCenter,
      },
    });
    viewport.dispatchGesture({
      type: 'pinchupdate',
      detail: {
        scaleRatio: scaleChange,
        direction: 'in',
        startScaleRatio: scaleChange,
        center: pinchCenter,
      },
    });
    viewport.dispatchGesture({
      type: 'pinchend',
      detail: {
        startScaleRatio: scaleChange,
        center: pinchCenter,
      },
    });

    // Pinch updates are throttled by rAF, so we schedule the rest of the test
    // after the pinch takes effect.
    await whenRequestAnimationFrame();
    assertRoughlyEquals(1.5, viewport.getZoom(), 0.001);
    assertRoughlyEquals(6.25, viewport.position.x, 0.001);
    assertRoughlyEquals(12.50, viewport.position.y, 0.001);

    chrome.test.succeed();
  },

  // Regression test for https://crbug.com/1123976
  async function testPinchZoomingUnsetsPageFitting() {
    const mockWindow = new MockElement(100, 100, null);
    const viewport = getZoomableViewport(mockWindow, new MockSizer(), 0, 1);
    const documentDimensions = new MockDocumentDimensions();
    documentDimensions.addPage(50, 100);
    viewport.setDocumentDimensions(documentDimensions);

    viewport.fitToWidth();
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, viewport.fittingType);
    chrome.test.assertEq(2, viewport.getZoom());

    // Pinch-zoom using gesture events.
    const pinchCenter = {x: 25, y: 25};
    const scaleChange = 0.5;
    const gestureEventTarget =
        viewport.getGestureDetectorForTesting().getEventTarget();
    gestureEventTarget.dispatchEvent(new CustomEvent('pinchstart', {
      detail: {
        center: pinchCenter,
      },
    }));
    gestureEventTarget.dispatchEvent(new CustomEvent('pinchupdate', {
      detail: {
        scaleRatio: scaleChange,
        direction: 'out',
        startScaleRatio: scaleChange,
        center: pinchCenter,
      },
    }));
    gestureEventTarget.dispatchEvent(new CustomEvent('pinchend', {
      detail: {
        startScaleRatio: scaleChange,
        center: pinchCenter,
      },
    }));

    // Pinch updates are throttled by rAF, so we schedule the rest of the test
    // after the pinch takes effect.
    await whenRequestAnimationFrame();
    chrome.test.assertEq(1, viewport.getZoom());

    // Changing the zoom using a pinch should unset the page fitting as it would
    // with other zooming mechanisms.
    chrome.test.assertEq(FittingType.NONE, viewport.fittingType);
    // A subsequent window resize should not cause a zoom change.
    mockWindow.setSize(101, 100);
    chrome.test.assertEq(1, viewport.getZoom());

    chrome.test.succeed();
  },

  function testGoToNextPage() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();

    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(200, 200);
    documentDimensions.addPage(100, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    // Start at the first page.
    viewport.goToPage(0);
    chrome.test.assertEq(0, viewport.position.y);

    // Go from first page to second.
    mockCallback.reset();
    viewport.goToNextPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(100, viewport.position.y);

    // Go from second page to third at 0.5x zoom.
    viewport.setZoom(0.5);
    mockCallback.reset();
    viewport.goToNextPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(150, viewport.position.y);

    // Try to go to page after third.
    mockCallback.reset();
    viewport.goToNextPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(150, viewport.position.y);
    chrome.test.succeed();
  },

  function testGoToNextPageInTwoUpView() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);

    const documentDimensions = new MockDocumentDimensions(
        800, 750,
        {direction: 0, defaultPageOrientation: 0, twoUpViewEnabled: true});
    documentDimensions.addPageForTwoUpView(200, 0, 200, 150);
    documentDimensions.addPageForTwoUpView(400, 0, 400, 200);
    documentDimensions.addPageForTwoUpView(100, 200, 300, 250);
    documentDimensions.addPageForTwoUpView(400, 200, 250, 225);
    documentDimensions.addPageForTwoUpView(150, 450, 250, 300);
    documentDimensions.addPageForTwoUpView(400, 450, 340, 200);
    documentDimensions.addPageForTwoUpView(100, 750, 300, 600);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    // Start at the first page.
    viewport.goToPage(0);
    chrome.test.assertEq(0, viewport.position.y);

    // Go from first page to third.
    mockCallback.reset();
    viewport.goToNextPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(100, viewport.position.x);
    chrome.test.assertEq(200, viewport.position.y);

    // Go from third page to fifth.
    mockCallback.reset();
    viewport.goToNextPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(150, viewport.position.x);
    chrome.test.assertEq(450, viewport.position.y);

    // Go from fifth page to seventh at 0.5x zoom.
    viewport.setZoom(0.5);
    mockCallback.reset();
    viewport.goToNextPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(50, viewport.position.x);
    chrome.test.assertEq(375, viewport.position.y);

    // Try going to page after seventh.
    mockCallback.reset();
    viewport.goToNextPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(50, viewport.position.x);
    chrome.test.assertEq(375, viewport.position.y);

    // Test behavior for right page.
    viewport.goToPage(1);
    viewport.setZoom(1);
    mockCallback.reset();
    viewport.goToNextPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(100, viewport.position.x);
    chrome.test.assertEq(200, viewport.position.y);
    chrome.test.succeed();
  },

  function testGoToPreviousPage() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();

    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(200, 200);
    documentDimensions.addPage(100, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    // Start at the third page.
    viewport.goToPage(2);
    chrome.test.assertEq(300, viewport.position.y);

    // Go from third page to second.
    mockCallback.reset();
    viewport.goToPreviousPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(100, viewport.position.y);

    // Go from second page to first at 0.5x zoom.
    viewport.setZoom(0.5);
    mockCallback.reset();
    viewport.goToPreviousPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    // Try going to page before first.
    mockCallback.reset();
    viewport.goToPreviousPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);
    chrome.test.succeed();
  },

  function testGoToPreviousPageInTwoUpView() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);

    const documentDimensions = new MockDocumentDimensions(
        800, 750,
        {direction: 0, defaultPageOrientation: 0, twoUpViewEnabled: true});
    documentDimensions.addPageForTwoUpView(200, 0, 200, 150);
    documentDimensions.addPageForTwoUpView(400, 0, 400, 200);
    documentDimensions.addPageForTwoUpView(100, 200, 300, 250);
    documentDimensions.addPageForTwoUpView(400, 200, 250, 225);
    documentDimensions.addPageForTwoUpView(150, 450, 250, 300);
    documentDimensions.addPageForTwoUpView(400, 450, 340, 200);
    documentDimensions.addPageForTwoUpView(100, 750, 300, 600);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    // Start at the seventh page.
    viewport.goToPage(6);
    chrome.test.assertEq(750, viewport.position.y);

    // Go from seventh page to fifth.
    mockCallback.reset();
    viewport.goToPreviousPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(150, viewport.position.x);
    chrome.test.assertEq(450, viewport.position.y);

    // Go from fifth page to third.
    mockCallback.reset();
    viewport.goToPreviousPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(100, viewport.position.x);
    chrome.test.assertEq(200, viewport.position.y);

    // Go from third page to first at 0.5x zoom.
    viewport.setZoom(0.5);
    mockCallback.reset();
    viewport.goToPreviousPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(100, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    // Try going to page before first.
    mockCallback.reset();
    viewport.goToPreviousPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(100, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);
    chrome.test.succeed();

    // Test behavior for right page.
    viewport.goToPage(3);
    viewport.setZoom(1);
    mockCallback.reset();
    viewport.goToPreviousPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(200, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);
    chrome.test.succeed();
  },

  function testGoToPage() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();

    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(200, 200);
    documentDimensions.addPage(100, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    mockCallback.reset();
    viewport.goToPage(0);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    mockCallback.reset();
    viewport.goToPage(1);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(100, viewport.position.y);

    mockCallback.reset();
    viewport.goToPage(2);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(300, viewport.position.y);

    viewport.setZoom(0.5);
    mockCallback.reset();
    viewport.goToPage(2);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(150, viewport.position.y);
    chrome.test.succeed();
  },

  function testGoToPageAndXY() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();

    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(200, 200);
    documentDimensions.addPage(100, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    mockCallback.reset();
    viewport.goToPageAndXy(0, 0, 0);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    mockCallback.reset();
    viewport.goToPageAndXy(1, 0, 0);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(100, viewport.position.y);

    mockCallback.reset();
    viewport.goToPageAndXy(2, 42, 46);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0 + 42, viewport.position.x);
    chrome.test.assertEq(300 + 46, viewport.position.y);

    mockCallback.reset();
    viewport.goToPageAndXy(2, 42, 0);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0 + 42, viewport.position.x);
    chrome.test.assertEq(300, viewport.position.y);

    mockCallback.reset();
    viewport.goToPageAndXy(2, 0, 46);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(300 + 46, viewport.position.y);

    viewport.setZoom(0.5);
    mockCallback.reset();
    viewport.goToPageAndXy(2, 42, 46);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0 + 21, viewport.position.x);
    chrome.test.assertEq(150 + 23, viewport.position.y);
    chrome.test.succeed();
  },

  function testScrollTo() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();

    documentDimensions.addPage(200, 200);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    mockCallback.reset();
    viewport.scrollTo({x: 0, y: 0});
    chrome.test.assertFalse(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    mockCallback.reset();
    viewport.scrollTo({x: 10, y: 20});
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(10, viewport.position.x);
    chrome.test.assertEq(20, viewport.position.y);

    mockCallback.reset();
    viewport.scrollTo({y: 30});
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(10, viewport.position.x);
    chrome.test.assertEq(30, viewport.position.y);

    mockCallback.reset();
    viewport.scrollTo({y: 30});
    chrome.test.assertFalse(mockCallback.wasCalled);
    chrome.test.assertEq(10, viewport.position.x);
    chrome.test.assertEq(30, viewport.position.y);

    mockCallback.reset();
    viewport.scrollTo({x: 40});
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(40, viewport.position.x);
    chrome.test.assertEq(30, viewport.position.y);

    mockCallback.reset();
    viewport.scrollTo({});
    chrome.test.assertFalse(mockCallback.wasCalled);
    chrome.test.assertEq(40, viewport.position.x);
    chrome.test.assertEq(30, viewport.position.y);

    chrome.test.succeed();
  },

  function testScrollBy() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();

    documentDimensions.addPage(200, 200);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    mockCallback.reset();
    viewport.scrollBy({x: 10, y: 20});
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(10, viewport.position.x);
    chrome.test.assertEq(20, viewport.position.y);

    mockCallback.reset();
    viewport.scrollBy({x: 10, y: 20});
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(20, viewport.position.x);
    chrome.test.assertEq(40, viewport.position.y);

    mockCallback.reset();
    viewport.scrollBy({x: -5, y: 0});
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(15, viewport.position.x);
    chrome.test.assertEq(40, viewport.position.y);

    mockCallback.reset();
    viewport.scrollBy({x: 0, y: 60});
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(15, viewport.position.x);
    chrome.test.assertEq(100, viewport.position.y);

    mockCallback.reset();
    viewport.scrollBy({x: 0, y: 0});
    chrome.test.assertFalse(mockCallback.wasCalled);
    chrome.test.assertEq(15, viewport.position.x);
    chrome.test.assertEq(100, viewport.position.y);

    chrome.test.succeed();
  },

  function testGetPageScreenRect() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);
    const documentDimensions = new MockDocumentDimensions();
    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(200, 200);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    // Test that the rect of the first page is positioned/sized correctly.
    mockWindow.scrollTo(0, 0);
    let rect1 = viewport.getPageScreenRect(0);
    chrome.test.assertEq(PAGE_SHADOW.left + 100 / 2, rect1.x);
    chrome.test.assertEq(PAGE_SHADOW.top, rect1.y);
    chrome.test.assertEq(
        100 - PAGE_SHADOW.right - PAGE_SHADOW.left, rect1.width);
    chrome.test.assertEq(
        100 - PAGE_SHADOW.bottom - PAGE_SHADOW.top, rect1.height);

    // Check that when we scroll, the rect of the first page is updated
    // correctly.
    mockWindow.scrollTo(100, 10);
    const rect2 = viewport.getPageScreenRect(0);
    chrome.test.assertEq(rect1.x - 100, rect2.x);
    chrome.test.assertEq(rect1.y - 10, rect2.y);
    chrome.test.assertEq(rect1.width, rect2.width);
    chrome.test.assertEq(rect1.height, rect2.height);

    // Check the rect of the second page is positioned/sized correctly.
    mockWindow.scrollTo(0, 100);
    rect1 = viewport.getPageScreenRect(1);
    chrome.test.assertEq(PAGE_SHADOW.left, rect1.x);
    chrome.test.assertEq(PAGE_SHADOW.top, rect1.y);
    chrome.test.assertEq(
        200 - PAGE_SHADOW.right - PAGE_SHADOW.left, rect1.width);
    chrome.test.assertEq(
        200 - PAGE_SHADOW.bottom - PAGE_SHADOW.top, rect1.height);
    chrome.test.succeed();
  },

  function testBeforeZoomAfterZoom() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);

    let afterZoomCalled = false;
    let beforeZoomCalled = false;
    const afterZoom = function() {
      afterZoomCalled = true;
      chrome.test.assertTrue(beforeZoomCalled);
      chrome.test.assertEq(0.5, viewport.getZoom());
    };
    const beforeZoom = function() {
      beforeZoomCalled = true;
      chrome.test.assertFalse(afterZoomCalled);
      chrome.test.assertEq(1, viewport.getZoom());
    };

    viewport.setBeforeZoomCallback(beforeZoom);
    viewport.setAfterZoomCallback(afterZoom);
    viewport.setZoom(0.5);
    chrome.test.succeed();
  },

  function testInitialSetDocumentDimensionsZoomConstrained() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1.2);
    viewport.setDocumentDimensions(new MockDocumentDimensions(50, 50));
    chrome.test.assertEq(1.2, viewport.getZoom());
    chrome.test.succeed();
  },

  function testInitialSetDocumentDimensionsZoomUnconstrained() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 3);
    viewport.setDocumentDimensions(new MockDocumentDimensions(50, 50));
    chrome.test.assertEq(2, viewport.getZoom());
    chrome.test.succeed();
  },

  function testLayoutOptions() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);

    chrome.test.assertEq(undefined, viewport.getLayoutOptions());

    viewport.setDocumentDimensions(new MockDocumentDimensions(
        50, 50,
        {direction: 1, defaultPageOrientation: 1, twoUpViewEnabled: true}));
    chrome.test.assertEq(
        {direction: 2, defaultPageOrientation: 1, twoUpViewEnabled: true},
        viewport.getLayoutOptions());

    viewport.setDocumentDimensions(new MockDocumentDimensions(50, 50));
    chrome.test.assertEq(undefined, viewport.getLayoutOptions());

    chrome.test.succeed();
  },

  function testSetContentShowLocalSizer() {
    const mockSizer = new MockSizer();
    const viewport =
        getZoomableViewport(new MockElement(100, 100, null), mockSizer, 0, 1);
    viewport.setRemoteContent(createMockPdfPluginForTest());

    const dummyPlugin = document.body.querySelector('#plugin');
    viewport.setContent(dummyPlugin);

    chrome.test.assertEq('block', mockSizer.style.display);
    chrome.test.succeed();
  },

  function testSetContentSizeToLocal() {
    const mockSizer = new MockSizer();
    const viewport =
        getZoomableViewport(new MockElement(100, 100, null), mockSizer, 0, 1);
    viewport.setRemoteContent(createMockPdfPluginForTest());
    viewport.setDocumentDimensions(new MockDocumentDimensions(20, 30));
    chrome.test.assertEq('0px', mockSizer.style.width);
    chrome.test.assertEq('0px', mockSizer.style.height);

    const dummyPlugin = document.body.querySelector('#plugin');
    viewport.setContent(dummyPlugin);

    chrome.test.assertEq('20px', mockSizer.style.width);
    chrome.test.assertEq('30px', mockSizer.style.height);
    chrome.test.succeed();
  },

  function testSetContentScrollToLocal() {
    const mockWindow = new MockElement(100, 100, null);
    const viewport = getZoomableViewport(mockWindow, new MockSizer(), 0, 1);
    viewport.setRemoteContent(createMockPdfPluginForTest());
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 200));
    viewport.setZoom(1);
    viewport.setPosition({x: 20, y: 30});
    chrome.test.assertEq(0, mockWindow.scrollLeft);
    chrome.test.assertEq(0, mockWindow.scrollTop);

    const dummyPlugin = document.body.querySelector('#plugin');
    viewport.setContent(dummyPlugin);

    chrome.test.assertEq(20, mockWindow.scrollLeft);
    chrome.test.assertEq(30, mockWindow.scrollTop);
    chrome.test.succeed();
  },

  function testSetRemoteContentAttachContent() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);

    const mockPlugin = createMockPdfPluginForTest();
    viewport.setRemoteContent(mockPlugin);

    const dummyContent = document.body.querySelector('div');
    chrome.test.assertEq(dummyContent, mockPlugin.parentNode);
    chrome.test.succeed();
  },

  function testSetRemoteContentHideLocalSizer() {
    const mockSizer = new MockSizer();
    const viewport =
        getZoomableViewport(new MockElement(100, 100, null), mockSizer, 0, 1);

    viewport.setRemoteContent(createMockPdfPluginForTest());

    chrome.test.assertEq('none', mockSizer.style.display);
    chrome.test.succeed();
  },

  function testSetRemoteContentSizeToRemote() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);
    viewport.setDocumentDimensions(new MockDocumentDimensions(20, 30));

    const mockPlugin = createMockPdfPluginForTest();
    viewport.setRemoteContent(mockPlugin);

    const {width, height} = mockPlugin.findMessage('updateSize');
    chrome.test.assertEq(20, width);
    chrome.test.assertEq(30, height);
    chrome.test.succeed();
  },

  function testSetRemoteContentScrollToRemote() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 200));
    viewport.setZoom(1);
    viewport.setPosition({x: 20, y: 30});

    const mockPlugin = createMockPdfPluginForTest();
    viewport.setRemoteContent(mockPlugin);

    const {x, y} = mockPlugin.findMessage('syncScrollToRemote');
    chrome.test.assertEq(20, x);
    chrome.test.assertEq(30, y);
    chrome.test.succeed();
  },

  function testSetDocumentDimensionsRemote() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);
    const mockPlugin = createMockPdfPluginForTest();
    viewport.setRemoteContent(mockPlugin);
    mockPlugin.clearMessages();

    viewport.setDocumentDimensions(new MockDocumentDimensions(20, 30));

    const {width, height} = mockPlugin.findMessage('updateSize');
    chrome.test.assertEq(20, width);
    chrome.test.assertEq(20, viewport.contentSize.width);
    chrome.test.assertEq(30, height);
    chrome.test.assertEq(30, viewport.contentSize.height);
    chrome.test.succeed();
  },

  function testSetPositionRemote() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);
    const mockPlugin = createMockPdfPluginForTest();
    viewport.setRemoteContent(mockPlugin);
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 200));
    viewport.setZoom(1);
    mockPlugin.clearMessages();

    viewport.setPosition({x: 20, y: 30});

    const {x, y} = mockPlugin.findMessage('syncScrollToRemote');
    chrome.test.assertEq(20, x);
    chrome.test.assertEq(20, viewport.position.x);
    chrome.test.assertEq(30, y);
    chrome.test.assertEq(30, viewport.position.y);
    chrome.test.succeed();
  },

  function testSetPositionRemoteModifiedByAck() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);
    const mockPlugin = createMockPdfPluginForTest();
    viewport.setRemoteContent(mockPlugin);
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 200));
    viewport.setZoom(1);
    ackAllScrollToRemoteMessages(viewport, mockPlugin);

    const scrollCounter = new ScrollEventCounter();
    viewport.setPosition({x: 20, y: 30});
    viewport.ackScrollToRemote({x: 10, y: 50});

    chrome.test.assertEq(1, scrollCounter.count);
    chrome.test.assertEq(10, viewport.position.x);
    chrome.test.assertEq(50, viewport.position.y);
    chrome.test.succeed();
  },

  function testSetPositionRemoteModifiedByAckIgnoreOverlapping() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);
    const mockPlugin = createMockPdfPluginForTest();
    viewport.setRemoteContent(mockPlugin);
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 200));
    viewport.setZoom(1);
    ackAllScrollToRemoteMessages(viewport, mockPlugin);

    const scrollCounter = new ScrollEventCounter();
    viewport.setPosition({x: 20, y: 30});
    viewport.setPosition({x: 30, y: 40});
    viewport.ackScrollToRemote({x: 10, y: 50});

    chrome.test.assertEq(1, scrollCounter.count);
    chrome.test.assertEq(30, viewport.position.x);
    chrome.test.assertEq(40, viewport.position.y);
    chrome.test.succeed();
  },

  function testSetPositionRemoteModifiedByAckMultiple() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);
    const mockPlugin = createMockPdfPluginForTest();
    viewport.setRemoteContent(mockPlugin);
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 200));
    viewport.setZoom(1);
    ackAllScrollToRemoteMessages(viewport, mockPlugin);

    const scrollCounter = new ScrollEventCounter();
    viewport.setPosition({x: 20, y: 30});
    viewport.setPosition({x: 30, y: 40});
    viewport.ackScrollToRemote({x: 10, y: 50});
    viewport.ackScrollToRemote({x: 10, y: 60});

    chrome.test.assertEq(2, scrollCounter.count);
    chrome.test.assertEq(10, viewport.position.x);
    chrome.test.assertEq(60, viewport.position.y);
    chrome.test.succeed();
  },

  function testSetPositionRemoteNaN() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);
    viewport.setRemoteContent(createMockPdfPluginForTest());

    viewport.setPosition({x: NaN, y: NaN});

    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);
    chrome.test.succeed();
  },

  function testSetPositionRemoteUnderflowLeftAndTop() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), SCROLLBAR_WIDTH, 1);
    viewport.setRemoteContent(createMockPdfPluginForTest());
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 200));
    viewport.setZoom(1);

    viewport.setPosition({x: -1, y: -1});

    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);
    chrome.test.succeed();
  },

  function testSetPositionRemoteUnderflowRightAndTop() {
    const mockWindow = new MockElement(100, 100, null);
    mockWindow.dir = 'rtl';
    const viewport =
        getZoomableViewport(mockWindow, new MockSizer(), SCROLLBAR_WIDTH, 1);
    viewport.setRemoteContent(createMockPdfPluginForTest());
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 200));
    viewport.setZoom(1);

    viewport.setPosition({x: 1, y: -1});

    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);
    chrome.test.succeed();
  },

  function testSetPositionRemoteOverflowRightAndBottom() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), SCROLLBAR_WIDTH, 1);
    viewport.setRemoteContent(createMockPdfPluginForTest());
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 300));
    viewport.setZoom(1);

    viewport.setPosition({x: 116, y: 216});

    chrome.test.assertEq(115, viewport.position.x);
    chrome.test.assertEq(215, viewport.position.y);
    chrome.test.succeed();
  },

  function testSetPositionRemoteOverflowLeftAndBottom() {
    const mockWindow = new MockElement(100, 100, null);
    mockWindow.dir = 'rtl';
    const viewport =
        getZoomableViewport(mockWindow, new MockSizer(), SCROLLBAR_WIDTH, 1);
    viewport.setRemoteContent(createMockPdfPluginForTest());
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 300));
    viewport.setZoom(1);

    viewport.setPosition({x: -116, y: 216});

    chrome.test.assertEq(-115, viewport.position.x);
    chrome.test.assertEq(215, viewport.position.y);
    chrome.test.succeed();
  },

  function testSetPositionRemoteOverflowWithoutVerticalScrollbarRight() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), SCROLLBAR_WIDTH, 1);
    viewport.setRemoteContent(createMockPdfPluginForTest());
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 85));
    viewport.setZoom(1);

    viewport.setPosition({x: 101, y: 1});

    chrome.test.assertEq(100, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);
    chrome.test.succeed();
  },

  function testSetPositionRemoteOverflowWithoutVerticalScrollbarLeft() {
    const mockWindow = new MockElement(100, 100, null);
    mockWindow.dir = 'rtl';
    const viewport =
        getZoomableViewport(mockWindow, new MockSizer(), SCROLLBAR_WIDTH, 1);
    viewport.setRemoteContent(createMockPdfPluginForTest());
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 85));
    viewport.setZoom(1);

    viewport.setPosition({x: -101, y: 1});

    chrome.test.assertEq(-100, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);
    chrome.test.succeed();
  },

  function testSetPositionRemoteOverflowWithoutHorizontalScrollbarBottom() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), SCROLLBAR_WIDTH, 1);
    viewport.setRemoteContent(createMockPdfPluginForTest());
    viewport.setDocumentDimensions(new MockDocumentDimensions(85, 300));
    viewport.setZoom(1);

    viewport.setPosition({x: 1, y: 201});

    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(200, viewport.position.y);
    chrome.test.succeed();
  },

  function testSyncScrollFromRemote() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);
    const mockPlugin = createMockPdfPluginForTest();
    viewport.setRemoteContent(mockPlugin);
    ackAllScrollToRemoteMessages(viewport, mockPlugin);

    const scrollCounter = new ScrollEventCounter();
    viewport.syncScrollFromRemote({x: 30, y: 20});

    chrome.test.assertEq(1, scrollCounter.count);
    chrome.test.assertEq(30, viewport.position.x);
    chrome.test.assertEq(20, viewport.position.y);
    chrome.test.succeed();
  },

  function testSyncScrollFromRemoteDuplicateScroll() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);
    const mockPlugin = createMockPdfPluginForTest();
    viewport.setRemoteContent(mockPlugin);
    ackAllScrollToRemoteMessages(viewport, mockPlugin);
    viewport.syncScrollFromRemote({x: 30, y: 20});

    const scrollCounter = new ScrollEventCounter();
    viewport.syncScrollFromRemote({x: 30, y: 20});

    chrome.test.assertEq(0, scrollCounter.count);
    chrome.test.assertEq(30, viewport.position.x);
    chrome.test.assertEq(20, viewport.position.y);
    chrome.test.succeed();
  },

  function testSyncScrollFromRemoteScrollToRemoteUnacked() {
    const viewport = getZoomableViewport(
        new MockElement(100, 100, null), new MockSizer(), 0, 1);
    const mockPlugin = createMockPdfPluginForTest();
    viewport.setRemoteContent(mockPlugin);
    chrome.test.assertTrue(!!mockPlugin.findMessage('syncScrollToRemote'));

    const scrollCounter = new ScrollEventCounter();
    viewport.syncScrollFromRemote({x: 30, y: 20});

    chrome.test.assertEq(0, scrollCounter.count);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);
    chrome.test.succeed();
  },

  // TODO(crbug.com/40262954): Currently, fit types 'FIT_TO_PAGE',
  // 'FIT_TO_WIDTH', 'FIT_TO_HEIGHT', and 'FIT_TO_BOUNDING_BOX` do not correctly
  // navigate to a destination with the correct position and zoom level. Add
  // checks for position and zoom level for these fit types once fully
  // supported.
];

chrome.test.runTests(tests);
