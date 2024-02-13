// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotationTool, ViewerInkHostElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {SaveRequestType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {waitFor} from './test_util.js';

window.onerror = e => chrome.test.fail((e as unknown as Error).stack);
window.onunhandledrejection = e => chrome.test.fail(e.reason);

const viewer = document.body.querySelector('pdf-viewer')!;

function animationFrame(): Promise<void> {
  return new Promise(resolve => requestAnimationFrame(() => resolve()));
}

function contentElement(): HTMLElement {
  return viewer.shadowRoot!.elementFromPoint(innerWidth / 2, innerHeight / 2) as
      HTMLElement;
}

function isAnnotationMode(): boolean {
  return viewer.$.toolbar.annotationMode;
}

chrome.test.runTests([
  function testAnnotationsEnabled() {
    const toolbar = viewer.$.toolbar;
    chrome.test.assertTrue(loadTimeData.getBoolean('pdfAnnotationsEnabled'));
    chrome.test.assertTrue(
        toolbar.shadowRoot!.querySelector('#annotate') != null);
    chrome.test.succeed();
  },
  async function testEnterAnnotationMode() {
    chrome.test.assertEq('EMBED', contentElement().tagName);

    // Enter annotation mode.
    viewer.$.toolbar.toggleAnnotation();
    await viewer.loaded;
    chrome.test.assertEq('VIEWER-INK-HOST', contentElement().tagName);
    chrome.test.succeed();
  },
  async function testViewportToCameraConversion() {
    chrome.test.assertTrue(isAnnotationMode());
    const inkHost = contentElement() as ViewerInkHostElement;
    const cameras: drawings.Box[] = [];
    inkHost.getInkApiForTesting().setCamera = (camera: drawings.Box) => {
      cameras.push(camera);
      return Promise.resolve();
    };

    viewer.viewport.setZoom(1);
    viewer.viewport.setZoom(2);
    chrome.test.assertEq(2, cameras.length);

    const scrollingContainer = viewer.$.scroller;
    scrollingContainer.scrollTo(100, 100);
    await animationFrame();

    chrome.test.assertEq(3, cameras.length);

    // When dark/light mode feature is enabled, a border will be applied to the
    // window. See crrev.com/c/3656414 for more details.
    const expectations = [
      {
        top: 2.25,
        dark_light_top: 2.25,
        left: -106.5,
        dark_light_left: -105.75,
        right: 718.5,
        dark_light_right: 717.75,
        bottom: -408.75,
        dark_light_bottom: -408.0,
      },
      {
        top: 2.25,
        dark_light_top: 2.25,
        left: -3.75,
        dark_light_left: -3.75,
        right: 408.75,
        dark_light_right: 408,
        bottom: -203.25,
        dark_light_bottom: -202.875,
      },
      {
        top: -35.25,
        dark_light_top: -35.25,
        left: 33.75,
        dark_light_left: 33.75,
        right: 446.25,
        dark_light_right: 445.5,
        bottom: -240.75,
        dark_light_bottom: -240.375,
      },
    ];

    for (const expectation of expectations) {
      const actual = cameras.shift()!;
      chrome.test.assertTrue(
          actual.top === expectation.top ||
          actual.top === expectation.dark_light_top);
      chrome.test.assertTrue(
          actual.left === expectation.left ||
          actual.left === expectation.dark_light_left);
      chrome.test.assertTrue(
          actual.bottom === expectation.bottom ||
          actual.bottom === expectation.dark_light_bottom);
      chrome.test.assertTrue(
          actual.right === expectation.right ||
          actual.right === expectation.dark_light_right);
    }
    chrome.test.succeed();
  },
  async function testPenOptions() {
    chrome.test.assertTrue(isAnnotationMode());
    const inkHost = contentElement() as ViewerInkHostElement;
    let toolOrNull: AnnotationTool|null = null;
    inkHost.getInkApiForTesting().setAnnotationTool =
        (value: AnnotationTool) => {
          toolOrNull = value;
        };

    // Pen defaults.
    const viewerPdfToolbar = viewer.$.toolbar;
    const viewerAnnotationsBar =
        viewerPdfToolbar.shadowRoot!.querySelector('viewer-annotations-bar')!;
    const pen = viewerAnnotationsBar.$.pen;
    pen.click();
    chrome.test.assertTrue(!!toolOrNull);

    let tool = toolOrNull as AnnotationTool;
    chrome.test.assertEq('pen', tool.tool);
    chrome.test.assertEq(0.1429, tool.size);
    chrome.test.assertEq('#000000', tool.color);


    // Selected size and color.
    const penOptions = viewerAnnotationsBar.shadowRoot!.querySelector(
        '#pen viewer-pen-options')!;
    penOptions.shadowRoot!.querySelector<HTMLElement>(
                              '#sizes [value="1"]')!.click();
    penOptions.shadowRoot!
        .querySelector<HTMLElement>('#colors [value="#00b0ff"]')!.click();
    await animationFrame();
    tool = toolOrNull as AnnotationTool;
    chrome.test.assertEq('pen', tool.tool);
    chrome.test.assertEq(1, tool.size);
    chrome.test.assertEq('#00b0ff', tool.color);


    // Eraser defaults.
    viewerAnnotationsBar.$.eraser.click();
    tool = toolOrNull as AnnotationTool;
    chrome.test.assertEq('eraser', tool.tool);
    chrome.test.assertEq(1, tool.size);
    chrome.test.assertEq(null, tool.color);


    // Pen keeps previous settings.
    pen.click();
    tool = toolOrNull as AnnotationTool;
    chrome.test.assertEq('pen', tool.tool);
    chrome.test.assertEq(1, tool.size);
    chrome.test.assertEq('#00b0ff', tool.color);


    // Highlighter defaults.
    viewerAnnotationsBar.$.highlighter.click();
    tool = toolOrNull as AnnotationTool;
    chrome.test.assertEq('highlighter', tool.tool);
    chrome.test.assertEq(0.7143, tool.size);
    chrome.test.assertEq('#ffbc00', tool.color);


    // Need to expand to use this color.
    const highlighterOptions =
        viewerAnnotationsBar.$.highlighter.querySelector('viewer-pen-options');
    chrome.test.assertTrue(!!highlighterOptions);
    viewerAnnotationsBar.$.highlighter.click();
    const collapsedColor =
        highlighterOptions.shadowRoot!.querySelector<HTMLInputElement>(
            '#colors [value="#d1c4e9"]');
    chrome.test.assertTrue(!!collapsedColor);
    chrome.test.assertTrue(collapsedColor.disabled);
    collapsedColor.click();
    chrome.test.assertEq('#ffbc00', tool.color);

    // Selected size and expanded color.
    highlighterOptions.shadowRoot!
        .querySelector<HTMLElement>('#sizes [value="1"]')!.click();
    highlighterOptions.shadowRoot!
        .querySelector<HTMLElement>('#colors #expand')!.click();
    chrome.test.assertFalse(collapsedColor.disabled);
    collapsedColor.click();

    tool = toolOrNull as AnnotationTool;
    chrome.test.assertEq('highlighter', tool.tool);
    chrome.test.assertEq(1, tool.size);
    chrome.test.assertEq('#d1c4e9', tool.color);
    chrome.test.succeed();
  },
  async function testStrokeUndoRedo() {
    const inkHost = contentElement();
    const viewerPdfToolbar = viewer.$.toolbar;
    const viewerAnnotationsBar =
        viewerPdfToolbar.shadowRoot!.querySelector('viewer-annotations-bar')!;
    const undo = viewerAnnotationsBar.$.undo;
    const redo = viewerAnnotationsBar.$.redo;

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
    chrome.test.succeed();
  },
  async function testPointerEvents() {
    chrome.test.assertTrue(isAnnotationMode());
    const inkHost = contentElement() as ViewerInkHostElement;
    inkHost.resetPenMode();
    const events: PointerEvent[] = [];
    inkHost.getInkApiForTesting().dispatchPointerEvent = (ev: PointerEvent) => {
      events.push(ev);
    };

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

    interface Expectation {
      type: string;
      init: PointerEventInit;
    }

    function checkExpectations(expectations: Expectation[]) {
      chrome.test.assertEq(expectations.length, events.length);
      while (expectations.length) {
        const event = events.shift()!;
        const expectation = expectations.shift()!;
        chrome.test.assertEq(expectation.type, event.type);
        interface IndexableType {
          [key: string]: any;
        }
        for (const key of Object.keys(expectation.init)) {
          chrome.test.assertEq(
              (expectation.init as IndexableType)[key],
              (event as IndexableType)[key]);
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
    chrome.test.succeed();
  },
  async function testTouchPanGestures() {
    // Ensure that we have an out-of-bounds area.
    viewer.viewport.setZoom(0.5);
    chrome.test.assertTrue(isAnnotationMode());
    const inkHost = contentElement() as ViewerInkHostElement;

    function dispatchPointerEvent(type: string, init: PointerEventInit) {
      const pointerEvent = new PointerEvent(type, init);
      inkHost.dispatchEvent(pointerEvent);
      return pointerEvent;
    }

    function dispatchPointerAndTouchEvents(
        type: string, init: PointerEventInit) {
      const pointerEvent = dispatchPointerEvent(type, init);
      let touchPrevented = false;

      // Can't use a real TouchEvent here, since |timestamp| is a read-only
      // property and can't be set to a desired value.
      inkHost.onTouchStart({
        timeStamp: pointerEvent.timeStamp,
        preventDefault() {
          touchPrevented = true;
        },
      } as unknown as TouchEvent);

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

    prevented = dispatchPointerAndTouchEvents('pointerdown', outOfBoundsTouch);
    dispatchPointerEvent('pointerup', outOfBoundsTouch);
    chrome.test.assertFalse(
        prevented, 'out of bounds touch should start gesture');

    prevented = dispatchPointerAndTouchEvents('pointerdown', pen);
    dispatchPointerEvent('pointerup', pen);
    chrome.test.assertTrue(prevented, 'in document pen should prevent default');

    prevented = dispatchPointerAndTouchEvents('pointerdown', outOfBoundsPen);
    dispatchPointerEvent('pointerup', outOfBoundsPen);
    chrome.test.assertFalse(
        prevented, 'out of bounds pen should start gesture');

    chrome.test.assertTrue(
        inkHost.getPenModeForTesting(), 'pen input should switch to pen mode');
    prevented = dispatchPointerAndTouchEvents('pointerdown', touch);
    dispatchPointerEvent('pointerup', touch);
    chrome.test.assertFalse(
        prevented, 'in document touch in pen mode should start gesture');
    chrome.test.succeed();
  },
  async function testExitAnnotationMode() {
    chrome.test.assertTrue(isAnnotationMode());
    // Exit annotation mode.
    viewer.$.toolbar.toggleAnnotation();
    await viewer.loaded;
    chrome.test.assertEq('EMBED', contentElement().tagName);
    chrome.test.succeed();
  },
  async function testSaveAfterAnnotationMode() {
    const saveData = await viewer.getCurrentControllerForTesting()!.save(
        SaveRequestType.EDITED);
    assert(saveData);
    chrome.test.assertTrue(saveData.editModeForTesting!);
    chrome.test.succeed();
  },
]);
