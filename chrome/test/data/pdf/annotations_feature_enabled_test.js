// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise, waitBeforeNextRender} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/_test_resources/webui/test_util.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {testAsync} from './test_util.js';

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
  return viewer.shadowRoot.elementFromPoint(innerWidth / 2, innerHeight / 2);
}

function isAnnotationMode() {
  return viewer.shadowRoot.querySelector('#toolbar').annotationMode;
}

chrome.test.runTests([
  function testAnnotationsEnabled() {
    const toolbar = viewer.shadowRoot.querySelector('#toolbar');
    chrome.test.assertTrue(loadTimeData.getBoolean('pdfAnnotationsEnabled'));
    chrome.test.assertTrue(
        toolbar.shadowRoot.querySelector('#annotate') != null);
    chrome.test.succeed();
  },
  function testEnterAnnotationMode() {
    testAsync(async () => {
      chrome.test.assertEq('EMBED', contentElement().tagName);

      // Enter annotation mode.
      viewer.shadowRoot.querySelector('#toolbar').toggleAnnotation();
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

      viewer.viewport.setZoom(1);
      viewer.viewport.setZoom(2);
      chrome.test.assertEq(2, cameras.length);

      const updateEnabled =
          document.documentElement.hasAttribute('pdf-viewer-update-enabled');
      const scrollingContainer =
          updateEnabled ? viewer.shadowRoot.querySelector('#scroller') : window;
      scrollingContainer.scrollTo(100, 100);
      await animationFrame();

      chrome.test.assertEq(3, cameras.length);

      const expectations = [
        {top: 44.25, left: -106.5, right: 718.5, bottom: -448.5},
        {top: 23.25, left: -3.75, right: 408.75, bottom: -223.125},
        {top: -14.25, left: 33.75, right: 446.25, bottom: -260.625},
      ];

      for (const expectation of expectations) {
        const actual = cameras.shift();
        const expectationTop = updateEnabled ?
            Math.min(2.25, expectation.top - 21) :
            expectation.top;
        chrome.test.assertEq(expectationTop, actual.top);
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
      const viewerPdfToolbar = viewer.shadowRoot.querySelector('#toolbar');
      const viewerAnnotationsBar =
          viewerPdfToolbar.shadowRoot.querySelector('viewer-annotations-bar');
      const pen = viewerAnnotationsBar.shadowRoot.querySelector('#pen');
      pen.click();
      chrome.test.assertEq('pen', tool.tool);
      chrome.test.assertEq(0.1429, tool.size);
      chrome.test.assertEq('#000000', tool.color);


      // Selected size and color.
      const penOptions = viewerAnnotationsBar.shadowRoot.querySelector(
          '#pen viewer-pen-options');
      penOptions.$$('#sizes [value="1"]').click();
      penOptions.$$('#colors [value="#00b0ff"]').click();
      await animationFrame();
      chrome.test.assertEq('pen', tool.tool);
      chrome.test.assertEq(1, tool.size);
      chrome.test.assertEq('#00b0ff', tool.color);


      // Eraser defaults.
      viewerAnnotationsBar.shadowRoot.querySelector('#eraser').click();
      chrome.test.assertEq('eraser', tool.tool);
      chrome.test.assertEq(1, tool.size);
      chrome.test.assertEq(null, tool.color);


      // Pen keeps previous settings.
      pen.click();
      chrome.test.assertEq('pen', tool.tool);
      chrome.test.assertEq(1, tool.size);
      chrome.test.assertEq('#00b0ff', tool.color);


      // Highlighter defaults.
      viewerAnnotationsBar.shadowRoot.querySelector('#highlighter').click();
      chrome.test.assertEq('highlighter', tool.tool);
      chrome.test.assertEq(0.7143, tool.size);
      chrome.test.assertEq('#ffbc00', tool.color);


      // Need to expand to use this color.
      const highlighterOptions = viewerAnnotationsBar.shadowRoot.querySelector(
          '#highlighter viewer-pen-options');
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
      const viewerPdfToolbar = viewer.shadowRoot.querySelector('#toolbar');
      const viewerAnnotationsBar =
          viewerPdfToolbar.shadowRoot.querySelector('viewer-annotations-bar');
      const undo = viewerAnnotationsBar.shadowRoot.querySelector('#undo');
      const redo = viewerAnnotationsBar.shadowRoot.querySelector('#redo');

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

      await waitFor(() => undo.disabled === false);
      chrome.test.assertEq(redo.disabled, true);

      undo.click();
      await waitFor(() => undo.disabled === true);
      chrome.test.assertEq(redo.disabled, false);

      redo.click();
      await waitFor(() => undo.disabled === false);
      chrome.test.assertEq(redo.disabled, true);
    });
  },
  function testPointerEvents() {
    testAsync(async () => {
      chrome.test.assertTrue(isAnnotationMode());
      const inkHost = contentElement();
      inkHost.resetPenMode();
      const events = [];
      inkHost.ink_.dispatchPointerEvent = (ev) => void events.push(ev);

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
            chrome.test.assertEq(expectation.init[key], event[key]);
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
      viewer.shadowRoot.querySelector('#toolbar').toggleAnnotation();
      await viewer.loaded;
      chrome.test.assertEq('EMBED', contentElement().tagName);
    });
  },
  function testHidingAnnotationsExitsAnnotationsMode() {
    testAsync(async () => {
      document.body.innerHTML = '';
      const toolbar = document.createElement('viewer-pdf-toolbar-new');
      document.body.appendChild(toolbar);
      toolbar.toggleAnnotation();
      // This is normally done by the parent in response to the event fired by
      // toggleAnnotation().
      toolbar.annotationMode = true;

      await toolbar.addEventListener('display-annotations-changed', async e => {
        chrome.test.assertFalse(e.detail);
        await waitFor(() => toolbar.annotationMode === false);
        chrome.test.succeed();
      });
      toolbar.shadowRoot.querySelector('#show-annotations-button').click();
    });
  },
  function testEnteringAnnotationsModeShowsAnnotations() {
    document.body.innerHTML = '';
    const toolbar = document.createElement('viewer-pdf-toolbar-new');
    document.body.appendChild(toolbar);
    chrome.test.assertFalse(toolbar.annotationMode);

    // Hide annotations.
    toolbar.shadowRoot.querySelector('#show-annotations-button').click();

    toolbar.addEventListener('annotation-mode-toggled', e => {
      chrome.test.assertTrue(e.detail);
      chrome.test.succeed();
    });
    toolbar.toggleAnnotation();
  },
  function testEnteringAnnotationsModeDisablesTwoUp() {
    document.body.innerHTML = '';
    const toolbar = document.createElement('viewer-pdf-toolbar-new');
    document.body.appendChild(toolbar);
    chrome.test.assertFalse(toolbar.annotationMode);

    toolbar.toggleAnnotation();
    // This is normally done by the parent in response to the event fired by
    // toggleAnnotation().
    toolbar.annotationMode = true;
    chrome.test.assertTrue(
        toolbar.shadowRoot.querySelector('#two-page-view-button').disabled);
    chrome.test.succeed();
  },
  function testRotateOrTwoUpViewTriggersDialog() {
    document.body.innerHTML = '';
    const toolbar = document.createElement('viewer-pdf-toolbar-new');
    document.body.appendChild(toolbar);
    toolbar.annotationAvailable = true;
    toolbar.pdfAnnotationsEnabled = true;
    toolbar.rotated = false;
    toolbar.twoUpViewEnabled = false;

    testAsync(async () => {
      await waitBeforeNextRender(toolbar);
      chrome.test.assertFalse(toolbar.annotationMode);

      // If rotation is enabled clicking the button shows the dialog.
      toolbar.rotated = true;
      const annotateButton = toolbar.shadowRoot.querySelector('#annotate');
      chrome.test.assertFalse(annotateButton.disabled);
      annotateButton.click();
      await waitBeforeNextRender(toolbar);
      let dialog =
          toolbar.shadowRoot.querySelector('viewer-annotations-mode-dialog');
      chrome.test.assertTrue(dialog.isOpen());

      // Cancel the dialog.
      const whenClosed = eventToPromise('close', dialog);
      dialog.shadowRoot.querySelector('.cancel-button').click();
      chrome.test.assertFalse(dialog.isOpen());
      await whenClosed;

      // If both two up and rotate are enabled, the dialog opens.
      toolbar.twoUpViewEnabled = true;
      chrome.test.assertFalse(annotateButton.disabled);
      annotateButton.click();
      await waitBeforeNextRender(toolbar);
      dialog =
          toolbar.shadowRoot.querySelector('viewer-annotations-mode-dialog');
      chrome.test.assertTrue(dialog.isOpen());

      // When "Edit" is clicked, the toolbar should fire
      // annotation-mode-dialog-confirmed.
      const whenConfirmed =
          eventToPromise('annotation-mode-dialog-confirmed', toolbar);
      dialog.shadowRoot.querySelector('.action-button').click();
      await whenConfirmed;
      chrome.test.assertFalse(dialog.isOpen());
      await waitBeforeNextRender(toolbar);

      // Dialog shows in two up view (un-rotated).
      toolbar.rotated = false;
      chrome.test.assertFalse(annotateButton.disabled);
      annotateButton.click();
      await waitBeforeNextRender(toolbar);
      dialog =
          toolbar.shadowRoot.querySelector('viewer-annotations-mode-dialog');
      chrome.test.assertTrue(dialog.isOpen());
    });
  }
]);
