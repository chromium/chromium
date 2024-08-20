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
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertNotEquals, assertNull, assertStringContains} from 'chrome-untrusted://webui-test/chai_assert.js';
// <if expr="not is_linux">
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
// </if>
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {assertBoxesWithinThreshold, createObject} from '../utils/object_utils.js';
import {getImageBoundingRect, simulateClick, simulateDrag} from '../utils/selection_utils.js';
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

    // Turn off the shimmer. Since the shimmer is resource intensive, turn off
    // to prevent from causing issues in the tests.
    loadTimeData.overrideValues({'enableShimmer': false});

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
    objects =
        [
          {x: 80, y: 20, width: 25, height: 10},
          {x: 70, y: 35, width: 20, height: 10},
        ]
            .map(
                (rect, i) => createObject(
                    i.toString(), normalizedBox(rect), /*isMaskClick=*/ false));
    callbackRouterRemote.objectsReceived(objects);
    return flushTasks();
  }

  async function verifyRegionRequest(
      expectedRegion: CenterRotatedBox, expectedIsClick: boolean) {
    await testBrowserProxy.handler.whenCalled('issueLensRegionRequest');
    const requestRegion =
        testBrowserProxy.handler.getArgs('issueLensRegionRequest')[0][0];
    const isClick =
        testBrowserProxy.handler.getArgs('issueLensRegionRequest')[0][1];
    assertBoxesWithinThreshold(expectedRegion, requestRegion);
    assertEquals(expectedIsClick, isClick);
  }

  // <if expr="not chromeos_lacros">
  test(
      'verify that starting a drag on a word does not trigger region search',
      async () => {
        await addWords();

        // Drag that starts on a word but finishes on empty space.
        const wordEl = selectionOverlayElement.$.textSelectionLayer
                           .getWordNodesForTesting()[0]!;
        await simulateDrag(
            selectionOverlayElement, {
              x: wordEl.getBoundingClientRect().left + 15,
              y: wordEl.getBoundingClientRect().top + 5,
            },
            {x: 0, y: 0});

        const textQuery = await testBrowserProxy.handler.whenCalled(
            'issueTextSelectionRequest');
        assertDeepEquals('hello', textQuery);
        assertEquals(
            0, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));
      });
  // </if>

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
        verifyRegionRequest(expectedRect, /*expectedIsClick=*/ false);
        assertEquals(
            0,
            testBrowserProxy.handler.getCallCount('issueTextSelectionRequest'));
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
      'verify object selection canvas resizes when selection overlay resizes',
      async () => {
        selectionOverlayElement.style.display = 'block';
        selectionOverlayElement.style.width = '50px';
        selectionOverlayElement.style.height = '50px';
        // Resize observer does not trigger with flushTasks(), so we need to use
        // waitAfterNextRender() instead.
        await waitAfterNextRender(selectionOverlayElement);
        assertEquals(
            50,
            selectionOverlayElement.$.objectSelectionLayer.$
                .objectSelectionCanvas.width);
        assertEquals(
            50,
            selectionOverlayElement.$.objectSelectionLayer.$
                .objectSelectionCanvas.height);

        selectionOverlayElement.style.width = '200px';
        selectionOverlayElement.style.height = '200px';
        await waitAfterNextRender(selectionOverlayElement);
        assertEquals(
            200,
            selectionOverlayElement.$.objectSelectionLayer.$
                .objectSelectionCanvas.width);
        assertEquals(
            200,
            selectionOverlayElement.$.objectSelectionLayer.$
                .objectSelectionCanvas.height);
      });

    test(
      `verify that text respond to taps, even when an object is underneath`,
      async () => {
        await Promise.all([addWords(), addObjects()]);

        await simulateClick(selectionOverlayElement, {x: 80, y: 20});

        const textQuery =
            await testBrowserProxy.handler.whenCalled('issueTextSelectionRequest');
        assertDeepEquals('test', textQuery);
        assertEquals(
            0, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));
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
        verifyRegionRequest(expectedRect, /*expectedIsClick=*/ false);
      });

  // <if expr="not chromeos_lacros and not is_linux">
  test(
      'verify that region search over text triggers detected text context menu',
      async () => {
        await addWords();

        await simulateDrag(
            selectionOverlayElement, {x: 51, y: 10}, {x: 80, y: 40});

        assertEquals(
            1, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));
        assertEquals(
            0,
            testBrowserProxy.handler.getCallCount('issueTextSelectionRequest'));
        assertTrue(
            selectionOverlayElement.getShowDetectedTextContextMenuForTesting());

        testBrowserProxy.handler.reset();
        selectionOverlayElement.handleSelectTextForTesting();

        const textQuery = await testBrowserProxy.handler.whenCalled(
            'issueTextSelectionRequest');
        assertDeepEquals('there test', textQuery);
        assertEquals(
            0, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));
      });

  test(
      `verify that adding text after region selection triggers detected text context menu`,
      async () => {
        callbackRouterRemote.setPostRegionSelection({
          box: normalizedBox({x: 65, y: 25, width: 30, height: 30}),
          rotation: 0.0,
          coordinateType: 1,
        });

        await addWords();

        assertEquals(
            0, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));
        assertEquals(
            0,
            testBrowserProxy.handler.getCallCount('issueTextSelectionRequest'));
        assertTrue(
            selectionOverlayElement.getShowDetectedTextContextMenuForTesting());

        testBrowserProxy.handler.reset();
        selectionOverlayElement.handleSelectTextForTesting();

        const textQuery = await testBrowserProxy.handler.whenCalled(
            'issueTextSelectionRequest');
        assertDeepEquals('there test', textQuery);
        assertEquals(
            0, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));
      });

  test(
      'verify that select text in detected text context menu works',
      async () => {
        await addWords();

        await simulateDrag(
            selectionOverlayElement, {x: 51, y: 10}, {x: 80, y: 40});
        selectionOverlayElement.handleSelectTextForTesting();

        const textQuery = await testBrowserProxy.handler.whenCalled(
            'issueTextSelectionRequest');
        assertDeepEquals('there test', textQuery);
        assertEquals(
            1, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));
        assertFalse(
            selectionOverlayElement.getShowDetectedTextContextMenuForTesting());
      });

  test(
      'verify that translate in detected text context menu works', async () => {
        await addWords();

        await simulateDrag(
            selectionOverlayElement, {x: 51, y: 10}, {x: 80, y: 40});
        selectionOverlayElement.handleTranslateDetectedTextForTesting();

        const textQuery = await testBrowserProxy.handler.whenCalled(
            'issueTranslateSelectionRequest');
        assertDeepEquals('there test', textQuery);
        assertEquals(
            1, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));
        assertFalse(
            selectionOverlayElement.getShowDetectedTextContextMenuForTesting());
        assertFalse(
            selectionOverlayElement.getShowSelectedTextContextMenuForTesting());
      });
  // </if>

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

  // <if expr="not is_linux">
  test('verify that you can drag text over post selection', async () => {
    // Add the words
    await addWords();
    // Add the post selection over the words.
    await simulateDrag(selectionOverlayElement, {x: 150, y: 150}, {x: 5, y: 5});
    testBrowserProxy.handler.reset();

    // Drag that starts on a word and post selection.
    const wordEl = selectionOverlayElement.$.textSelectionLayer
                       .getWordNodesForTesting()[1]!;
    const wordElBoundingBox = wordEl.getBoundingClientRect();
    await simulateDrag(
        selectionOverlayElement, {
          x: wordElBoundingBox.left + (wordElBoundingBox.width / 3),
          y: wordElBoundingBox.top + (wordElBoundingBox.height / 2),
        },
        {
          x: wordElBoundingBox.right,
          y: wordElBoundingBox.bottom,
        });

    const textQuery =
        await testBrowserProxy.handler.whenCalled('issueTextSelectionRequest');
    assertDeepEquals('there test', textQuery);
    assertEquals(
        0, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));
  });

  test('verify that copy in selected text context menu works', async () => {
    // Add the words
    await addWords();
    testBrowserProxy.handler.reset();

    // Drag that starts on a word.
    const wordEl = selectionOverlayElement.$.textSelectionLayer
                       .getWordNodesForTesting()[1]!;
    const wordElBoundingBox = wordEl.getBoundingClientRect();
    await simulateDrag(
        selectionOverlayElement, {
          x: wordElBoundingBox.left + (wordElBoundingBox.width / 3),
          y: wordElBoundingBox.top + (wordElBoundingBox.height / 2),
        },
        {
          x: wordElBoundingBox.right,
          y: wordElBoundingBox.bottom,
        });

    assertFalse(
        selectionOverlayElement.getShowDetectedTextContextMenuForTesting());
    assertTrue(
        selectionOverlayElement.getShowSelectedTextContextMenuForTesting());

    selectionOverlayElement.handleCopyForTesting();
    const textQuery = await testBrowserProxy.handler.whenCalled('copyText');
    assertDeepEquals('there test', textQuery);

    // Verify context menu hides when an option is selected.
    assertFalse(
        selectionOverlayElement.getShowSelectedTextContextMenuForTesting());

    // Verify context menu is restored when a selected word is right-clicked.
    await simulateClick(
        selectionOverlayElement, {
          x: wordElBoundingBox.left + (wordElBoundingBox.width / 2),
          y: wordElBoundingBox.top + (wordElBoundingBox.height / 2),
        },
        /* button = */ 2);

    assertTrue(
        selectionOverlayElement.getShowSelectedTextContextMenuForTesting());
  });

  test(
      'verify that translate in selected text context menu works', async () => {
        // Add the words
        await addWords();
        testBrowserProxy.handler.reset();

        // Drag that starts on a word.
        const wordEl = selectionOverlayElement.$.textSelectionLayer
                           .getWordNodesForTesting()[1]!;
        const wordElBoundingBox = wordEl.getBoundingClientRect();
        await simulateDrag(
            selectionOverlayElement, {
              x: wordElBoundingBox.left + (wordElBoundingBox.width / 3),
              y: wordElBoundingBox.top + (wordElBoundingBox.height / 2),
            },
            {
              x: wordElBoundingBox.right,
              y: wordElBoundingBox.bottom,
            });

        assertFalse(
            selectionOverlayElement.getShowDetectedTextContextMenuForTesting());
        assertTrue(
            selectionOverlayElement.getShowSelectedTextContextMenuForTesting());


        selectionOverlayElement.handleTranslateForTesting();
        const textQuery = await testBrowserProxy.handler.whenCalled(
            'issueTranslateSelectionRequest');
        assertDeepEquals('there test', textQuery);

        // Verify context menu hides when an option is selected.
        assertFalse(
            selectionOverlayElement.getShowSelectedTextContextMenuForTesting());

        // Verify context menu is restored when a selected word is
        // right-clicked.
        await simulateClick(
            selectionOverlayElement, {
              x: wordElBoundingBox.left + (wordElBoundingBox.width / 2),
              y: wordElBoundingBox.top + (wordElBoundingBox.height / 2),
            },
            /* button = */ 2);

        assertTrue(
            selectionOverlayElement.getShowSelectedTextContextMenuForTesting());
      });
  // </if>

  test(
      'verify that dragging on post selection over an object does not tap that object',
      async () => {
        // Add the objects
        await addObjects();
        // Add the post selection over the words.
        await simulateDrag(
            selectionOverlayElement, {x: 150, y: 150}, {x: 5, y: 5});
        testBrowserProxy.handler.reset();

        // Store the previous post seleciton dimensions.
        let postSelectionStyles =
            selectionOverlayElement.$.postSelectionRenderer.style;
        const oldLeft =
            parseInt(postSelectionStyles.getPropertyValue('--selection-top'));
        const oldTop =
            parseInt(postSelectionStyles.getPropertyValue('--selection-left'));

        // Drag that starts on an object
        const objectEl = selectionOverlayElement.$.objectSelectionLayer
                             .getObjectNodesForTesting()[1]!;
        const objectBoundingBox = objectEl.getBoundingClientRect();
        await simulateDrag(
            selectionOverlayElement, {
              x: objectBoundingBox.left + (objectBoundingBox.width / 2),
              y: objectBoundingBox.top + (objectBoundingBox.height / 2),
            },
            {
              x: 100,
              y: 100,
            });
        // Should only be called once from post selection adjustment and not
        // object tap.
        assertEquals(
            1, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));

        // Get most recent styles
        postSelectionStyles =
            selectionOverlayElement.$.postSelectionRenderer.style;
        assertNotEquals(
            oldLeft,
            parseInt(postSelectionStyles.getPropertyValue('--selection-left')));
        assertNotEquals(
            oldTop,
            parseInt(postSelectionStyles.getPropertyValue('--selection-top')));
      });
  test(
      `verify that only objects respond to taps, even when post selection overlaps`,
      async () => {
        // Add the objects
        await addObjects();
        // Add the post selection over the words.
        await simulateDrag(
            selectionOverlayElement, {x: 150, y: 150}, {x: 5, y: 5});
        testBrowserProxy.handler.reset();

        // Click on an object behind post selection
        const objectEl = selectionOverlayElement.$.objectSelectionLayer
                             .getObjectNodesForTesting()[1]!;
        const objectBoundingBox = objectEl.getBoundingClientRect();
        await simulateClick(selectionOverlayElement, {
          x: objectBoundingBox.left + (objectBoundingBox.width / 2),
          y: objectBoundingBox.top + (objectBoundingBox.height / 2),
        });

        // Should only be called once from post selection adjustment and not
        // object tap.
        assertEquals(
            1, testBrowserProxy.handler.getCallCount('issueLensObjectRequest'));

        // Verify tap triggered new post selection
        const postSelectionStyles =
            selectionOverlayElement.$.postSelectionRenderer.style;
        const parentBoundingRect =
            selectionOverlayElement.getBoundingClientRect();

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

  test(
      `verify that post selection corners are draggable over text and objects`,
      async () => {
        await Promise.all([addWords(), addObjects()]);
        // Add the post selection to have top left corner overlap with text
        // and objects
        await simulateDrag(
            selectionOverlayElement, {x: 10, y: 150}, {x: 80, y: 20});
        testBrowserProxy.handler.reset();

        // Start drag on word corner over word and object
        await simulateDrag(
            selectionOverlayElement, {x: 85, y: 25}, {x: 100, y: 50});

        // No text request was triggered
        assertEquals(
            0,
            testBrowserProxy.handler.getCallCount('issueTextSelectionRequest'));
        // Lens request for the new region
        const expectedRect: CenterRotatedBox = {
          box: normalizedBox({
            x: 55,
            y: 100,
            width: 90,
            height: 100,
          }),
          rotation: 0,
          coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
        };
        verifyRegionRequest(expectedRect, /*expectedIsClick=*/ false);
      });
  test('verify that completing a drag calls closeSearchBubble', async () => {
    const imageBounds = getImageBoundingRect(selectionOverlayElement);
    const startPointInsideOverlay = {
      x: imageBounds.left + 10,
      y: imageBounds.top + 10,
    };
    const endPointAboveOverlay = {
      x: imageBounds.left + 100,
      y: imageBounds.top - 30,
    };

    await simulateDrag(
        selectionOverlayElement, startPointInsideOverlay, endPointAboveOverlay);

    await testBrowserProxy.handler.whenCalled('closeSearchBubble');
    assertEquals(1, testBrowserProxy.handler.getCallCount('closeSearchBubble'));
  });
  test(`verify that a tap calls closeSearchBubble`, async () => {
    const imageBounds = getImageBoundingRect(selectionOverlayElement);
    await simulateClick(
        selectionOverlayElement,
        {x: imageBounds.left + 10, y: imageBounds.top + 10});

    await testBrowserProxy.handler.whenCalled('closeSearchBubble');
    assertEquals(1, testBrowserProxy.handler.getCallCount('closeSearchBubble'));
  });
  test(
      'verify that completing a drag calls closePreselectionBubble',
      async () => {
        const imageBounds = getImageBoundingRect(selectionOverlayElement);
        const startPointInsideOverlay = {
          x: imageBounds.left + 10,
          y: imageBounds.top + 10,
        };
        const endPointAboveOverlay = {
          x: imageBounds.left + 100,
          y: imageBounds.top - 30,
        };

        await simulateDrag(
            selectionOverlayElement, startPointInsideOverlay,
            endPointAboveOverlay);

        await testBrowserProxy.handler.whenCalled('closePreselectionBubble');
        assertEquals(
            1,
            testBrowserProxy.handler.getCallCount('closePreselectionBubble'));
      });
  test(`verify that a tap calls closePreselectionBubble`, async () => {
    const imageBounds = getImageBoundingRect(selectionOverlayElement);
    await simulateClick(
        selectionOverlayElement,
        {x: imageBounds.left + 10, y: imageBounds.top + 10});

    await testBrowserProxy.handler.whenCalled('closePreselectionBubble');
    assertEquals(
        1, testBrowserProxy.handler.getCallCount('closePreselectionBubble'));
  });
});
