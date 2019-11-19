// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PdfNavigator} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/navigator.js';
import {OpenPdfParamsParser} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/open_pdf_params_parser.js';
import {PDFScriptingAPI} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_scripting_api.js';
import {Viewport} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/viewport.js';

import {MockDocumentDimensions, MockSizer, MockViewportChangedCallback, MockWindow} from './test_util.js';

class MockNavigatorDelegate {
  constructor() {
    this.navigateInCurrentTabCalled = false;
    this.navigateInNewTabCalled = false;
    this.navigateInNewWindowCalled = false;
    this.url = undefined;
  }

  /** @param {?string} url */
  navigateInCurrentTab(url) {
    this.navigateInCurrentTabCalled = true;
    this.url = url || '<called, but no url set>';
  }

  /** @param {?string} url */
  navigateInNewTab(url) {
    this.navigateInNewTabCalled = true;
    this.url = url || '<called, but no url set>';
  }

  navigateInNewWindow(url) {
    this.navigateInNewWindowCalled = true;
    this.url = url || '<called, but no url set>';
  }

  reset() {
    this.navigateInCurrentTabCalled = false;
    this.navigateInNewTabCalled = false;
    this.navigateInNewWindowCalled = false;
    this.url = undefined;
  }
}

/**
 * Given a |navigator|, navigate to |url| in the current tab, a new tab, or
 * a new window depending on the value of |disposition|. Use
 * |viewportChangedCallback| and |navigatorDelegate| to check the callbacks,
 * and that the navigation to |expectedResultUrl| happened.
 */
function doNavigationUrlTest(
    navigator, url, disposition, expectedResultUrl, viewportChangedCallback,
    navigatorDelegate) {
  viewportChangedCallback.reset();
  navigatorDelegate.reset();
  navigator.navigate(url, disposition);
  chrome.test.assertFalse(viewportChangedCallback.wasCalled);
  chrome.test.assertEq(expectedResultUrl, navigatorDelegate.url);
  if (expectedResultUrl === undefined) {
    return;
  }
  switch (disposition) {
    case PdfNavigator.WindowOpenDisposition.CURRENT_TAB:
      chrome.test.assertTrue(navigatorDelegate.navigateInCurrentTabCalled);
      break;
    case PdfNavigator.WindowOpenDisposition.NEW_BACKGROUND_TAB:
      chrome.test.assertTrue(navigatorDelegate.navigateInNewTabCalled);
      break;
    case PdfNavigator.WindowOpenDisposition.NEW_WINDOW:
      chrome.test.assertTrue(navigatorDelegate.navigateInNewWindowCalled);
      break;
    default:
      break;
  }
}

/**
 * Helper function to run doNavigationUrlTest() for the current tab, a new
 * tab, and a new window.
 */
function doNavigationUrlTests(originalUrl, url, expectedResultUrl) {
  const mockWindow = new MockWindow(100, 100);
  const mockSizer = new MockSizer();
  const mockViewportChangedCallback = new MockViewportChangedCallback();
  const viewport = new Viewport(mockWindow, mockSizer, 0, 1, 0);
  viewport.setViewportChangedCallback(mockViewportChangedCallback.callback);

  const paramsParser = new OpenPdfParamsParser(function(name) {
    paramsParser.onNamedDestinationReceived(-1);
  });

  const navigatorDelegate = new MockNavigatorDelegate();
  const navigator =
      new PdfNavigator(originalUrl, viewport, paramsParser, navigatorDelegate);

  doNavigationUrlTest(
      navigator, url, PdfNavigator.WindowOpenDisposition.CURRENT_TAB,
      expectedResultUrl, mockViewportChangedCallback, navigatorDelegate);
  doNavigationUrlTest(
      navigator, url, PdfNavigator.WindowOpenDisposition.NEW_BACKGROUND_TAB,
      expectedResultUrl, mockViewportChangedCallback, navigatorDelegate);
  doNavigationUrlTest(
      navigator, url, PdfNavigator.WindowOpenDisposition.NEW_WINDOW,
      expectedResultUrl, mockViewportChangedCallback, navigatorDelegate);
}

const tests = [
  /**
   * Test navigation within the page, opening a url in the same tab and
   * opening a url in a new tab.
   */
  function testNavigate() {
    const mockWindow = new MockWindow(100, 100);
    const mockSizer = new MockSizer();
    const mockCallback = new MockViewportChangedCallback();
    const viewport = new Viewport(mockWindow, mockSizer, 0, 1, 0);
    viewport.setViewportChangedCallback(mockCallback.callback);

    const paramsParser = new OpenPdfParamsParser(function(destination) {
      if (destination == 'US') {
        paramsParser.onNamedDestinationReceived(0);
      } else if (destination == 'UY') {
        paramsParser.onNamedDestinationReceived(2);
      } else {
        paramsParser.onNamedDestinationReceived(-1);
      }
    });
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
    navigator.navigate(
        url + '#US', PdfNavigator.WindowOpenDisposition.CURRENT_TAB);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    mockCallback.reset();
    navigatorDelegate.reset();
    // This should open "http://xyz.pdf#US" in a new tab. So current tab
    // viewport should not update and viewport position should remain same.
    navigator.navigate(
        url + '#US', PdfNavigator.WindowOpenDisposition.NEW_BACKGROUND_TAB);
    chrome.test.assertFalse(mockCallback.wasCalled);
    chrome.test.assertTrue(navigatorDelegate.navigateInNewTabCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    mockCallback.reset();
    // This should move viewport to page 2.
    navigator.navigate(
        url + '#UY', PdfNavigator.WindowOpenDisposition.CURRENT_TAB);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(300, viewport.position.y);

    mockCallback.reset();
    navigatorDelegate.reset();
    // #ABC is not a named destination in the page so viewport should not
    // update and viewport position should remain same. As this link will open
    // in the same tab.
    navigator.navigate(
        url + '#ABC', PdfNavigator.WindowOpenDisposition.CURRENT_TAB);
    chrome.test.assertFalse(mockCallback.wasCalled);
    chrome.test.assertTrue(navigatorDelegate.navigateInCurrentTabCalled);
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
  function testNavigateForLinksWithoutScheme() {
    const url = 'http://www.example.com/subdir/xyz.pdf';

    // Sanity check.
    doNavigationUrlTests(
        url, 'https://www.foo.com/bar.pdf', 'https://www.foo.com/bar.pdf');

    // Open relative links.
    doNavigationUrlTests(
        url, 'foo/bar.pdf', 'http://www.example.com/subdir/foo/bar.pdf');
    doNavigationUrlTests(
        url, 'foo.com/bar.pdf',
        'http://www.example.com/subdir/foo.com/bar.pdf');
    doNavigationUrlTests(
        url, '../www.foo.com/bar.pdf',
        'http://www.example.com/www.foo.com/bar.pdf');

    // Open an absolute link.
    doNavigationUrlTests(
        url, '/foodotcom/bar.pdf', 'http://www.example.com/foodotcom/bar.pdf');

    // Open a http url without a scheme.
    doNavigationUrlTests(
        url, 'www.foo.com/bar.pdf', 'http://www.foo.com/bar.pdf');

    // Test three dots.
    doNavigationUrlTests(
        url, '.../bar.pdf', 'http://www.example.com/subdir/.../bar.pdf');

    // Test forward slashes.
    doNavigationUrlTests(url, '..\\bar.pdf', 'http://www.example.com/bar.pdf');
    doNavigationUrlTests(
        url, '.\\bar.pdf', 'http://www.example.com/subdir/bar.pdf');
    doNavigationUrlTests(
        url, '\\bar.pdf', 'http://www.example.com/subdir//bar.pdf');

    // Regression test for https://crbug.com/569040
    doNavigationUrlTests(
        url, 'http://something.else/foo#page=5',
        'http://something.else/foo#page=5');

    chrome.test.succeed();
  },
  /**
   * Test opening a url in the same tab, in a new tab, and in a new window with
   * a file:/// url as the current location.
   */
  function testNavigateFromLocalFile() {
    const url = 'file:///some/path/to/myfile.pdf';

    // Open an absolute link.
    doNavigationUrlTests(
        url, '/foodotcom/bar.pdf', 'file:///foodotcom/bar.pdf');

    chrome.test.succeed();
  },

  function testNavigateInvalidUrls() {
    const url = 'https://example.com/some-web-document.pdf';

    // From non-file: to file:
    doNavigationUrlTests(url, 'file:///bar.pdf', undefined);

    doNavigationUrlTests(url, 'chrome://version', undefined);

    doNavigationUrlTests(
        url, 'javascript:// this is not a document.pdf', undefined);

    doNavigationUrlTests(
        url, 'this-is-not-a-valid-scheme://path.pdf', undefined);

    doNavigationUrlTests(url, '', undefined);

    chrome.test.succeed();
  }
];

const scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
