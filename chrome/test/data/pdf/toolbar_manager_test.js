// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ToolbarManager} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/toolbar_manager.js';

import {MockWindow} from './test_util.js';

// A cut-down version of MockInteractions.move, which is not exposed
// publicly.
function getMouseMoveEvents(fromX, fromY, toX, toY, steps) {
  const dx = Math.round((toX - fromX) / steps);
  const dy = Math.round((toY - fromY) / steps);
  const events = [];

  // Deliberate <= to ensure that an event is run for toX, toY
  for (let i = 0; i <= steps; i++) {
    const e = new MouseEvent('mousemove', {
      clientX: fromX,
      clientY: fromY,
      movementX: dx,
      movementY: dy,
    });
    events.push(e);
    fromX += dx;
    fromY += dy;
  }
  return events;
}

function makeTapEvent(x, y) {
  const e = new MouseEvent('mousemove', {
    clientX: x,
    clientY: y,
    movementX: 0,
    movementY: 0,
    sourceCapabilities: new InputDeviceCapabilities({firesTouchEvents: true})
  });
  return e;
}

/**
 * Mock version of ToolbarManager.getCurrentTimestamp_ which returns a timestamp
 * which linearly increases each time it is called.
 */
function mockGetCurrentTimestamp() {
  this.callCount = this.callCount + 1 || 1;
  return 1449000000000 + this.callCount * 50;
}

const tests = [
  /**
   * Test that ToolbarManager.forceHideTopToolbar hides (or shows) the top
   * toolbar correctly for different mouse movements.
   */
  function testToolbarManagerForceHideTopToolbar() {
    const mockWindow = new MockWindow(1920, 1080);

    const toolbar = document.createElement('viewer-pdf-toolbar');
    document.body.appendChild(toolbar);
    const zoomToolbar = document.createElement('viewer-zoom-toolbar');
    document.body.appendChild(zoomToolbar);
    const toolbarManager = new ToolbarManager(mockWindow, toolbar, zoomToolbar);
    toolbarManager.getCurrentTimestamp_ = mockGetCurrentTimestamp;

    const mouseMove = function(fromX, fromY, toX, toY, steps) {
      getMouseMoveEvents(fromX, fromY, toX, toY, steps).forEach(function(e) {
        toolbarManager.handleMouseMove(e);
      });
    };

    // Force hide the toolbar, then do a quick mousemove in the center of the
    // window. Top toolbar should not show.
    toolbarManager.forceHideTopToolbar();
    chrome.test.assertFalse(toolbar.opened);
    mouseMove(1900, 100, 100, 1000, 3);
    chrome.test.assertFalse(toolbar.opened, 'Closed before move 1');
    // Move back into the zoom toolbar again. The top toolbar should still not
    // show.
    mouseMove(100, 500, 1900, 1000, 3);
    chrome.test.assertFalse(toolbar.opened, 'Closed after move 1');

    // Hide the toolbar, wait for the timeout to expire, then move the mouse
    // quickly. The top toolbar should show.
    toolbarManager.forceHideTopToolbar();
    chrome.test.assertFalse(toolbar.opened, 'Closed before move 2');
    // Manually expire the timeout. This is the same as waiting 1 second.
    mockWindow.runTimeout();
    mouseMove(1900, 1000, 100, 1000, 3);
    chrome.test.assertTrue(toolbar.opened, 'Opened after move 2');

    // Force hide the toolbar, then move the mouse to the top of the screen. The
    // top toolbar should show.
    toolbarManager.forceHideTopToolbar();
    chrome.test.assertFalse(toolbar.opened, 'Closed before move 3');
    mouseMove(1900, 1000, 1000, 30, 3);
    chrome.test.assertTrue(toolbar.opened, 'Opened after move 3');

    chrome.test.succeed();
  },

  /**
   * Test that changes to window height bubble down to dropdowns correctly.
   */
  function testToolbarManagerResizeDropdown() {
    const mockWindow = new MockWindow(1920, 1080);
    const mockZoomToolbar = {
      clientHeight: 400,
      isPrintPreview: false,
    };
    const toolbar = document.getElementById('toolbar');
    const bookmarksDropdown = toolbar.$.bookmarks;

    const toolbarManager =
        new ToolbarManager(mockWindow, toolbar, mockZoomToolbar);

    chrome.test.assertEq(680, bookmarksDropdown.lowerBound);

    mockWindow.setSize(1920, 480);
    chrome.test.assertEq(80, bookmarksDropdown.lowerBound);

    chrome.test.succeed();
  },

  /**
   * Test that the toolbar will not be hidden when navigating with the tab key.
   */
  function testToolbarKeyboardNavigation() {
    const mockWindow = new MockWindow(1920, 1080);
    const toolbar = document.createElement('viewer-pdf-toolbar');
    toolbar.loadProgress = 100;
    document.body.appendChild(toolbar);
    const zoomToolbar = document.createElement('viewer-zoom-toolbar');
    document.body.appendChild(zoomToolbar);
    const toolbarManager = new ToolbarManager(mockWindow, toolbar, zoomToolbar);
    toolbarManager.getCurrentTimestamp_ = mockGetCurrentTimestamp;

    const mouseMove = function(fromX, fromY, toX, toY, steps) {
      getMouseMoveEvents(fromX, fromY, toX, toY, steps).forEach(function(e) {
        toolbarManager.handleMouseMove(e);
      });
    };

    // Move the mouse and then hit tab -> Toolbars stay open.
    mouseMove(200, 200, 800, 800, 5);
    toolbarManager.showToolbarsForKeyboardNavigation();
    chrome.test.assertTrue(toolbar.opened);
    mockWindow.runTimeout();
    chrome.test.assertTrue(
        toolbar.opened, 'toolbar stays open after keyboard navigation');

    // Hit escape -> Toolbars close.
    toolbarManager.hideSingleToolbarLayer();
    chrome.test.assertFalse(toolbar.opened, 'toolbars close on escape key');

    // Show toolbars, use mouse, run timeout -> Toolbars close.
    toolbarManager.showToolbarsForKeyboardNavigation();
    mouseMove(200, 200, 800, 800, 5);
    chrome.test.assertTrue(toolbar.opened);
    mockWindow.runTimeout();
    chrome.test.assertFalse(toolbar.opened, 'toolbars close after mouse move');

    chrome.test.succeed();
  },

  /**
   * Tests that the zoom toolbar becomes visible when it is focused, and is made
   * invisible by calling resetKeyboardNavigationAndHideToolbars().
   * Simulates focusing and then un-focusing the zoom toolbar buttons from Print
   * Preview.
   */
  function testToolbarManagerResetKeyboardNavigation() {
    const mockWindow = new MockWindow(1920, 1080);

    const zoomToolbar = document.createElement('viewer-zoom-toolbar');
    zoomToolbar.isPrintPreview = true;
    document.body.appendChild(zoomToolbar);
    const toolbarManager = new ToolbarManager(mockWindow, null, zoomToolbar);
    toolbarManager.getCurrentTimestamp_ = mockGetCurrentTimestamp;

    // Move the mouse and wait for a timeout to ensure toolbar is invisible.
    getMouseMoveEvents(200, 200, 800, 800, 5).forEach(function(e) {
      toolbarManager.handleMouseMove(e);
    });
    mockWindow.runTimeout();
    chrome.test.assertFalse(zoomToolbar.isVisible());

    // Simulate focusing the fit to page button using the tab key.
    zoomToolbar.$$('#fit-button')
        .dispatchEvent(
            new CustomEvent('focus', {bubbles: true, composed: true}));
    chrome.test.assertTrue(zoomToolbar.isVisible());

    // Call resetKeyboardNavigationAndHideToolbars(). This happens when focus
    // leaves the PDF viewer in Print Preview, and returns to the main Print
    // Preview sidebar UI.
    toolbarManager.resetKeyboardNavigationAndHideToolbars();

    chrome.test.assertTrue(zoomToolbar.isVisible());

    // Simulate re-focusing the zoom toolbar with the tab key. See
    // https://crbug.com/982694.
    zoomToolbar.$$('#fit-button')
        .dispatchEvent(
            new CustomEvent('keyup', {bubbles: true, composed: true}));
    mockWindow.runTimeout();
    chrome.test.assertTrue(zoomToolbar.isVisible());

    // Simulate focus leaving the PDF viewer again, but this time don't
    // refocus the button afterward.
    toolbarManager.resetKeyboardNavigationAndHideToolbars();
    chrome.test.assertTrue(zoomToolbar.isVisible());
    mockWindow.runTimeout();

    // Toolbar should be hidden.
    chrome.test.assertFalse(zoomToolbar.isVisible());

    chrome.test.succeed();
  },

  /*
   * Test that the toolbars can be shown or hidden by tapping with a touch
   * device.
   */
  function testToolbarTouchInteraction() {
    const mockWindow = new MockWindow(1920, 1080);
    const toolbar = document.createElement('viewer-pdf-toolbar');
    toolbar.loadProgress = 100;
    document.body.appendChild(toolbar);
    const zoomToolbar = document.createElement('viewer-zoom-toolbar');
    document.body.appendChild(zoomToolbar);
    const toolbarManager = new ToolbarManager(mockWindow, toolbar, zoomToolbar);

    toolbarManager.hideToolbarsIfAllowed();
    chrome.test.assertFalse(toolbar.opened);

    // Tap anywhere on the screen -> Toolbars open.
    toolbarManager.handleMouseMove(makeTapEvent(500, 500));
    chrome.test.assertTrue(toolbar.opened, 'toolbars open after tap');

    // Tap again -> Toolbars close.
    toolbarManager.handleMouseMove(makeTapEvent(500, 500));
    chrome.test.assertFalse(toolbar.opened, 'toolbars close after tap');

    // Open toolbars, wait 2 seconds -> Toolbars close.
    toolbarManager.handleMouseMove(makeTapEvent(500, 500));
    mockWindow.runTimeout();
    chrome.test.assertFalse(toolbar.opened, 'toolbars close after wait');

    // Open toolbars, tap near toolbars -> Toolbar doesn't close.
    toolbarManager.handleMouseMove(makeTapEvent(500, 500));
    toolbarManager.handleMouseMove(makeTapEvent(100, 75));
    chrome.test.assertTrue(
        toolbar.opened, 'toolbars stay open after tap near toolbars');
    mockWindow.runTimeout();
    chrome.test.assertTrue(
        toolbar.opened, 'tap near toolbars prevents auto close');

    chrome.test.succeed();
  }
];

chrome.test.runTests(tests);
