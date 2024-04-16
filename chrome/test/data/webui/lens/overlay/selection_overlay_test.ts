// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/selection_overlay.js';

import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome-untrusted://lens/browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from 'chrome-untrusted://lens/geometry.mojom-webui.js';
import type {CenterRotatedBox} from 'chrome-untrusted://lens/geometry.mojom-webui.js';
import type {LensPageRemote} from 'chrome-untrusted://lens/lens.mojom-webui.js';
import type {OverlayObject} from 'chrome-untrusted://lens/overlay_object.mojom-webui.js';
import type {SelectionOverlayElement} from 'chrome-untrusted://lens/selection_overlay.js';
import {assertDeepEquals, assertEquals, assertNotEquals, assertNull, assertStringContains} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {assertBoxesWithinThreshold, createObject} from '../utils/object_utils.js';
import {simulateClick, simulateDrag} from '../utils/selection_utils.js';
import {createLine, createParagraph, createText, createWord} from '../utils/text_utils.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

suite('SelectionOverlay', function() {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let selectionOverlayElement: SelectionOverlayElement;
  let callbackRouterRemote: LensPageRemote;
  let objects: OverlayObject[];

  setup(() => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    selectionOverlayElement = document.createElement('lens-selection-overlay');
    document.body.appendChild(selectionOverlayElement);
    // Since the size of the Selection Overlay is based on the screenshot which
    // is not loaded in the test, we need to force the overlay to take up the
    // viewport.
    selectionOverlayElement.$.selectionOverlay.style.width = '100%';
    selectionOverlayElement.$.selectionOverlay.style.height = '100%';
    return waitAfterNextRender(selectionOverlayElement);
  });

  // Normalizes the given values to the size of selection overlay.
  function normalizedBox(box: RectF): RectF {
    const boundingRect = selectionOverlayElement.getBoundingClientRect();
    return {
      x: box.x / boundingRect.width,
      y: box.y / boundingRect.height,
      width: box.width / boundingRect.width,
      height: box.height / boundingRect.height,
    };
  }

  function addWords() {
    const text = createText([
      createParagraph([
        createLine([
          createWord(
              'hello', normalizedBox({x: 20, y: 20, width: 30, height: 10})),
          createWord(
              'there', normalizedBox({x: 50, y: 20, width: 50, height: 10})),
          createWord(
              'test', normalizedBox({x: 80, y: 20, width: 30, height: 10})),
        ]),
      ]),
    ]);
    callbackRouterRemote.textReceived(text);
    return flushTasks();
  }

  function addObjects() {
    objects = [
      {x: 80, y: 20, width: 25, height: 10},
      {x: 70, y: 35, width: 20, height: 10},
    ].map((rect, i) => createObject(i.toString(), normalizedBox(rect)));
    callbackRouterRemote.objectsReceived(objects);
    return flushTasks();
  }

  test(
      'verify that starting a drag on a word does not trigger region search',
      async () => {
        await addWords();

        // Drag that starts on a word but finishes on empty space.
        const wordEl = selectionOverlayElement.$.textSelectionLayer
                           .getWordNodesForTesting()[0]!;
        await simulateDrag(
            selectionOverlayElement, {
              x: wordEl.getBoundingClientRect().left + 5,
              y: wordEl.getBoundingClientRect().top + 5,
            },
            {x: 0, y: 0});

        assertEquals(
            0, testBrowserProxy.handler.getCallCount('issueLensRequest'));
      });

  test(
      `verify that starting a drag off a word and continuing onto a word triggers region search`,
      async () => {
        await addWords();

        // Drag that starts off a word but finishes on a word.
        const wordEl = selectionOverlayElement.$.textSelectionLayer
                           .getWordNodesForTesting()[0]!;
        const dragEnd = {
          x: wordEl.getBoundingClientRect().left + 5,
          y: wordEl.getBoundingClientRect().top + 5,
        };
        await simulateDrag(selectionOverlayElement, {x: 0, y: 0}, dragEnd);

        const expectedRect: CenterRotatedBox = {
          box: normalizedBox({
            x: dragEnd.x / 2,
            y: dragEnd.y / 2,
            width: dragEnd.x,
            height: dragEnd.y,
          }),
          rotation: 0,
          coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
        };
        const rect =
            await testBrowserProxy.handler.whenCalled('issueLensRequest');
        assertDeepEquals(expectedRect, rect);
      });

  test(
      'verify region search canvas resizes when selection overlay resizes',
      async () => {
        selectionOverlayElement.style.display = 'block';
        selectionOverlayElement.style.width = '50px';
        selectionOverlayElement.style.height = '50px';
        // Resize observer does not trigger with flushTasks(), so we need to use
        // waitAfterNextRender() instead.
        await waitAfterNextRender(selectionOverlayElement);
        assertEquals(
            50,
            selectionOverlayElement.$.regionSelectionLayer.$
                .regionSelectionCanvas.width);
        assertEquals(
            50,
            selectionOverlayElement.$.regionSelectionLayer.$
                .regionSelectionCanvas.height);

        selectionOverlayElement.style.width = '200px';
        selectionOverlayElement.style.height = '200px';
        await waitAfterNextRender(selectionOverlayElement);
        assertEquals(
            200,
            selectionOverlayElement.$.regionSelectionLayer.$
                .regionSelectionCanvas.width);
        assertEquals(
            200,
            selectionOverlayElement.$.regionSelectionLayer.$
                .regionSelectionCanvas.height);
      });

  test(
      `verify that only objects respond to taps, even when text overlaps`,
      async () => {
        await Promise.all([addWords(), addObjects()]);

        await simulateClick(selectionOverlayElement, {x: 80, y: 20});

        const rect =
            await testBrowserProxy.handler.whenCalled('issueLensRequest');
        assertBoxesWithinThreshold(objects[0]!.geometry.boundingBox, rect);
      });

  test(
      `verify that dragging performs region search, even when an object overlaps`,
      async () => {
        await addObjects();

        // Drag that starts and ends inside the bounding box of an object.
        const objectEl = selectionOverlayElement.$.objectSelectionLayer
                             .getObjectNodesForTesting()[1]!;
        const objectElBoundingBox = objectEl.getBoundingClientRect();
        const dragStart = {
          x: objectElBoundingBox.left + 1,
          y: objectElBoundingBox.top + 1,
        };
        const dragEnd = {
          x: objectElBoundingBox.right - 1,
          y: objectElBoundingBox.bottom - 1,
        };
        await simulateDrag(selectionOverlayElement, dragStart, dragEnd);

        const expectedRect: CenterRotatedBox = {
          box: normalizedBox({
            x: (dragStart.x + dragEnd.x) / 2,
            y: (dragStart.y + dragEnd.y) / 2,
            width: dragEnd.x - dragStart.x,
            height: dragEnd.y - dragStart.y,
          }),
          rotation: 0,
          coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
        };
        const rect =
            await testBrowserProxy.handler.whenCalled('issueLensRequest');
        assertDeepEquals(expectedRect, rect);
      });

  test('verify that region search triggers post selection', async () => {
    await simulateDrag(
        selectionOverlayElement, {x: 50, y: 25}, {x: 300, y: 200});

    const postSelectionStyles =
        selectionOverlayElement.$.postSelectionRenderer.style;
    const parentBoundingRect = selectionOverlayElement.getBoundingClientRect();
    const expectedHeight = 175 / parentBoundingRect.height * 100;
    const expectedWidth = 250 / parentBoundingRect.width * 100;
    const expectedTop = 25 / parentBoundingRect.height * 100;
    const expectedLeft = 50 / parentBoundingRect.width * 100;

    assertEquals(
        `${expectedHeight}%`,
        postSelectionStyles.getPropertyValue('--selection-height'));
    assertEquals(
        `${expectedWidth}%`,
        postSelectionStyles.getPropertyValue('--selection-width'));
    assertEquals(
        `${expectedTop}%`,
        postSelectionStyles.getPropertyValue('--selection-top'));
    assertEquals(
        `${expectedLeft}%`,
        postSelectionStyles.getPropertyValue('--selection-left'));
  });

  test('verify that tapping an object triggers post selection', async () => {
    await addObjects();
    const objectEl = selectionOverlayElement.$.objectSelectionLayer
                         .getObjectNodesForTesting()[1]!;
    const objectBoundingBox = objectEl.getBoundingClientRect();

    await simulateClick(
        selectionOverlayElement,
        {x: objectBoundingBox.left + 2, y: objectBoundingBox.top + 2});

    const postSelectionStyles =
        selectionOverlayElement.$.postSelectionRenderer.style;
    const parentBoundingRect = selectionOverlayElement.getBoundingClientRect();

    // Based on box coordinates of {x: 70, y: 35, width: 20, height: 10},
    const expectedHeight = 10 / parentBoundingRect.height * 100;
    const expectedWidth = 20 / parentBoundingRect.width * 100;
    const expectedTop = 30 / parentBoundingRect.height * 100;
    const expectedLeft = 60 / parentBoundingRect.width * 100;

    // Only look at first 5 digits to account for rounding errors.
    assertStringContains(
        postSelectionStyles.getPropertyValue('--selection-height'),
        expectedHeight.toString().substring(0, 6));
    assertStringContains(
        postSelectionStyles.getPropertyValue('--selection-width'),
        expectedWidth.toString().substring(0, 6));
    assertStringContains(
        postSelectionStyles.getPropertyValue('--selection-top'),
        expectedTop.toString().substring(0, 6));
    assertStringContains(
        postSelectionStyles.getPropertyValue('--selection-left'),
        expectedLeft.toString().substring(0, 6));
  });

  test('verify that resizing renders image with padding', async () => {
    selectionOverlayElement.style.display = 'block';
    selectionOverlayElement.style.width = '50px';
    selectionOverlayElement.style.height = '50px';
    await waitAfterNextRender(selectionOverlayElement);
    assertNotEquals(null, selectionOverlayElement.getAttribute('is-resized'));

    // Verify resizing back no longer renders with padding
    selectionOverlayElement.style.width = '100%';
    selectionOverlayElement.style.height = '100%';
    await waitAfterNextRender(selectionOverlayElement);
    assertNull(selectionOverlayElement.getAttribute('is-resized'));
  });

  test(
      'verify that resizing within threshold does not rerender image',
      async () => {
        selectionOverlayElement.style.display = 'block';
        selectionOverlayElement.style.width =
            `${selectionOverlayElement.getBoundingClientRect().width - 4}px`;
        await waitAfterNextRender(selectionOverlayElement);
        assertNull(selectionOverlayElement.getAttribute('is-resized'));
      });
});
