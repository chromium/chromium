// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_fitting_type.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createBookmarksForTest} from './test_util.js';

/**
 * Captures 'fit-to-changed' events and verifies the last one has the expected
 * paylod.
 */
class FitToEventChecker {
  constructor(zoomToolbar) {
    this.lastEvent_ = null;
    zoomToolbar.addEventListener('fit-to-changed', e => this.lastEvent_ = e);
  }

  /**
   * Asserts the last event has the expected payload.
   * @param {FittingType} fittingType Expected fitting type.
   * @param {boolean} userInitiated Expected "is user initiated" flag.
   */
  assertEvent(fittingType, userInitiated) {
    chrome.test.assertEq('fit-to-changed', this.lastEvent_.type);
    chrome.test.assertEq(fittingType, this.lastEvent_.detail.fittingType);
    chrome.test.assertEq(userInitiated, this.lastEvent_.detail.userInitiated);
    this.lastEvent_ = null;
  }
}

/**
 * Standalone unit tests of the PDF Polymer elements.
 */
const tests = [
  /**
   * Test that viewer-page-selector reacts correctly to text entry. The page
   * selector validates that input is an integer, and does not allow navigation
   * past document bounds.
   */
  function testPageSelectorChange() {
    const selector = document.createElement('viewer-page-selector');
    selector.docLength = 1234;
    document.body.appendChild(selector);

    const input = selector.pageSelector;
    // Simulate entering text into `input` and pressing enter.
    function changeInput(newValue) {
      input.value = newValue;
      input.dispatchEvent(new CustomEvent('change'));
    }

    const navigatedPages = [];
    selector.addEventListener('change-page', function(e) {
      navigatedPages.push(e.detail.page);
      // A change-page handler is expected to set the pageNo to the new value.
      selector.pageNo = e.detail.page + 1;
    });

    changeInput('1000');
    changeInput('1234');
    changeInput('abcd');
    changeInput('12pp');
    changeInput('3.14');
    changeInput('3000');

    chrome.test.assertEq(4, navigatedPages.length);
    // The event page number is 0-based.
    chrome.test.assertEq(999, navigatedPages[0]);
    chrome.test.assertEq(1233, navigatedPages[1]);
    chrome.test.assertEq(11, navigatedPages[2]);
    // If the user types 3.14, the . will be ignored but 14 will be captured.
    chrome.test.assertEq(313, navigatedPages[3]);

    chrome.test.succeed();
  },

  /**
   * Test that viewer-page-selector changes in response to setting docLength.
   */
  function testPageSelectorDocLength() {
    const selector = document.createElement('viewer-page-selector');
    selector.docLength = 1234;
    document.body.appendChild(selector);
    chrome.test.assertEq('1234', selector.$.pagelength.textContent);
    chrome.test.assertEq(
        '4', selector.style.getPropertyValue('--page-length-digits'));
    chrome.test.succeed();
  },

  /**
   * Test that clicking the dropdown icon opens/closes the dropdown.
   */
  function testToolbarDropdownShowHide() {
    const dropdown = document.createElement('viewer-toolbar-dropdown');
    dropdown.header = 'Test Menu';
    dropdown.closedIcon = 'closedIcon';
    dropdown.openIcon = 'openIcon';
    document.body.appendChild(dropdown);

    const button = dropdown.$.button;
    chrome.test.assertFalse(dropdown.dropdownOpen);
    chrome.test.assertEq('closedIcon,cr:arrow-drop-down', button.ironIcon);

    button.click();

    chrome.test.assertTrue(dropdown.dropdownOpen);
    chrome.test.assertEq('openIcon,cr:arrow-drop-down', button.ironIcon);

    button.click();

    chrome.test.assertFalse(dropdown.dropdownOpen);

    chrome.test.succeed();
  },

  /**
   * Test that viewer-bookmarks-content creates a bookmark tree with the correct
   * structure and behaviour.
   */
  function testBookmarkStructure() {
    const bookmarkContent = createBookmarksForTest();
    bookmarkContent.bookmarks = [{
      title: 'Test 1',
      page: 1,
      children: [
        {title: 'Test 1a', page: 2, children: []},
        {title: 'Test 1b', page: 3, children: []}
      ]
    }];
    document.body.appendChild(bookmarkContent);

    // Force templates to render.
    flush();

    const rootBookmarks =
        bookmarkContent.shadowRoot.querySelectorAll('viewer-bookmark');
    chrome.test.assertEq(1, rootBookmarks.length, 'one root bookmark');
    const rootBookmark = rootBookmarks[0];
    rootBookmark.$.expand.click();

    flush();

    const subBookmarks =
        rootBookmark.shadowRoot.querySelectorAll('viewer-bookmark');
    chrome.test.assertEq(2, subBookmarks.length, 'two sub bookmarks');
    chrome.test.assertEq(
        1, subBookmarks[1].depth, 'sub bookmark depth correct');

    let lastPageChange;
    rootBookmark.addEventListener('change-page', function(e) {
      lastPageChange = e.detail.page;
    });

    rootBookmark.$.item.click();
    chrome.test.assertEq(1, lastPageChange);

    subBookmarks[1].$.item.click();
    chrome.test.assertEq(3, lastPageChange);

    chrome.test.succeed();
  },

  /**
   * Test that the zoom toolbar toggles between showing the fit-to-page and
   * fit-to-width buttons.
   */
  function testZoomToolbarToggle() {
    const zoomToolbar = document.createElement('viewer-zoom-toolbar');
    document.body.appendChild(zoomToolbar);
    const fitButton = zoomToolbar.$['fit-button'];
    const button = fitButton.$$('cr-icon-button');

    const fitWidthIcon = 'fullscreen';
    const fitPageIcon = 'fullscreen-exit';

    const fitToEventChecker = new FitToEventChecker(zoomToolbar);

    // Initial: Show fit-to-page.
    // TODO(tsergeant): This assertion attempts to be resilient to iconset
    // changes. A better solution is something like
    // https://github.com/PolymerElements/iron-icon/issues/68.
    chrome.test.assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Tap 1: Fire fit-to-changed(FIT_TO_PAGE), show fit-to-width.
    button.click();
    fitToEventChecker.assertEvent(FittingType.FIT_TO_PAGE, true);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    // Tap 2: Fire fit-to-changed(FIT_TO_WIDTH), show fit-to-page.
    button.click();
    fitToEventChecker.assertEvent(FittingType.FIT_TO_WIDTH, true);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Tap 3: Fire fit-to-changed(FIT_TO_PAGE) again.
    button.click();
    fitToEventChecker.assertEvent(FittingType.FIT_TO_PAGE, true);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    // Do the same as above, but with fitToggleFromHotKey().
    zoomToolbar.fitToggleFromHotKey();
    fitToEventChecker.assertEvent(FittingType.FIT_TO_WIDTH, true);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitPageIcon));
    zoomToolbar.fitToggleFromHotKey();
    fitToEventChecker.assertEvent(FittingType.FIT_TO_PAGE, true);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitWidthIcon));
    zoomToolbar.fitToggleFromHotKey();
    fitToEventChecker.assertEvent(FittingType.FIT_TO_WIDTH, true);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Tap 4: Fire fit-to-changed(FIT_TO_PAGE) again.
    button.click();
    fitToEventChecker.assertEvent(FittingType.FIT_TO_PAGE, true);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    chrome.test.succeed();
  },

  function testZoomToolbarForceFitToPage() {
    const zoomToolbar = document.createElement('viewer-zoom-toolbar');
    document.body.appendChild(zoomToolbar);
    const fitButton = zoomToolbar.$['fit-button'];
    const button = fitButton.$$('cr-icon-button');

    const fitWidthIcon = 'fullscreen';
    const fitPageIcon = 'fullscreen-exit';

    const fitToEventChecker = new FitToEventChecker(zoomToolbar);

    // Initial: Show fit-to-page.
    chrome.test.assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Test forceFit(FIT_TO_PAGE) from initial state.
    zoomToolbar.forceFit(FittingType.FIT_TO_PAGE);
    fitToEventChecker.assertEvent(FittingType.FIT_TO_PAGE, false);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    // Tap 1: Fire fit-to-changed(FIT_TO_WIDTH).
    button.click();
    fitToEventChecker.assertEvent(FittingType.FIT_TO_WIDTH, true);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Test forceFit(FIT_TO_PAGE) from fit-to-width mode.
    zoomToolbar.forceFit(FittingType.FIT_TO_PAGE);
    fitToEventChecker.assertEvent(FittingType.FIT_TO_PAGE, false);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    // Test forceFit(FIT_TO_PAGE) when already in fit-to-page mode.
    zoomToolbar.forceFit(FittingType.FIT_TO_PAGE);
    fitToEventChecker.assertEvent(FittingType.FIT_TO_PAGE, false);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    // Tap 2: Fire fit-to-changed(FIT_TO_WIDTH).
    button.click();
    fitToEventChecker.assertEvent(FittingType.FIT_TO_WIDTH, true);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitPageIcon));

    chrome.test.succeed();
  },

  function testZoomToolbarForceFitToWidth() {
    const zoomToolbar = document.createElement('viewer-zoom-toolbar');
    document.body.appendChild(zoomToolbar);
    const fitButton = zoomToolbar.$['fit-button'];
    const button = fitButton.$$('cr-icon-button');

    const fitWidthIcon = 'fullscreen';
    const fitPageIcon = 'fullscreen-exit';

    const fitToEventChecker = new FitToEventChecker(zoomToolbar);

    // Initial: Show fit-to-page.
    chrome.test.assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Test forceFit(FIT_TO_WIDTH) from initial state.
    zoomToolbar.forceFit(FittingType.FIT_TO_WIDTH);
    fitToEventChecker.assertEvent(FittingType.FIT_TO_WIDTH, false);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Tap 1: Fire fit-to-changed(FIT_TO_PAGE).
    button.click();
    fitToEventChecker.assertEvent(FittingType.FIT_TO_PAGE, true);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    // Tap 2: Fire fit-to-changed(FIT_TO_WIDTH).
    button.click();
    fitToEventChecker.assertEvent(FittingType.FIT_TO_WIDTH, true);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Test forceFit(FIT_TO_WIDTH) from fit-to-width state.
    zoomToolbar.forceFit(FittingType.FIT_TO_WIDTH);
    fitToEventChecker.assertEvent(FittingType.FIT_TO_WIDTH, false);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Tap 3: Fire fit-to-changed(FIT_TO_PAGE).
    button.click();
    fitToEventChecker.assertEvent(FittingType.FIT_TO_PAGE, true);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    // Test forceFit(FIT_TO_WIDTH) from fit-to-page state.
    zoomToolbar.forceFit(FittingType.FIT_TO_WIDTH);
    fitToEventChecker.assertEvent(FittingType.FIT_TO_WIDTH, false);
    chrome.test.assertTrue(button.ironIcon.endsWith(fitPageIcon));

    chrome.test.succeed();
  }
];

chrome.test.runTests(tests);
