// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NavigatorDelegate} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {OpenPdfParamsParser, PdfNavigator, WindowOpenDisposition} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {getZoomableViewport, MockDocumentDimensions, MockElement, MockSizer, MockViewportChangedCallback} from './test_util.js';

// URL allowed local file access.
const ALLOWED_URL: string = 'https://test-allowed-domain.com/document.pdf';

class MockNavigatorDelegate extends TestBrowserProxy implements
    NavigatorDelegate {
  constructor() {
    super([
      'navigateInCurrentTab',
      'navigateInNewTab',
      'navigateInNewWindow',
      'isAllowedLocalFileAccess',
    ]);
  }

  navigateInCurrentTab(url: string) {
    this.methodCalled('navigateInCurrentTab', url);
  }

  navigateInNewTab(url: string) {
    this.methodCalled('navigateInNewTab', url);
  }

  navigateInNewWindow(url: string) {
    this.methodCalled('navigateInNewWindow', url);
  }

  isAllowedLocalFileAccess(url: string): Promise<boolean> {
    return Promise.resolve(url === ALLOWED_URL);
  }
}

/**
 * Given a |navigator|, navigate to |url| in the current tab, a new tab, or
 * a new window depending on the value of |disposition|. Use
 * |viewportChangedCallback| and |navigatorDelegate| to check the callbacks,
 * and that the navigation to |expectedResultUrl| happened.
 */
async function doNavigationUrlTest(
    navigator: PdfNavigator, url: string, disposition: WindowOpenDisposition,
    expectedResultUrl: string|undefined,
    viewportChangedCallback: MockViewportChangedCallback,
    navigatorDelegate: MockNavigatorDelegate) {
  viewportChangedCallback.reset();
  navigatorDelegate.reset();
  await navigator.navigate(url, disposition);
  chrome.test.assertFalse(viewportChangedCallback.wasCalled);

  if (expectedResultUrl === undefined) {
    // Navigation shouldn't occur.
    switch (disposition) {
      case WindowOpenDisposition.CURRENT_TAB:
        chrome.test.assertEq(
            0, navigatorDelegate.getCallCount('navigateInCurrentTab'));
        break;
      case WindowOpenDisposition.NEW_BACKGROUND_TAB:
        chrome.test.assertEq(
            0, navigatorDelegate.getCallCount('navigateInNewTab'));
        break;
      case WindowOpenDisposition.NEW_WINDOW:
        chrome.test.assertEq(
            0, navigatorDelegate.getCallCount('navigateInNewWindow'));
        break;
      default:
        assertNotReached();
    }
    return;
  }

  let actualUrl = null;
  switch (disposition) {
    case WindowOpenDisposition.CURRENT_TAB:
      actualUrl = await navigatorDelegate.whenCalled('navigateInCurrentTab');
      break;
    case WindowOpenDisposition.NEW_BACKGROUND_TAB:
      actualUrl = await navigatorDelegate.whenCalled('navigateInNewTab');
      break;
    case WindowOpenDisposition.NEW_WINDOW:
      actualUrl = await navigatorDelegate.whenCalled('navigateInNewWindow');
      break;
    default:
      break;
  }

  chrome.test.assertEq(expectedResultUrl, actualUrl);
}

/**
 * Helper function to run doNavigationUrlTest() for the current tab, a new
 * tab, and a new window.
 */
async function doNavigationUrlTests(
    originalUrl: string, url: string, expectedResultUrl: string|undefined) {
  const mockWindow = new MockElement(100, 100, null);
  const mockSizer = new MockSizer();
  const mockViewportChangedCallback = new MockViewportChangedCallback();
  const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
  viewport.setViewportChangedCallback(mockViewportChangedCallback.callback);

  const getNamedDestinationCallback = function(_name: string) {
    return Promise.resolve(
        {messageId: 'getNamedDestination_1', pageNumber: -1});
  };
  const getPageBoundingBoxCallback = function(_page: number) {
    return Promise.resolve({x: -1, y: -1, width: -1, height: -1});
  };
  const paramsParser = new OpenPdfParamsParser(
      getNamedDestinationCallback, getPageBoundingBoxCallback);

  const navigatorDelegate = new MockNavigatorDelegate();
  const navigator =
      new PdfNavigator(originalUrl, viewport, paramsParser, navigatorDelegate);

  await doNavigationUrlTest(
      navigator, url, WindowOpenDisposition.CURRENT_TAB, expectedResultUrl,
      mockViewportChangedCallback, navigatorDelegate);
  await doNavigationUrlTest(
      navigator, url, WindowOpenDisposition.NEW_BACKGROUND_TAB,
      expectedResultUrl, mockViewportChangedCallback, navigatorDelegate);
  await doNavigationUrlTest(
      navigator, url, WindowOpenDisposition.NEW_WINDOW, expectedResultUrl,
      mockViewportChangedCallback, navigatorDelegate);
}

chrome.test.runTests([
  /**
   * Test navigation within the page, opening a url in the same tab and
   * opening a url in a new tab.
   */
  async function testNavigate() {
    const mockWindow = new MockElement(100, 100, null);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = getZoomableViewport(mockWindow, mockSizer, 0, 1);
    viewport.setViewportChangedCallback(mockCallback.callback);

    const getNamedDestinationCallback = function(destination: string) {
      if (destination === 'US') {
        return Promise.resolve(
            {messageId: 'getNamedDestination_1', pageNumber: 0});
      } else if (destination === 'UY') {
        return Promise.resolve(
            {messageId: 'getNamedDestination_2', pageNumber: 2});
      } else {
        return Promise.resolve(
            {messageId: 'getNamedDestination_3', pageNumber: -1});
      }
    };
    const getPageBoundingBoxCallback = function(_page: number) {
      return Promise.resolve({x: -1, y: -1, width: -1, height: -1});
    };
    const paramsParser = new OpenPdfParamsParser(
        getNamedDestinationCallback, getPageBoundingBoxCallback);
    const url = 'http://xyz.pdf';

    const navigatorDelegate = new MockNavigatorDelegate();
    const navigator =
        new PdfNavigator(url, viewport, paramsParser, navigatorDelegate);

    const documentDimensions = new MockDocumentDimensions();
    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(200, 200);
    documentDimensions.addPage(100, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    mockCallback.reset();
    // This should move viewport to page 0.
    await navigator.navigate(url + '#US', WindowOpenDisposition.CURRENT_TAB);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    mockCallback.reset();
    navigatorDelegate.reset();
    // This should open "http://xyz.pdf#US" in a new tab. So current tab
    // viewport should not update and viewport position should remain same.
    await navigator.navigate(
        url + '#US', WindowOpenDisposition.NEW_BACKGROUND_TAB);
    chrome.test.assertFalse(mockCallback.wasCalled);
    await navigatorDelegate.whenCalled('navigateInNewTab');
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    mockCallback.reset();
    // This should move viewport to page 2.
    await navigator.navigate(url + '#UY', WindowOpenDisposition.CURRENT_TAB);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(300, viewport.position.y);

    mockCallback.reset();
    navigatorDelegate.reset();
    // #ABC is not a named destination in the page so viewport should not
    // update, and the viewport position should remain same as testNavigate3's
    // navigating results, as this link will open in the same tab.
    await navigator.navigate(url + '#ABC', WindowOpenDisposition.CURRENT_TAB);
    chrome.test.assertFalse(mockCallback.wasCalled);
    await navigatorDelegate.whenCalled('navigateInCurrentTab');
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(300, viewport.position.y);
    chrome.test.succeed();
  },
  /**
   * Test opening a url in the same tab, in a new tab, and in a new window for
   * a http:// url as the current location. The destination url may not have
   * a valid scheme, so the navigator must determine the url by following
   * similar heuristics as Adobe Acrobat Reader.
   */
  async function testNavigateForLinksWithoutScheme() {
    const url = 'http://www.example.com/subdir/xyz.pdf';

    // Sanity check.
    await doNavigationUrlTests(
        url, 'https://www.foo.com/bar.pdf', 'https://www.foo.com/bar.pdf');

    // Open relative links.
    await doNavigationUrlTests(
        url, 'foo/bar.pdf', 'http://www.example.com/subdir/foo/bar.pdf');
    await doNavigationUrlTests(
        url, 'foo.com/bar.pdf',
        'http://www.example.com/subdir/foo.com/bar.pdf');
    await doNavigationUrlTests(
        url, '../www.foo.com/bar.pdf',
        'http://www.example.com/www.foo.com/bar.pdf');

    // Open an absolute link.
    await doNavigationUrlTests(
        url, '/foodotcom/bar.pdf', 'http://www.example.com/foodotcom/bar.pdf');

    // Open a http url without a scheme.
    await doNavigationUrlTests(
        url, 'www.foo.com/bar.pdf', 'http://www.foo.com/bar.pdf');

    // Test three dots.
    await doNavigationUrlTests(
        url, '.../bar.pdf', 'http://www.example.com/subdir/.../bar.pdf');

    // Test forward slashes.
    await doNavigationUrlTests(
        url, '..\\bar.pdf', 'http://www.example.com/bar.pdf');
    await doNavigationUrlTests(
        url, '.\\bar.pdf', 'http://www.example.com/subdir/bar.pdf');
    await doNavigationUrlTests(
        url, '\\bar.pdf', 'http://www.example.com/subdir//bar.pdf');

    // Regression test for https://crbug.com/569040
    await doNavigationUrlTests(
        url, 'http://something.else/foo#page=5',
        'http://something.else/foo#page=5');

    chrome.test.succeed();
  },
  /**
   * Test opening a url in the same tab, in a new tab, and in a new window with
   * a file:/// url as the current location.
   */
  async function testNavigateFromLocalFile() {
    const url = 'file:///some/path/to/myfile.pdf';

    // Open an absolute link.
    await doNavigationUrlTests(
        url, '/foodotcom/bar.pdf', 'file:///foodotcom/bar.pdf');

    chrome.test.succeed();
  },

  async function testNavigateDisallowedSchemes() {
    const url = 'https://example.com/some-web-document.pdf';

    // From non-file: to file:
    await doNavigationUrlTests(url, 'file:///bar.pdf', undefined);

    await doNavigationUrlTests(url, 'chrome://version', undefined);

    await doNavigationUrlTests(
        url, 'javascript://this-is-not-a-document.pdf', undefined);

    await doNavigationUrlTests(
        url, 'this-is-not-a-valid-scheme://path.pdf', undefined);

    await doNavigationUrlTests(url, '', undefined);

    chrome.test.succeed();
  },

  /**
   * Test domains and urls have access to file:/// urls when allowed.
   */
  async function testNavigateAllowedLocalFileAccess() {
    await doNavigationUrlTests(
        ALLOWED_URL, 'file:///bar.pdf', 'file:///bar.pdf');

    const disallowedUrl = 'https://test-disallowed-domain.com/document.pdf';

    await doNavigationUrlTests(disallowedUrl, 'file:///bar.pdf', undefined);

    chrome.test.succeed();
  },

]);
