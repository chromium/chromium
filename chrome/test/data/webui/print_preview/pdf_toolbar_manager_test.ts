// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ViewerZoomToolbarElement} from 'chrome://print/pdf/pdf_print_wrapper.js';
import {ToolbarManager} from 'chrome://print/pdf/pdf_print_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

class MockWindow {
  innerWidth: number;
  innerHeight: number;
  pageXOffset: number = 0;
  pageYOffset: number = 0;
  resizeCallback: ((param?: any) => void)|null = null;
  scrollCallback: ((param?: any) => void)|null = null;
  timerCallback: (() => void)|null = null;

  constructor(width: number, height: number) {
    this.innerWidth = width;
    this.innerHeight = height;
  }

  /**
   * @param e The event name
   * @param f The callback
   */
  addEventListener(e: string, f: (param?: any) => void) {
    if (e === 'scroll') {
      this.scrollCallback = f;
    }
    if (e === 'resize') {
      this.resizeCallback = f;
    }
  }

  setSize(width: number, height: number) {
    this.innerWidth = width;
    this.innerHeight = height;
    assert(this.resizeCallback);
    this.resizeCallback();
  }

  scrollTo(options?: ScrollToOptions|undefined): void;
  scrollTo(x: number, y: number): void;
  scrollTo(xOrOptions: ScrollToOptions|number|undefined, y?: number) {
    this.pageXOffset = Math.max(0, xOrOptions as number);
    this.pageYOffset = Math.max(0, y as number);
    assert(this.scrollCallback);
    this.scrollCallback();
  }

  setTimeout(callback: () => void): number {
    this.timerCallback = callback;
    return 111;
  }

  clearTimeout(_timerId: number|undefined) {
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
function getMouseMoveEvents(
    fromX: number, fromY: number, toX: number, toY: number,
    steps: number): MouseEvent[] {
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

function makeTapEvent(x: number, y: number): MouseEvent {
  const e = new MouseEvent('mousemove', {
    clientX: x,
    clientY: y,
    movementX: 0,
    movementY: 0,
    sourceCapabilities: new InputDeviceCapabilities({firesTouchEvents: true}),
  });
  return e;
}

suite('PdfToolbarManagerTest', function() {
  let mockWindow: MockWindow;

  let zoomToolbar: ViewerZoomToolbarElement;

  let toolbarManager: ToolbarManager;

  let callCount: number = 0;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    mockWindow = new MockWindow(1920, 1080);
    zoomToolbar = document.createElement('viewer-zoom-toolbar');
    document.body.appendChild(zoomToolbar);
    toolbarManager =
        new ToolbarManager(mockWindow as unknown as Window, zoomToolbar);
    toolbarManager.getCurrentTimestamp = () => {
      callCount = callCount + 1 || 1;
      return 1449000000000 + callCount * 50;
    };
  });

  /**
   * Test that the toolbar will not be hidden when navigating with the tab key.
   */
  test('KeyboardNavigation', function() {
    function mouseMove(
        fromX: number, fromY: number, toX: number, toY: number, steps: number) {
      getMouseMoveEvents(fromX, fromY, toX, toY, steps)
          .forEach(function(e: Event) {
            document.dispatchEvent(e);
          });
    }

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
  test('ResetKeyboardNavigation', function() {
    // Move the mouse and wait for a timeout to ensure toolbar is invisible.
    getMouseMoveEvents(200, 200, 800, 800, 5).forEach(function(e: Event) {
      document.dispatchEvent(e);
    });
    mockWindow.runTimeout();
    assertFalse(zoomToolbar.isVisible());

    // Simulate focusing the fit to page button using the tab key.
    zoomToolbar.$.fitButton.dispatchEvent(
        new CustomEvent('focus', {bubbles: true, composed: true}));
    assertTrue(zoomToolbar.isVisible());

    // Call resetKeyboardNavigationAndHideToolbar(). This happens when focus
    // leaves the PDF viewer in Print Preview, and returns to the main Print
    // Preview sidebar UI.
    toolbarManager.resetKeyboardNavigationAndHideToolbar();

    assertTrue(zoomToolbar.isVisible());

    // Simulate re-focusing the zoom toolbar with the tab key. See
    // https://crbug.com/982694.
    zoomToolbar.$.fitButton.dispatchEvent(
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
  test('TouchInteraction', function() {
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
