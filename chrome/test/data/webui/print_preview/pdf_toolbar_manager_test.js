// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/pdf/elements/viewer-zoom-toolbar.js';

import {ToolbarManager} from 'chrome://print/pdf/toolbar_manager.js';
import {assert} from 'chrome://resources/js/assert.m.js';

import {assertFalse, assertTrue} from '../chai_assert.js';

class MockWindow {
  /**
   * @param {number} width
   * @param {number} height
   */
  constructor(width, height) {
    /** @type {number} */
    this.innerWidth = width;

    /** @type {number} */
    this.innerHeight = height;

    /** @type {number} */
    this.pageXOffset = 0;

    /** @type {number} */
    this.pageYOffset = 0;

    /** @type {?Function} */
    this.scrollCallback = null;

    /** @type {?Function} */
    this.resizeCallback = null;

    /** @type {?Function} */
    this.timerCallback = null;
  }

  /**
   * @param {string} e The event name
   * @param {!Function} f The callback
   */
  addEventListener(e, f) {
    if (e === 'scroll') {
      this.scrollCallback = f;
    }
    if (e === 'resize') {
      this.resizeCallback = f;
    }
  }

  /**
   * @param {number} width
   * @param {number} height
   */
  setSize(width, height) {
    this.innerWidth = width;
    this.innerHeight = height;
    this.resizeCallback();
  }

  /**
   * @param {number} x
   * @param {number} y
   */
  scrollTo(x, y) {
    this.pageXOffset = Math.max(0, x);
    this.pageYOffset = Math.max(0, y);
    this.scrollCallback();
  }

  /**
   * @param {Function} callback
   * @return {string}
   */
  setTimeout(callback, time) {
    this.timerCallback = callback;
    return 'timerId';
  }

  /** @param {string} timerId */
  clearTimeout(timerId) {
    this.timerCallback = null;
  }

  runTimeout() {
    if (this.timerCallback) {
      this.timerCallback();
    }
  }
}


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

window.pdf_toolbar_manager_test = {};
pdf_toolbar_manager_test.suiteName = 'PdfToolbarManagerTest';
/** @enum {string} */
pdf_toolbar_manager_test.TestNames = {
  KeyboardNavigation: 'keyboard navigation',
  ResetKeyboardNavigation: 'reset keyboard navigation',
  TouchInteraction: 'touch interaction',
};

suite(pdf_toolbar_manager_test.suiteName, function() {
  /** @type {!MockWindow} */
  let mockWindow;

  /** @type {!ViewerZoomToolbarElement} */
  let zoomToolbar;

  /** @type {!ToolbarManager} */
  let toolbarManager;

  /** @type {number} */
  let callCount = 0;

  setup(function() {
    document.body.innerHTML = '';

    mockWindow = new MockWindow(1920, 1080);
    zoomToolbar = document.createElement('viewer-zoom-toolbar');
    document.body.appendChild(zoomToolbar);
    toolbarManager = new ToolbarManager(mockWindow, zoomToolbar);
    toolbarManager.getCurrentTimestamp_ = () => {
      callCount = callCount + 1 || 1;
      return 1449000000000 + callCount * 50;
    };
  });

  /**
   * Test that the toolbar will not be hidden when navigating with the tab key.
   */
  test(
      assert(pdf_toolbar_manager_test.TestNames.KeyboardNavigation),
      function() {
        const mouseMove = function(fromX, fromY, toX, toY, steps) {
          getMouseMoveEvents(fromX, fromY, toX, toY, steps)
              .forEach(function(e) {
                document.dispatchEvent(e);
              });
        };

        // Move the mouse and then hit tab -> Toolbar stays open.
        mouseMove(200, 200, 800, 800, 5);
        toolbarManager.showToolbarForKeyboardNavigation();
        assertTrue(zoomToolbar.isVisible());
        mockWindow.runTimeout();
        assertTrue(
            zoomToolbar.isVisible(),
            'toolbar stays open after keyboard navigation');

        // Use mouse, run timeout -> Toolbar closes.
        mouseMove(200, 200, 800, 800, 5);
        assertTrue(zoomToolbar.isVisible());
        mockWindow.runTimeout();
        assertFalse(zoomToolbar.isVisible(), 'toolbar closes after mouse move');
      });

  /**
   * Tests that the zoom toolbar becomes visible when it is focused, and is made
   * invisible by calling resetKeyboardNavigationAndHideToolbar().
   * Simulates focusing and then un-focusing the zoom toolbar buttons from Print
   * Preview.
   */
  test(
      assert(pdf_toolbar_manager_test.TestNames.ResetKeyboardNavigation),
      function() {
        // Move the mouse and wait for a timeout to ensure toolbar is invisible.
        getMouseMoveEvents(200, 200, 800, 800, 5).forEach(function(e) {
          document.dispatchEvent(e);
        });
        mockWindow.runTimeout();
        assertFalse(zoomToolbar.isVisible());

        // Simulate focusing the fit to page button using the tab key.
        zoomToolbar.$$('#fit-button')
            .dispatchEvent(
                new CustomEvent('focus', {bubbles: true, composed: true}));
        assertTrue(zoomToolbar.isVisible());

        // Call resetKeyboardNavigationAndHideToolbar(). This happens when focus
        // leaves the PDF viewer in Print Preview, and returns to the main Print
        // Preview sidebar UI.
        toolbarManager.resetKeyboardNavigationAndHideToolbar();

        assertTrue(zoomToolbar.isVisible());

        // Simulate re-focusing the zoom toolbar with the tab key. See
        // https://crbug.com/982694.
        zoomToolbar.$$('#fit-button')
            .dispatchEvent(
                new CustomEvent('keyup', {bubbles: true, composed: true}));
        mockWindow.runTimeout();
        assertTrue(zoomToolbar.isVisible());

        // Simulate focus leaving the PDF viewer again, but this time don't
        // refocus the button afterward.
        toolbarManager.resetKeyboardNavigationAndHideToolbar();
        assertTrue(zoomToolbar.isVisible());
        mockWindow.runTimeout();

        // Toolbar should be hidden.
        assertFalse(zoomToolbar.isVisible());
      });

  /*
   * Test that the toolbars can be shown or hidden by tapping with a touch
   * device.
   */
  test(assert(pdf_toolbar_manager_test.TestNames.TouchInteraction), function() {
    toolbarManager.resetKeyboardNavigationAndHideToolbar();
    mockWindow.runTimeout();
    assertFalse(zoomToolbar.isVisible());

    // Tap anywhere on the screen -> Toolbar opens.
    document.dispatchEvent(makeTapEvent(500, 500));
    assertTrue(zoomToolbar.isVisible(), 'toolbar opens after tap');

    // Tap again -> Toolbar closes.
    document.dispatchEvent(makeTapEvent(500, 500));
    assertFalse(zoomToolbar.isVisible(), 'toolbar closes after tap');

    // Open toolbars, wait 2 seconds -> Toolbar closes.
    document.dispatchEvent(makeTapEvent(500, 500));
    mockWindow.runTimeout();
    assertFalse(zoomToolbar.isVisible(), 'toolbar closes after wait');

    // Open toolbar, tap near toolbar -> Toolbar doesn't close.
    document.dispatchEvent(makeTapEvent(500, 500));
    document.dispatchEvent(makeTapEvent(100, 1000));
    assertTrue(
        zoomToolbar.isVisible(), 'toolbar stays open after tap near toolbar');
    mockWindow.runTimeout();
    assertTrue(zoomToolbar.isVisible(), 'tap near toolbar prevents auto close');
  });
});
