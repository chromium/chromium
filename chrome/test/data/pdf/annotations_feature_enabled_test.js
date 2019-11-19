// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';

window.onerror = e => chrome.test.fail(e.stack);
window.onunhandledrejection = e => chrome.test.fail(e.reason);

function animationFrame() {
  return new Promise(resolve => requestAnimationFrame(resolve));
}

// Async spin until predicate() returns true.
function waitFor(predicate) {
  if (predicate()) {
    return;
  }
  return new Promise(resolve => setTimeout(() => {
                       resolve(waitFor(predicate));
                     }, 0));
}

function contentElement() {
  return document.elementFromPoint(innerWidth / 2, innerHeight / 2);
}

function isAnnotationMode() {
  return document.querySelector('#toolbar').annotationMode;
}

async function testAsync(f) {
  try {
    await f();
    chrome.test.succeed();
  } catch (e) {
    chrome.test.fail(e.stack);
  }
}

chrome.test.runTests([
  function testAnnotationsEnabled() {
    const toolbar = document.body.querySelector('#toolbar');
    chrome.test.assertTrue(toolbar.pdfAnnotationsEnabled);
    chrome.test.assertTrue(
        toolbar.shadowRoot.querySelector('#annotate') != null);
    chrome.test.succeed();
  },
  function testEnterAnnotationMode() {
    testAsync(async () => {
      chrome.test.assertEq('EMBED', contentElement().tagName);

      // Enter annotation mode.
      $('toolbar').toggleAnnotation();
      await viewer.loaded;
      chrome.test.assertEq('VIEWER-INK-HOST', contentElement().tagName);
    });
  },
  function testViewportToCameraConversion() {
    testAsync(async () => {
      chrome.test.assertTrue(isAnnotationMode());
      const inkHost = contentElement();
      const cameras = [];
      inkHost.ink_.setCamera = camera => cameras.push(camera);

      viewer.viewport_.setZoom(1);
      viewer.viewport_.setZoom(2);
      chrome.test.assertEq(2, cameras.length);

      window.scrollTo(100, 100);
      await animationFrame();

      chrome.test.assertEq(3, cameras.length);

      const expectations = [
        {top: 44.25, left: -106.5, right: 718.5, bottom: -448.5},
        {top: 23.25, left: -3.75, right: 408.75, bottom: -223.125},
        {top: -14.25, left: 33.75, right: 446.25, bottom: -260.625},
      ];

      for (const expectation of expectations) {
        const actual = cameras.shift();
        chrome.test.assertEq(expectation.top, actual.top);
        chrome.test.assertEq(expectation.left, actual.left);
        chrome.test.assertEq(expectation.bottom, actual.bottom);
        chrome.test.assertEq(expectation.right, actual.right);
      }
    });
  },
  function testPenOptions() {
    testAsync(async () => {
      chrome.test.assertTrue(isAnnotationMode());
      const inkHost = contentElement();
      let tool = null;
      inkHost.ink_.setAnnotationTool = value => tool = value;

      // Pen defaults.
      const viewerPdfToolbar = document.querySelector('viewer-pdf-toolbar');
      const pen = viewerPdfToolbar.$$('#pen');
      pen.click();
      chrome.test.assertEq('pen', tool.tool);
      chrome.test.assertEq(0.1429, tool.size);
      chrome.test.assertEq('#000000', tool.color);


      // Selected size and color.
      const penOptions = viewerPdfToolbar.$$('#pen viewer-pen-options');
      penOptions.$$('#sizes [value="1"]').click();
      penOptions.$$('#colors [value="#00b0ff"]').click();
      await animationFrame();
      chrome.test.assertEq('pen', tool.tool);
      chrome.test.assertEq(1, tool.size);
      chrome.test.assertEq('#00b0ff', tool.color);


      // Eraser defaults.
      viewerPdfToolbar.$$('#eraser').click();
      chrome.test.assertEq('eraser', tool.tool);
      chrome.test.assertEq(1, tool.size);
      chrome.test.assertEq(null, tool.color);


      // Pen keeps previous settings.
      pen.click();
      chrome.test.assertEq('pen', tool.tool);
      chrome.test.assertEq(1, tool.size);
      chrome.test.assertEq('#00b0ff', tool.color);


      // Highlighter defaults.
      viewerPdfToolbar.$$('#highlighter').click();
      chrome.test.assertEq('highlighter', tool.tool);
      chrome.test.assertEq(0.7143, tool.size);
      chrome.test.assertEq('#ffbc00', tool.color);


      // Need to expand to use this color.
      const highlighterOptions =
          viewerPdfToolbar.$$('#highlighter viewer-pen-options');
      highlighterOptions.$$('#colors [value="#d1c4e9"]').click();
      chrome.test.assertEq('#ffbc00', tool.color);

      // Selected size and expanded color.
      highlighterOptions.$$('#sizes [value="1"]').click();
      highlighterOptions.$$('#colors #expand').click();
      highlighterOptions.$$('#colors [value="#d1c4e9"]').click();
      chrome.test.assertEq('highlighter', tool.tool);
      chrome.test.assertEq(1, tool.size);
      chrome.test.assertEq('#d1c4e9', tool.color);
    });
  },
  function testStrokeUndoRedo() {
    testAsync(async () => {
      const inkHost = contentElement();
      const viewerPdfToolbar = document.querySelector('viewer-pdf-toolbar');
      const undo = viewerPdfToolbar.$$('#undo');
      const redo = viewerPdfToolbar.$$('#redo');

      const pen = {
        pointerId: 2,
        pointerType: 'pen',
        pressure: 0.5,
        clientX: inkHost.offsetWidth / 2,
        clientY: inkHost.offsetHeight / 2,
        buttons: 0,
      };

      // Initial state.
      chrome.test.assertEq(undo.disabled, true);
      chrome.test.assertEq(redo.disabled, true);

      // Draw a stroke.
      inkHost.dispatchEvent(new PointerEvent('pointerdown', pen));
      inkHost.dispatchEvent(new PointerEvent('pointermove', pen));
      inkHost.dispatchEvent(new PointerEvent('pointerup', pen));

      await waitFor(() => undo.disabled == false);
      chrome.test.assertEq(redo.disabled, true);

      undo.click();
      await waitFor(() => undo.disabled == true);
      chrome.test.assertEq(redo.disabled, false);

      redo.click();
      await waitFor(() => undo.disabled == false);
      chrome.test.assertEq(redo.disabled, true);
    });
  },
  function testPointerEvents() {
    testAsync(async () => {
      chrome.test.assertTrue(isAnnotationMode());
      const inkHost = contentElement();
      inkHost.resetPenMode();
      const events = [];
      inkHost.ink_.dispatchPointerEvent = (type, init) =>
          events.push({type: type, init: init});

      const mouse = {pointerId: 1, pointerType: 'mouse', buttons: 1};
      const pen = {
        pointerId: 2,
        pointerType: 'pen',
        pressure: 0.5,
        clientX: 3,
        clientY: 4,
        buttons: 0,
      };
      const touch1 = {pointerId: 11, pointerType: 'touch'};
      const touch2 = {pointerId: 22, pointerType: 'touch'};

      function checkExpectations(expectations) {
        chrome.test.assertEq(expectations.length, events.length);
        while (expectations.length) {
          const event = events.shift();
          const expectation = expectations.shift();
          chrome.test.assertEq(expectation.type, event.type);
          for (const key of Object.keys(expectation.init)) {
            chrome.test.assertEq(expectation.init[key], event.init[key]);
          }
        }
      }

      // Normal sequence.
      inkHost.dispatchEvent(new PointerEvent('pointerdown', pen));
      inkHost.dispatchEvent(new PointerEvent('pointermove', pen));
      inkHost.dispatchEvent(new PointerEvent('pointerup', pen));
      checkExpectations([
        {type: 'pointerdown', init: pen},
        {type: 'pointermove', init: pen},
        {type: 'pointerup', init: pen},
      ]);

      // Multi-touch gesture should cancel and suppress first pointer.
      inkHost.resetPenMode();
      inkHost.dispatchEvent(new PointerEvent('pointerdown', touch1));
      inkHost.dispatchEvent(new PointerEvent('pointerdown', touch2));
      inkHost.dispatchEvent(new PointerEvent('pointermove', touch1));
      inkHost.dispatchEvent(new PointerEvent('pointerup', touch1));
      checkExpectations([
        {type: 'pointerdown', init: touch1},
        {type: 'pointercancel', init: touch1},
      ]);

      // Pointers which are not active should be suppressed.
      inkHost.resetPenMode();
      inkHost.dispatchEvent(new PointerEvent('pointerdown', mouse));
      inkHost.dispatchEvent(new PointerEvent('pointerdown', pen));
      inkHost.dispatchEvent(new PointerEvent('pointerdown', touch1));
      inkHost.dispatchEvent(new PointerEvent('pointermove', mouse));
      inkHost.dispatchEvent(new PointerEvent('pointermove', pen));
      inkHost.dispatchEvent(new PointerEvent('pointermove', touch1));
      inkHost.dispatchEvent(new PointerEvent('pointerup', mouse));
      inkHost.dispatchEvent(new PointerEvent('pointermove', pen));
      checkExpectations([
        {type: 'pointerdown', init: mouse},
        {type: 'pointermove', init: mouse},
        {type: 'pointerup', init: mouse},
      ]);

      // pointerleave should cause mouseup
      inkHost.dispatchEvent(new PointerEvent('pointerdown', mouse));
      inkHost.dispatchEvent(new PointerEvent('pointerleave', mouse));
      checkExpectations([
        {type: 'pointerdown', init: mouse},
        {type: 'pointerup', init: mouse},
      ]);

      // pointerleave does not apply to non-mouse pointers
      inkHost.dispatchEvent(new PointerEvent('pointerdown', pen));
      inkHost.dispatchEvent(new PointerEvent('pointerleave', pen));
      inkHost.dispatchEvent(new PointerEvent('pointerup', pen));
      checkExpectations([
        {type: 'pointerdown', init: pen},
        {type: 'pointerup', init: pen},
      ]);

      // Browser will cancel touch on pen input
      inkHost.resetPenMode();
      inkHost.dispatchEvent(new PointerEvent('pointerdown', touch1));
      inkHost.dispatchEvent(new PointerEvent('pointercancel', touch1));
      inkHost.dispatchEvent(new PointerEvent('pointerdown', pen));
      inkHost.dispatchEvent(new PointerEvent('pointerup', pen));
      checkExpectations([
        {type: 'pointerdown', init: touch1},
        {type: 'pointercancel', init: touch1},
        {type: 'pointerdown', init: pen},
        {type: 'pointerup', init: pen},
      ]);
    });
  },
  function testTouchPanGestures() {
    testAsync(async () => {
      // Ensure that we have an out-of-bounds area.
      viewer.viewport_.setZoom(0.5);
      chrome.test.assertTrue(isAnnotationMode());
      const inkHost = contentElement();
      function dispatchPointerEvent(type, init) {
        const pointerEvent = new PointerEvent(type, init);
        inkHost.dispatchEvent(pointerEvent);
        return pointerEvent;
      }
      function dispatchPointerAndTouchEvents(type, init) {
        const pointerEvent = dispatchPointerEvent(type, init);
        let touchPrevented = false;
        inkHost.onTouchStart_({
          timeStamp: pointerEvent.timeStamp,
          preventDefault() {
            touchPrevented = true;
          }
        });
        return touchPrevented;
      }

      const pen = {
        pointerId: 2,
        pointerType: 'pen',
        pressure: 0.5,
        clientX: innerWidth / 2,
        clientY: innerHeight / 2,
      };

      const outOfBoundsPen = {
        pointerId: 2,
        pointerType: 'pen',
        pressure: 0.5,
        clientX: 2,
        clientY: 3,
      };

      const touch = {
        pointerId: 3,
        pointerType: 'touch',
        pressure: 0.5,
        clientX: innerWidth / 2,
        clientY: innerHeight / 2,
      };

      const outOfBoundsTouch = {
        pointerId: 4,
        pointerType: 'touch',
        pressure: 0.5,
        clientX: 4,
        clientY: 5,
      };

      inkHost.resetPenMode();
      let prevented = dispatchPointerAndTouchEvents('pointerdown', touch);
      dispatchPointerEvent('pointerup', touch);
      chrome.test.assertTrue(
          prevented, 'in document touch should prevent default');

      prevented =
          dispatchPointerAndTouchEvents('pointerdown', outOfBoundsTouch);
      dispatchPointerEvent('pointerup', outOfBoundsTouch);
      chrome.test.assertFalse(
          prevented, 'out of bounds touch should start gesture');

      prevented = dispatchPointerAndTouchEvents('pointerdown', pen);
      dispatchPointerEvent('pointerup', pen);
      chrome.test.assertTrue(
          prevented, 'in document pen should prevent default');

      prevented = dispatchPointerAndTouchEvents('pointerdown', outOfBoundsPen);
      dispatchPointerEvent('pointerup', outOfBoundsPen);
      chrome.test.assertFalse(
          prevented, 'out of bounds pen should start gesture');

      chrome.test.assertTrue(
          inkHost.penMode_, 'pen input should switch to pen mode');
      prevented = dispatchPointerAndTouchEvents('pointerdown', touch);
      dispatchPointerEvent('pointerup', touch);
      chrome.test.assertFalse(
          prevented, 'in document touch in pen mode should start gesture');
    });
  },
  function testExitAnnotationMode() {
    testAsync(async () => {
      chrome.test.assertTrue(isAnnotationMode());
      // Exit annotation mode.
      $('toolbar').toggleAnnotation();
      await viewer.loaded;
      chrome.test.assertEq('EMBED', contentElement().tagName);
    });
  },
]);
