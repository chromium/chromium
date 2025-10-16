// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/selection_overlay.js';

import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from 'chrome-untrusted://lens-overlay/geometry.mojom-webui.js';
import type {CenterRotatedBox} from 'chrome-untrusted://lens-overlay/geometry.mojom-webui.js';
import type {LensPageRemote} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import {SemanticEvent, UserAction} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import {ContextMenuOption} from 'chrome-untrusted://lens-overlay/metrics_utils.js';
import type {OverlayObject} from 'chrome-untrusted://lens-overlay/overlay_object.mojom-webui.js';
import {ScreenshotBitmapBrowserProxyImpl} from 'chrome-untrusted://lens-overlay/screenshot_bitmap_browser_proxy.js';
import type {SelectedRegionContextMenuData, SelectionOverlayElement} from 'chrome-untrusted://lens-overlay/selection_overlay.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome-untrusted://webui-test/test_util.js';

import {fakeScreenshotBitmap, waitForScreenshotRendered} from '../utils/image_utils.js';
import {assertBoxesWithinThreshold, createObject} from '../utils/object_utils.js';
import {getImageBoundingRect, normalizeBoxInElement, simulateClick, simulateDrag} from '../utils/selection_utils.js';
import {addEmptyTextToPage, addGenericRegionWordsToPage, addGenericWordsToPage, createLine, createParagraph, createText, createWord} from '../utils/text_utils.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

suite('SelectionOverlay', function() {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let selectionOverlayElement: SelectionOverlayElement;
  let callbackRouterRemote: LensPageRemote;
  let objects: OverlayObject[];
  let metrics: MetricsTracker;

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    loadTimeData.overrideValues({
      // Turn off the shimmer. Since the shimmer is resource intensive, turn off
      // to prevent from causing issues in the tests.
      'enableShimmer': false,
      'enableCopyAsImage': true,
      'enableSaveAsImage': true,
    });


    selectionOverlayElement = document.createElement('lens-selection-overlay');
    document.body.appendChild(selectionOverlayElement);
    metrics = fakeMetricsPrivate();

    // Since the size of the Selection Overlay is based on the screenshot which
    // is not loaded in the test, we need to force the overlay to take up the
    // viewport.
    selectionOverlayElement.$.selectionOverlay.style.width = '100%';
    selectionOverlayElement.$.selectionOverlay.style.height = '100%';

    // The first frame triggers our resize handler. Wait another frame for us
    // the changes made by our resize handler to take effect.
    await waitAfterNextRender(selectionOverlayElement);
    return waitAfterNextRender(selectionOverlayElement);
  });

  // Normalizes the given values to the size of selection overlay.
  function normalizedBox(box: RectF): RectF {
    return normalizeBoxInElement(box, selectionOverlayElement);
  }

  async function addEmptyText() {
    const text = createText([]);
    callbackRouterRemote.textReceived(text);
    await waitAfterNextRender(selectionOverlayElement);
  }

  async function addWords() {
    const text = createText([
      createParagraph([
        createLine([
          createWord(
              'hello', normalizedBox({x: 20, y: 20, width: 30, height: 10})),
          createWord(
              'there', normalizedBox({x: 50, y: 20, width: 50, height: 10})),
        ]),
      ]),
      createParagraph([
        createLine([
          createWord(
              'test', normalizedBox({x: 80, y: 20, width: 30, height: 10})),
        ]),
      ]),
    ]);
    callbackRouterRemote.textReceived(text);
    await flushTasks();
    await waitAfterNextRender(selectionOverlayElement);
  }

  async function addObjects() {
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

  async function waitForScreenshotResize(): Promise<void> {
    // The first render triggers the ResizeObserver. The second runs the
    // requestAnimationFrame callback queued by the ResizeObserver.
    await waitAfterNextRender(selectionOverlayElement);
    await waitAfterNextRender(selectionOverlayElement);
  }

  async function dispatchUpdateSelectedRegionContextMenuEvent() {
    const centerRotatedBox: CenterRotatedBox = {
      box: {x: 0.2, y: 0.2, width: 0.4, height: 0.4},
      rotation: 0,
      coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
    };
    selectionOverlayElement.dispatchEvent(
        new CustomEvent<SelectedRegionContextMenuData>(
            'update-selected-region-context-menu', {
              bubbles: true,
              composed: true,
              detail: {
                box: centerRotatedBox,
                selectionStartIndex: 0,
                selectionEndIndex: 1,
                text: 'text',
              },
            }));
    await flushTasks();
  }

  suite('WithoutWordsOrObjects', function() {
    test(
        'verify region search canvas resizes when selection overlay resizes',
        async () => {
          selectionOverlayElement.style.display = 'block';
          selectionOverlayElement.style.width = '50px';
          selectionOverlayElement.style.height = '50px';
          // Resize observer does not trigger with flushTasks(), so we need to
          // use waitAfterNextRender() instead.
          await waitForScreenshotResize();
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
          await waitForScreenshotResize();
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
          // Resize observer does not trigger with flushTasks(), so we need to
          // use waitAfterNextRender() instead.
          await waitForScreenshotResize();
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
          await waitForScreenshotResize();
          assertEquals(
              200,
              selectionOverlayElement.$.objectSelectionLayer.$
                  .objectSelectionCanvas.width);
          assertEquals(
              200,
              selectionOverlayElement.$.objectSelectionLayer.$
                  .objectSelectionCanvas.height);
        });


    test('verify that region search triggers post selection', async () => {
      await simulateDrag(
          selectionOverlayElement, {x: 50, y: 25}, {x: 300, y: 200});

      const postSelectionStyles =
          selectionOverlayElement.$.postSelectionRenderer.style;
      const parentBoundingRect =
          selectionOverlayElement.getBoundingClientRect();
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

    test('verify that region search triggers selected region context menu', async () => {
      await addEmptyText();

      await simulateDrag(
          selectionOverlayElement, {x: 50, y: 25}, {x: 300, y: 200});
      await waitAfterNextRender(selectionOverlayElement);

      assertTrue(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());
      assertFalse(selectionOverlayElement
                      .getShowDetectedTextContextMenuOptionsForTesting());

      // Check that the region context menu options were shown.
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown',
              ContextMenuOption.COPY_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown',
              ContextMenuOption.SAVE_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ContextMenuOption.Shown',
              ContextMenuOption.COPY_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ContextMenuOption.Shown',
              ContextMenuOption.SAVE_AS_IMAGE));

      // Ensure that no other context menu options were shown.
      assertEquals(
          2,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown'));
      assertEquals(2, metrics.count('Lens.Overlay.ContextMenuOption.Shown'));
    });

    test('verify that resizing renders image with padding', async () => {
      // Resize to be the same size as screenshot.
      selectionOverlayElement.style.display = 'block';
      selectionOverlayElement.style.width = '100px';
      selectionOverlayElement.style.height = '100px';

      // ScreenshotBitmapBrowserProxy assumes only one screenshot will be sent.
      // We need to reset it to allow a new screenshot to be fetched.
      ScreenshotBitmapBrowserProxyImpl.setInstance(
          new ScreenshotBitmapBrowserProxyImpl());
      selectionOverlayElement.fetchNewScreenshotForTesting();

      // Send a fake screenshot of size 100x100.
      testBrowserProxy.page.screenshotDataReceived(
          fakeScreenshotBitmap(100, 100), /*isSidePanelOpen=*/ false);
      await waitForScreenshotRendered(selectionOverlayElement);
      await waitForScreenshotResize();

      // Resize to smaller than the screenshot and verify margins.
      selectionOverlayElement.style.width = '90px';
      selectionOverlayElement.style.height = '90px';
      await waitForScreenshotResize();

      // Size should now be 90px - 48px margin.
      let imageSize = selectionOverlayElement.$.backgroundImageCanvas
                          .getBoundingClientRect();
      assertEquals(42, imageSize.width);
      assertEquals(42, imageSize.height);

      // Resize back to same size as screenshot and verify no margins.
      selectionOverlayElement.style.width = '100px';
      selectionOverlayElement.style.height = '100px';
      await waitForScreenshotResize();

      // Size should now be back to fullscreen.
      imageSize = selectionOverlayElement.$.backgroundImageCanvas
                      .getBoundingClientRect();
      assertEquals(100, imageSize.width);
      assertEquals(100, imageSize.height);

      // Increase the device pixel ratio and resize. Since 100 (screenshot size)
      // / 1.5 is 66.666667, this will also test rounding errors in our
      // calculations.
      selectionOverlayElement.style.width = '67px';
      selectionOverlayElement.style.height = '67px';
      window.devicePixelRatio = 1.5;
      await waitForScreenshotResize();

      // Size should now be back to fullscreen.
      imageSize = selectionOverlayElement.$.backgroundImageCanvas
                      .getBoundingClientRect();
      assertEquals(67, imageSize.width);
      assertEquals(67, imageSize.height);
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

    test('context menu is left-aligned if it fits', async () => {
      await addEmptyText();

      await simulateDrag(
          selectionOverlayElement, {x: 50, y: 100}, {x: 30, y: 2000});
      await waitAfterNextRender(selectionOverlayElement);
      assertTrue(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());

      selectionOverlayElement.style.width = '500px';
      selectionOverlayElement.style.height = '500px';
      await waitForScreenshotResize();

      assertStringContains(
          selectionOverlayElement.$.selectedRegionContextMenu.style
              .getPropertyValue('left'),
          '%');
      assertEquals(
          '',
          selectionOverlayElement.$.selectedRegionContextMenu.style
              .getPropertyValue('right'));
    });

    test('context menu is right-aligned if needed', async () => {
      await addEmptyText();

      await simulateDrag(
          selectionOverlayElement, {x: 50, y: 100}, {x: 30, y: 2000});
      await waitAfterNextRender(selectionOverlayElement);
      assertTrue(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());

      selectionOverlayElement.style.width = '150px';
      selectionOverlayElement.style.height = '200px';
      await waitForScreenshotResize();

      assertEquals(
          '',
          selectionOverlayElement.$.selectedRegionContextMenu.style
              .getPropertyValue('left'));
      assertEquals(
          '0px',
          selectionOverlayElement.$.selectedRegionContextMenu.style
              .getPropertyValue('right'));
    });

    test(
        'context menu is constrained on left and right if needed', async () => {
          await addEmptyText();

          await simulateDrag(
              selectionOverlayElement, {x: 50, y: 100}, {x: 30, y: 2000});
          await waitAfterNextRender(selectionOverlayElement);
          assertTrue(selectionOverlayElement
                         .getShowSelectedRegionContextMenuForTesting());

          selectionOverlayElement.style.width = '50px';
          selectionOverlayElement.style.height = '50px';
          await waitForScreenshotResize();

          assertEquals(
              '',
              selectionOverlayElement.$.selectedRegionContextMenu.style
                  .getPropertyValue('left'));
          assertEquals(
              '',
              selectionOverlayElement.$.selectedRegionContextMenu.style
                  .getPropertyValue('right'));
        });

    test('context menu is below region if it fits', async () => {
      await addEmptyText();

      await simulateDrag(
          selectionOverlayElement, {x: 50, y: 100}, {x: 30, y: 300});
      await waitAfterNextRender(selectionOverlayElement);
      assertTrue(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());

      selectionOverlayElement.style.width = '500px';
      selectionOverlayElement.style.height = '500px';
      await waitForScreenshotResize();

      assertStringContains(
          selectionOverlayElement.$.selectedRegionContextMenu.style
              .getPropertyValue('top'),
          '+');
      assertEquals(
          '',
          selectionOverlayElement.$.selectedRegionContextMenu.style
              .getPropertyValue('bottom'));
    });

    test('context menu is above region if needed', async () => {
      await addEmptyText();

      await simulateDrag(
          selectionOverlayElement, {x: 50, y: 500}, {x: 30, y: 2000});
      await waitAfterNextRender(selectionOverlayElement);
      assertTrue(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());

      selectionOverlayElement.style.width = '500px';
      selectionOverlayElement.style.height = '1250px';
      await waitForScreenshotResize();

      assertEquals(
          '',
          selectionOverlayElement.$.selectedRegionContextMenu.style
              .getPropertyValue('top'));
      assertStringContains(
          selectionOverlayElement.$.selectedRegionContextMenu.style
              .getPropertyValue('bottom'),
          '+');
    });

    test('context menu overlaps region if needed', async () => {
      await addEmptyText();

      await simulateDrag(
          selectionOverlayElement, {x: 50, y: 10}, {x: 30, y: 2000});
      await waitAfterNextRender(selectionOverlayElement);
      assertTrue(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());

      selectionOverlayElement.style.width = '500px';
      selectionOverlayElement.style.height = '1250px';
      await waitForScreenshotResize();

      assertEquals(
          '',
          selectionOverlayElement.$.selectedRegionContextMenu.style
              .getPropertyValue('top'));
      assertStringContains(
          selectionOverlayElement.$.selectedRegionContextMenu.style
              .getPropertyValue('bottom'),
          '-');
    });
  });

  suite('WithWords', function() {
    setup(async function() {
      return addGenericWordsToPage(
          callbackRouterRemote, selectionOverlayElement);
    });

    test(
        'verify that adding region words twice sends a end text view event.',
        async () => {
          let semanticEventArgs = await testBrowserProxy.handler.getArgs(
              'recordLensOverlaySemanticEvent');
          // No semantic events should be logged yet.
          assertEquals(0, semanticEventArgs.length);

          await addGenericRegionWordsToPage(
              callbackRouterRemote, selectionOverlayElement);
          assertEquals(1, semanticEventArgs.length);
          assertEquals(
              SemanticEvent.kTextGleamsViewStart, semanticEventArgs[0]);

          await addGenericRegionWordsToPage(
              callbackRouterRemote, selectionOverlayElement);

          semanticEventArgs = await testBrowserProxy.handler.getArgs(
              'recordLensOverlaySemanticEvent');
          assertEquals(3, semanticEventArgs.length);
          assertEquals(SemanticEvent.kTextGleamsViewEnd, semanticEventArgs[1]);
          assertEquals(
              SemanticEvent.kTextGleamsViewStart, semanticEventArgs[2]);
        });

    test('verify that translate in detected text options works', async () => {
      await addGenericRegionWordsToPage(
          callbackRouterRemote, selectionOverlayElement);
      await simulateDrag(selectionOverlayElement, {x: 0, y: 0}, {x: 80, y: 40});
      selectionOverlayElement.handleTranslateDetectedTextForTesting();

      const textQuery = await testBrowserProxy.handler.whenCalled(
          'issueTranslateSelectionRequest');
      assertDeepEquals('hello there test', textQuery);
      assertEquals(
          1, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));
      assertFalse(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());
      assertFalse(
          selectionOverlayElement.getShowSelectedTextContextMenuForTesting());
    });

    test(
        'verify that copy as image in selected region context menu works',
        async () => {
          await simulateDrag(
              selectionOverlayElement, {x: 0, y: 0}, {x: 80, y: 40});

          // Verify that the image and detected text context menu options are
          // shown.
          assertEquals(
              4, metrics.count('Lens.Overlay.ContextMenuOption.Shown'));

          selectionOverlayElement.handleCopyAsImageForTesting();

          await testBrowserProxy.handler.whenCalled('copyImage');

          // Verify context menu hides when an option is selected.
          assertFalse(selectionOverlayElement
                          .getShowSelectedRegionContextMenuForTesting());

          // Verify context menu is restored when selection region is
          // right-clicked.
          await simulateClick(
              selectionOverlayElement, {
                x: 40,
                y: 20,
              },
              /* button = */ 2);

          assertTrue(selectionOverlayElement
                         .getShowSelectedRegionContextMenuForTesting());

          // Verify that the image and detected text context menu options are
          // logged again.
          assertEquals(
              8, metrics.count('Lens.Overlay.ContextMenuOption.Shown'));
        });

    test(
        'verify that save as image in selected region context menu works',
        async () => {
          await simulateDrag(
              selectionOverlayElement, {x: 0, y: 0}, {x: 80, y: 40});
          selectionOverlayElement.handleSaveAsImageForTesting();

          await testBrowserProxy.handler.whenCalled('saveAsImage');

          // Verify context menu hides when an option is selected.
          assertFalse(selectionOverlayElement
                          .getShowSelectedRegionContextMenuForTesting());

          // Verify context menu is restored when selection region is
          // right-clicked.
          await simulateClick(
              selectionOverlayElement, {
                x: 40,
                y: 20,
              },
              /* button = */ 2);

          assertTrue(selectionOverlayElement
                         .getShowSelectedRegionContextMenuForTesting());
        });
  });

  suite('WithObjects', function() {
    setup(async function() {
      return addObjects();
    });

    test(
        `verify that dragging performs region search, even when an object overlaps`,
        async () => {
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

    test('verify that tapping an object triggers post selection', async () => {
      const objectEl = selectionOverlayElement.$.objectSelectionLayer
                           .getObjectNodesForTesting()[1]!;
      const objectBoundingBox = objectEl.getBoundingClientRect();

      await simulateClick(
          selectionOverlayElement,
          {x: objectBoundingBox.left + 2, y: objectBoundingBox.top + 2});

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
        'verify that dragging on post selection over an object does not tap that object',
        async () => {
          // Add the post selection over the words.
          await simulateDrag(
              selectionOverlayElement, {x: 150, y: 150}, {x: 5, y: 5});
          testBrowserProxy.handler.reset();

          // Store the previous post seleciton dimensions.
          let postSelectionStyles =
              selectionOverlayElement.$.postSelectionRenderer.style;
          const oldLeft =
              parseInt(postSelectionStyles.getPropertyValue('--selection-top'));
          const oldTop = parseInt(
              postSelectionStyles.getPropertyValue('--selection-left'));

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
              1,
              testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));

          // Get most recent styles
          postSelectionStyles =
              selectionOverlayElement.$.postSelectionRenderer.style;
          assertNotEquals(
              oldLeft,
              parseInt(
                  postSelectionStyles.getPropertyValue('--selection-left')));
          assertNotEquals(
              oldTop,
              parseInt(
                  postSelectionStyles.getPropertyValue('--selection-top')));
        });
    test(
        `verify that only objects respond to taps, even when post selection overlaps`,
        async () => {
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
              1,
              testBrowserProxy.handler.getCallCount('issueLensObjectRequest'));

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
  });

  suite('WithObjectsAndWords', function() {
    setup(async function() {
      return Promise.all([addWords(), addObjects()]);
    });

    test(
        `verify that post selection corners are draggable over text and
          objects`,
        async () => {
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
              testBrowserProxy.handler.getCallCount(
                  'issueTextSelectionRequest'));
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

    test('SearchboxDisablesTaps', async () => {
      // Emulate the searchbox having focus.
      selectionOverlayElement.setSearchboxFocusForTesting(true);

      // Verify tapping on text does nothing.
      await simulateClick(selectionOverlayElement, {x: 80, y: 20});
      assertEquals(
          0,
          testBrowserProxy.handler.getCallCount('issueTextSelectionRequest'));

      // Verify tapping on object does nothing.
      const objectEl = selectionOverlayElement.$.objectSelectionLayer
                           .getObjectNodesForTesting()[1]!;
      const objectBoundingBox = objectEl.getBoundingClientRect();

      await simulateClick(
          selectionOverlayElement,
          {x: objectBoundingBox.left + 2, y: objectBoundingBox.top + 2});

      assertEquals(
          0, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));

      // Verify tapping for default region does nothing.
      await simulateClick(selectionOverlayElement, {x: 1, y: 1});
      assertEquals(
          0, testBrowserProxy.handler.getCallCount('issueLensRegionRequest'));
    });
  });

  suite('SimplifiedSelection', function() {
    setup(async function() {
      loadTimeData.overrideValues({
        'textReceivedTimeout': 0,
        'copyTextTimeout': 0,
        'translateTextTimeout': 0,
      });

      // Recreate overlay element with new load time data.
      document.body.removeChild(selectionOverlayElement);
      selectionOverlayElement =
          document.createElement('lens-selection-overlay');
      document.body.appendChild(selectionOverlayElement);
      selectionOverlayElement.$.selectionOverlay.style.width = '100%';
      selectionOverlayElement.$.selectionOverlay.style.height = '100%';
      await waitAfterNextRender(selectionOverlayElement);
      return waitAfterNextRender(selectionOverlayElement);
    });

    test('UpdateRegionContextMenuEventDoesNotShow', async () => {
      // Default state of selection overlay.
      assertFalse(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());
      assertEquals(
          undefined,
          selectionOverlayElement
              .getShowDetectedTextContextMenuOptionsForTesting());

      await dispatchUpdateSelectedRegionContextMenuEvent();

      assertFalse(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());
      assertTrue(selectionOverlayElement
                     .getShowDetectedTextContextMenuOptionsForTesting());
    });

    test('SelectedRegionContextMenuAppearsWithNoText', async () => {
      await simulateDrag(
          selectionOverlayElement, {x: 50, y: 25}, {x: 300, y: 200});
      await waitAfterNextRender(selectionOverlayElement);
      await addEmptyText();

      assertTrue(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());
      assertFalse(selectionOverlayElement
                      .getShowDetectedTextContextMenuOptionsForTesting());

      // Check that the region context menu options were shown.
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown',
              ContextMenuOption.COPY_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown',
              ContextMenuOption.SAVE_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ContextMenuOption.Shown',
              ContextMenuOption.COPY_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ContextMenuOption.Shown',
              ContextMenuOption.SAVE_AS_IMAGE));

      // Ensure that no other context menu options were shown.
      assertEquals(
          2,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown'));
      assertEquals(2, metrics.count('Lens.Overlay.ContextMenuOption.Shown'));
    });

    test('SelectedRegionContextMenuAppearsWithEmptyText', async () => {
      await simulateDrag(
          selectionOverlayElement, {x: 50, y: 25}, {x: 300, y: 200});
      await waitAfterNextRender(selectionOverlayElement);
      await addEmptyText();

      assertTrue(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());
      assertFalse(selectionOverlayElement
                      .getShowDetectedTextContextMenuOptionsForTesting());

      // Check that the region context menu options were shown.
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown',
              ContextMenuOption.COPY_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown',
              ContextMenuOption.SAVE_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ContextMenuOption.Shown',
              ContextMenuOption.COPY_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ContextMenuOption.Shown',
              ContextMenuOption.SAVE_AS_IMAGE));

      // Ensure that no other context menu options were shown.
      assertEquals(
          2,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown'));
      assertEquals(2, metrics.count('Lens.Overlay.ContextMenuOption.Shown'));
    });

    test('SelectedRegionContextMenuAppearsWithText', async () => {
      await addGenericWordsToPage(
          callbackRouterRemote, selectionOverlayElement);
      await simulateDrag(selectionOverlayElement, {x: 0, y: 0}, {x: 50, y: 20});
      await waitAfterNextRender(selectionOverlayElement);

      assertTrue(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());
      assertTrue(selectionOverlayElement
                     .getShowDetectedTextContextMenuOptionsForTesting());

      // Check that the region context menu options were shown.
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown',
              ContextMenuOption.COPY_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown',
              ContextMenuOption.SAVE_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown',
              ContextMenuOption.TRANSLATE_TEXT_IN_REGION));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown',
              ContextMenuOption.COPY_TEXT_IN_REGION));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ContextMenuOption.Shown',
              ContextMenuOption.COPY_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ContextMenuOption.Shown',
              ContextMenuOption.SAVE_AS_IMAGE));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ContextMenuOption.Shown',
              ContextMenuOption.TRANSLATE_TEXT_IN_REGION));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.ContextMenuOption.Shown',
              ContextMenuOption.COPY_TEXT_IN_REGION));

      // Ensure that no other context menu options were shown.
      assertEquals(
          4,
          metrics.count(
              'Lens.Overlay.ByInvocationSource.AppMenu.ContextMenuOption.Shown'));
      assertEquals(4, metrics.count('Lens.Overlay.ContextMenuOption.Shown'));
    });

    test(
        'SelectedRegionContextMenuCopyDetectedTextFromFullImageResponse',
        async () => {
          await addGenericWordsToPage(
              callbackRouterRemote, selectionOverlayElement);
          await simulateDrag(
              selectionOverlayElement, {x: 5, y: 15}, {x: 75, y: 25});
          await waitAfterNextRender(selectionOverlayElement);

          assertTrue(selectionOverlayElement
                         .getShowSelectedRegionContextMenuForTesting());
          assertTrue(selectionOverlayElement
                         .getShowDetectedTextContextMenuOptionsForTesting());

          selectionOverlayElement.handleCopyDetectedTextForTesting();
          const textQuery =
              await testBrowserProxy.handler.whenCalled('copyText');
          // Copied text should include newlines.
          assertDeepEquals('hello there\r\ntest', textQuery);

          // Verify context menu hides when an option is selected.
          await waitAfterNextRender(selectionOverlayElement);
          assertFalse(selectionOverlayElement
                          .getShowSelectedTextContextMenuForTesting());
        });

    test(
        'SelectedRegionContextMenuCopyDetectedTextFromRegionResponse',
        async () => {
          await addEmptyTextToPage(callbackRouterRemote);
          await simulateDrag(
              selectionOverlayElement, {x: 5, y: 15}, {x: 75, y: 25});
          await addGenericWordsToPage(
              callbackRouterRemote, selectionOverlayElement);
          await waitAfterNextRender(selectionOverlayElement);

          assertTrue(selectionOverlayElement
                         .getShowSelectedRegionContextMenuForTesting());
          assertTrue(selectionOverlayElement
                         .getShowDetectedTextContextMenuOptionsForTesting());

          selectionOverlayElement.handleCopyDetectedTextForTesting();
          const textQuery =
              await testBrowserProxy.handler.whenCalled('copyText');
          // Copied text should include newlines.
          assertDeepEquals('hello there\r\ntest', textQuery);

          // Verify context menu hides when an option is selected.
          await waitAfterNextRender(selectionOverlayElement);
          assertFalse(selectionOverlayElement
                          .getShowSelectedTextContextMenuForTesting());
        });

    test('NewSelectionClearsDetectedTextOptions', async () => {
      await addGenericWordsToPage(
          callbackRouterRemote, selectionOverlayElement);
      await simulateDrag(
          selectionOverlayElement, {x: 5, y: 15}, {x: 75, y: 25});
      await waitAfterNextRender(selectionOverlayElement);

      assertTrue(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());
      assertTrue(selectionOverlayElement
                     .getShowDetectedTextContextMenuOptionsForTesting());

      await simulateDrag(
          selectionOverlayElement, {x: 50, y: 25}, {x: 300, y: 200});
      await waitAfterNextRender(selectionOverlayElement);
      await addEmptyText();

      assertTrue(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());
      assertFalse(selectionOverlayElement
                      .getShowDetectedTextContextMenuOptionsForTesting());
    });

    test('OnCopyCommandCopiesText', async () => {
      await addEmptyTextToPage(callbackRouterRemote);
      await simulateDrag(
          selectionOverlayElement, {x: 5, y: 15}, {x: 75, y: 25});
      await addGenericWordsToPage(
          callbackRouterRemote, selectionOverlayElement);
      await waitAfterNextRender(selectionOverlayElement);

      loadTimeData.overrideValues({
        shouldCopyAsImage: false,
      });

      callbackRouterRemote.onCopyCommand();
      await flushTasks();

      const copiedWords = await testBrowserProxy.handler.whenCalled('copyText');
      assertFalse(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());
      assertEquals('hello there\r\ntest', copiedWords);
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.Overlay.UserAction', UserAction.kCopyText));
      assertEquals(
          1,
          metrics.count(
              'Lens.Overlay.Overlay.ByInvocationSource.AppMenu.UserAction',
              UserAction.kCopyText));
    });

    test('TextDetectedInRegionEventFired', async () => {
      await addGenericWordsToPage(
          callbackRouterRemote, selectionOverlayElement);
      const textDetectedInRegionEvent =
          eventToPromise('text-found-in-region', document.body);
      await simulateDrag(selectionOverlayElement, {x: 0, y: 0}, {x: 50, y: 20});
      await textDetectedInRegionEvent;
      await waitAfterNextRender(selectionOverlayElement);

      assertTrue(
          selectionOverlayElement.getShowSelectedRegionContextMenuForTesting());
      assertTrue(selectionOverlayElement
                     .getShowDetectedTextContextMenuOptionsForTesting());
    });
  });

  suite('InvocationSourceContextMenuImage', function() {
    setup(async function() {
      loadTimeData.overrideValues({
        invocationSource: 'ContentAreaContextMenuImage',
      });

      // Recreate overlay element with new load time data.
      document.body.removeChild(selectionOverlayElement);
      selectionOverlayElement =
          document.createElement('lens-selection-overlay');
      document.body.appendChild(selectionOverlayElement);
      selectionOverlayElement.$.selectionOverlay.style.width = '100%';
      selectionOverlayElement.$.selectionOverlay.style.height = '100%';
      await waitAfterNextRender(selectionOverlayElement);
      return waitAfterNextRender(selectionOverlayElement);
    });

    test(
        'verify that copy and save as image are initially suppressed',
        async () => {
          await addWords();

          assertTrue(selectionOverlayElement
                         .getSuppressCopyAndSaveAsImageForTesting());

          await simulateDrag(
              selectionOverlayElement, {x: 0, y: 0}, {x: 80, y: 40});

          assertFalse(selectionOverlayElement
                          .getSuppressCopyAndSaveAsImageForTesting());
        });
  });

});
