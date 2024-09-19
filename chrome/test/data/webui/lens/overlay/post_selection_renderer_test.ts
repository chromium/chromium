// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/post_selection_renderer.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import type {LensPageRemote} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import {UserAction} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import type {PostSelectionBoundingBox, PostSelectionRendererElement} from 'chrome-untrusted://lens-overlay/post_selection_renderer.js';
import {CUTOUT_RADIUS_PX, MAX_CORNER_LENGTH_PX, MAX_CORNER_RADIUS_PX, MIN_BOX_SIZE_PX, PERIMETER_SELECTION_PADDING_PX} from 'chrome-untrusted://lens-overlay/post_selection_renderer.js';
import type {GestureEvent} from 'chrome-untrusted://lens-overlay/selection_utils.js';
import {GestureState} from 'chrome-untrusted://lens-overlay/selection_utils.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {assertPixelsWithinThreshold, assertWithinThreshold} from '../utils/object_utils.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

const TEST_WIDTH = 800;
const TEST_HEIGHT = 500;

function normalizeX(x: number): number {
  return x / TEST_WIDTH;
}

function normalizeY(y: number): number {
  return y / TEST_HEIGHT;
}

suite('PostSelectionRenderer', () => {
  let postSelectionRenderer: PostSelectionRendererElement;
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let callbackRouterRemote: LensPageRemote;
  let metrics: MetricsTracker;

  setup(() => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();

    postSelectionRenderer = document.createElement('post-selection-renderer');
    postSelectionRenderer.setSelectionOverlayRectForTesting(
        new DOMRect(0, 0, TEST_WIDTH, TEST_HEIGHT));

    postSelectionRenderer.style.display = 'block';
    postSelectionRenderer.style.width = `${TEST_WIDTH}px`;
    postSelectionRenderer.style.height = `${TEST_HEIGHT}px`;
    metrics = fakeMetricsPrivate();

    document.body.appendChild(postSelectionRenderer);
    return waitAfterNextRender(postSelectionRenderer);
  });

  async function triggerPostSelectionRender(
      boundingBox: PostSelectionBoundingBox): Promise<void> {
    document.dispatchEvent(
        new CustomEvent('render-post-selection', {detail: boundingBox}));
    return waitAfterNextRender(postSelectionRenderer);
  }

  // Drags on a corner by xOffset horizontal pixels and yOffset vertical pixels.
  // The corner to drag on is determined by the top and left variables. Ex. top:
  // false and left: true would click on the bottom left corner.
  function simulateCornerDrag(
      top: boolean, left: boolean, xOffset: number, yOffset: number): void {
    const postSelectionBoundingBox =
        postSelectionRenderer.$.postSelection.getBoundingClientRect();
    const xTarget =
        left ? postSelectionBoundingBox.left : postSelectionBoundingBox.right;
    const yTarget =
        top ? postSelectionBoundingBox.top : postSelectionBoundingBox.bottom;
    simulateDrag(xTarget, yTarget, xOffset, yOffset);
  }

  function simulateDrag(
      xStart: number, yStart: number, xOffset: number, yOffset: number): void {
    const dragGesture: GestureEvent = {
      state: GestureState.DRAGGING,
      startX: xStart,
      startY: yStart,
      clientX: xStart,
      clientY: yStart,
    };

    assertTrue(postSelectionRenderer.handleGestureStart(dragGesture));
    dragGesture.clientX = xStart + xOffset;
    dragGesture.clientY = yStart + yOffset;

    postSelectionRenderer.handleGestureDrag(dragGesture);
    postSelectionRenderer.handleGestureEnd();
  }

  // Verifies the post seleciton is rendered with the given percentage values
  // between 0-1.
  function assertPostSelectionRender(
      expectedLeft: number, expectedTop: number, expectedWidth: number,
      expectedHeight: number): void {
    assertWithinThreshold(
        expectedLeft * 100,
        parseFloat(
            postSelectionRenderer.style.getPropertyValue('--selection-left')));
    assertWithinThreshold(
        expectedTop * 100,
        parseFloat(
            postSelectionRenderer.style.getPropertyValue('--selection-top')));
    assertWithinThreshold(
        expectedWidth * 100,
        parseFloat(
            postSelectionRenderer.style.getPropertyValue('--selection-width')));
    assertWithinThreshold(
        expectedHeight * 100,
        parseFloat(postSelectionRenderer.style.getPropertyValue(
            '--selection-height')));
  }

  // Verifies the selection corners are rendered with the given pixel values.
  function assertPostSelectionRenderCorners(
      expectedCornerLength: number, expectedCornerRadius: number,
      expectedCutoutRadius: number): void {
    assertPixelsWithinThreshold(
        `${expectedCornerLength}px`,

        postSelectionRenderer.style.getPropertyValue(
            '--post-selection-corner-horizontal-length'));
    assertPixelsWithinThreshold(
        `${expectedCornerLength}px`,

        postSelectionRenderer.style.getPropertyValue(
            '--post-selection-corner-vertical-length'));
    assertPixelsWithinThreshold(
        `${expectedCornerRadius}px`,

        postSelectionRenderer.style.getPropertyValue(
            '--post-selection-corner-radius'));
    assertPixelsWithinThreshold(
        `${expectedCutoutRadius}px`,
        postSelectionRenderer.style.getPropertyValue(
            '--post-selection-cutout-corner-radius'));
  }

  // Verifies that a Lens request was issued with the given percentage values
  // between 0-1.
  async function assertLensRegionRequest(
      expectedLeft: number, expectedTop: number, expectedWidth: number,
      expectedHeight: number): Promise<void> {
    await testBrowserProxy.handler.whenCalled('issueLensRegionRequest');
    const rect =
        testBrowserProxy.handler.getArgs('issueLensRegionRequest')[0][0];
    assertWithinThreshold(expectedLeft + expectedWidth / 2, rect.box.x);
    assertWithinThreshold(expectedTop + expectedHeight / 2, rect.box.y);
    assertWithinThreshold(expectedWidth, rect.box.width);
    assertWithinThreshold(expectedHeight, rect.box.height);
  }

  test('PostSelectionUnhides', async () => {
    // Verifies the post selection bounding box unhides after receiving
    // `render-post-selection` event.
    await triggerPostSelectionRender(
        {top: 0, left: 0, width: 0.2, height: 0.2});

    assertTrue(isVisible(postSelectionRenderer.$.postSelection));
  });

  test('PostSelectionRendersCorrectly', async () => {
    // Verifies the post selection bounding box renders correctly after
    // receiving `render-post-selection` event.
    const postSelectionBox = {top: 0.2, left: 0.35, width: 0.5, height: 0.3};
    await triggerPostSelectionRender(postSelectionBox);

    const renderedSelectionBox =
        postSelectionRenderer.$.postSelection.getBoundingClientRect();

    assertEquals(TEST_HEIGHT * postSelectionBox.top, renderedSelectionBox.top);
    assertEquals(TEST_WIDTH * postSelectionBox.left, renderedSelectionBox.left);
    assertEquals(
        TEST_HEIGHT * postSelectionBox.height, renderedSelectionBox.height);
    assertEquals(
        TEST_WIDTH * postSelectionBox.width, renderedSelectionBox.width);
  });

  test('PostSelectionHides', async () => {
    // Verifies the post selection bounding box hides after receiving
    // `render-post-selection` event with invalid dimensions.
    await triggerPostSelectionRender(
        {top: 0, left: 100, height: -1, width: 50});

    assertFalse(isVisible(postSelectionRenderer.$.postSelection));
  });

  test('PostSelectionClearSelection', async () => {
    // Verifies the post selection bounding box hides after clearSelection() is
    // called.
    await triggerPostSelectionRender(
        {top: 0, left: 0, width: 0.2, height: 0.2});
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    postSelectionRenderer.clearSelection();
    await waitAfterNextRender(postSelectionRenderer);
    assertFalse(isVisible(postSelectionRenderer.$.postSelection));
  });

  test('PostSelectionTopLeft', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ true, /*left=*/ true, /*xOffset=*/ 20, /*yOffset=*/ 20);

    const expectedLeft = normalizeX(30);
    const expectedTop = normalizeY(30);
    const expectedWidth = normalizeX(80);
    const expectedHeight = normalizeY(50);

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        MAX_CORNER_LENGTH_PX, MAX_CORNER_RADIUS_PX, CUTOUT_RADIUS_PX);
    assertEquals(1, metrics.count('Lens.Overlay.Overlay.UserAction'));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.UserAction',
            UserAction.kRegionSelectionChange));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.ByInvocationSource.AppMenu.UserAction',
            UserAction.kRegionSelectionChange));
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionTopLeftMIN_BOX_SIZE', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ true, /*left=*/ true, /*xOffset=*/ 200, /*yOffset=*/ 200);

    const expectedLeft = normalizeX(110 - MIN_BOX_SIZE_PX);
    const expectedTop = normalizeY(80 - MIN_BOX_SIZE_PX);
    const expectedWidth = normalizeX(MIN_BOX_SIZE_PX);
    const expectedHeight = normalizeY(MIN_BOX_SIZE_PX);
    const expectedCornerLength = MIN_BOX_SIZE_PX / 2;
    const expectedCornerRadius = MIN_BOX_SIZE_PX / 3;
    const expectedCutoutRadius = 0;

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        expectedCornerLength, expectedCornerRadius, expectedCutoutRadius);
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionTopLeftIntermediateBox', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ true, /*left=*/ true, /*xOffset=*/ 40, /*yOffset=*/ 40);

    const expectedLeft = normalizeX(50);
    const expectedTop = normalizeY(50);
    const expectedWidth = normalizeX(60);
    const expectedHeight = normalizeY(30);
    const expectedCornerLength = 15;
    const expectedCornerRadius = 10;
    const expectedCutoutRadius = 0;

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        expectedCornerLength, expectedCornerRadius, expectedCutoutRadius);
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionTopLeftOutOfBounds', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ true, /*left=*/ true, /*xOffset=*/ -50, /*yOffset=*/ -50);

    const expectedLeft = normalizeX(PERIMETER_SELECTION_PADDING_PX);
    const expectedTop = normalizeY(PERIMETER_SELECTION_PADDING_PX);
    const expectedWidth = normalizeX(100 + 10 - PERIMETER_SELECTION_PADDING_PX);
    const expectedHeight = normalizeY(70 + 10 - PERIMETER_SELECTION_PADDING_PX);

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        MAX_CORNER_LENGTH_PX, MAX_CORNER_RADIUS_PX, CUTOUT_RADIUS_PX);
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionTopRight', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ true, /*left=*/ false, /*xOffset=*/ 20, /*yOffset=*/ 20);

    const expectedLeft = normalizeX(10);
    const expectedTop = normalizeY(30);
    const expectedWidth = normalizeX(120);
    const expectedHeight = normalizeY(50);

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        MAX_CORNER_LENGTH_PX, MAX_CORNER_RADIUS_PX, CUTOUT_RADIUS_PX);
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionTopRightMIN_BOX_SIZE', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ true, /*left=*/ false, /*xOffset=*/ -200, /*yOffset=*/ 200);

    const expectedLeft = normalizeX(10);
    const expectedTop = normalizeY(80 - MIN_BOX_SIZE_PX);
    const expectedWidth = normalizeX(MIN_BOX_SIZE_PX);
    const expectedHeight = normalizeY(MIN_BOX_SIZE_PX);
    const expectedCornerLength = MIN_BOX_SIZE_PX / 2;
    const expectedCornerRadius = MIN_BOX_SIZE_PX / 3;
    const expectedCutoutRadius = 0;

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        expectedCornerLength, expectedCornerRadius, expectedCutoutRadius);
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionTopRightOutOfBounds', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ true, /*left=*/ false, /*xOffset=*/ TEST_WIDTH,
        /*yOffset=*/ -50);

    const expectedLeft = normalizeX(10);
    const expectedTop = normalizeY(PERIMETER_SELECTION_PADDING_PX);
    const expectedWidth =
        normalizeX(TEST_WIDTH - 10 - PERIMETER_SELECTION_PADDING_PX);
    const expectedHeight = normalizeY(70 + 10 - PERIMETER_SELECTION_PADDING_PX);

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        MAX_CORNER_LENGTH_PX, MAX_CORNER_RADIUS_PX, CUTOUT_RADIUS_PX);
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionBottomRight', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ false, /*left=*/ false, /*xOffset=*/ 20, /*yOffset=*/ 20);

    const expectedLeft = normalizeX(10);
    const expectedTop = normalizeY(10);
    const expectedWidth = normalizeX(120);
    const expectedHeight = normalizeY(90);

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        MAX_CORNER_LENGTH_PX, MAX_CORNER_RADIUS_PX, CUTOUT_RADIUS_PX);
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionBottomRightMIN_BOX_SIZE', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ false, /*left=*/ false, /*xOffset=*/ -200, /*yOffset=*/ -200);

    const expectedLeft = normalizeX(10);
    const expectedTop = normalizeY(10);
    const expectedWidth = normalizeX(MIN_BOX_SIZE_PX);
    const expectedHeight = normalizeY(MIN_BOX_SIZE_PX);
    const expectedCornerLength = MIN_BOX_SIZE_PX / 2;
    const expectedCornerRadius = MIN_BOX_SIZE_PX / 3;
    const expectedCutoutRadius = 0;

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        expectedCornerLength, expectedCornerRadius, expectedCutoutRadius);
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionBottomRightOutOfBounds', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ false, /*left=*/ false, /*xOffset=*/ TEST_WIDTH,
        /*yOffset=*/ TEST_HEIGHT);

    const expectedLeft = normalizeX(10);
    const expectedTop = normalizeY(10);
    const expectedWidth =
        normalizeX(TEST_WIDTH - 10 - PERIMETER_SELECTION_PADDING_PX);
    const expectedHeight =
        normalizeY(TEST_HEIGHT - 10 - PERIMETER_SELECTION_PADDING_PX);

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        MAX_CORNER_LENGTH_PX, MAX_CORNER_RADIUS_PX, CUTOUT_RADIUS_PX);
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionBottomLeft', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ false, /*left=*/ true, /*xOffset=*/ 20, /*yOffset=*/ 20);

    const expectedLeft = normalizeX(30);
    const expectedTop = normalizeY(10);
    const expectedWidth = normalizeX(80);
    const expectedHeight = normalizeY(90);

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        MAX_CORNER_LENGTH_PX, MAX_CORNER_RADIUS_PX, CUTOUT_RADIUS_PX);
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionBottomLeftMIN_BOX_SIZE', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ false, /*left=*/ true, /*xOffset=*/ 200, /*yOffset=*/ -200);

    const expectedLeft = normalizeX(110 - MIN_BOX_SIZE_PX);
    const expectedTop = normalizeY(10);
    const expectedWidth = normalizeX(MIN_BOX_SIZE_PX);
    const expectedHeight = normalizeY(MIN_BOX_SIZE_PX);
    const expectedCornerLength = MIN_BOX_SIZE_PX / 2;
    const expectedCornerRadius = MIN_BOX_SIZE_PX / 3;
    const expectedCutoutRadius = 0;

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        expectedCornerLength, expectedCornerRadius, expectedCutoutRadius);
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionBottomLeftOutOfBounds', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    await simulateCornerDrag(
        /*top=*/ false, /*left=*/ true, /*xOffset=*/ -50,
        /*yOffset=*/ TEST_HEIGHT);

    const expectedLeft = normalizeX(PERIMETER_SELECTION_PADDING_PX);
    const expectedTop = normalizeY(10);
    const expectedWidth = normalizeX(100 + 10 - PERIMETER_SELECTION_PADDING_PX);
    const expectedHeight =
        normalizeY(TEST_HEIGHT - 10 - PERIMETER_SELECTION_PADDING_PX);

    assertPostSelectionRender(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
    assertPostSelectionRenderCorners(
        MAX_CORNER_LENGTH_PX, MAX_CORNER_RADIUS_PX, CUTOUT_RADIUS_PX);
    await assertLensRegionRequest(
        expectedLeft, expectedTop, expectedWidth, expectedHeight);
  });

  test('PostSelectionDraggingDisabled', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    const dragGesture: GestureEvent = {
      state: GestureState.DRAGGING,
      startX: 60,
      startY: 45,
      clientX: 60,
      clientY: 45,
    };

    assertFalse(postSelectionRenderer.handleGestureStart(dragGesture));
  });

  test('PostSelectionClearAllSelectionsCallback', async () => {
    await triggerPostSelectionRender({
      top: normalizeY(10),
      left: normalizeX(10),
      width: normalizeX(100),
      height: normalizeY(70),
    });
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));

    callbackRouterRemote.clearAllSelections();
    await waitAfterNextRender(postSelectionRenderer);
    assertFalse(isVisible(postSelectionRenderer.$.postSelection));
  });


  test('PostSelectionSetPostRegionSelectionCallback', async () => {
    assertFalse(isVisible(postSelectionRenderer.$.postSelection));

    callbackRouterRemote.setPostRegionSelection({
      box: {
        x: normalizeX(10),
        y: normalizeY(10),
        width: normalizeX(100),
        height: normalizeY(70),
      },
      rotation: 0.0,
      coordinateType: 1,
    });
    await waitAfterNextRender(postSelectionRenderer);
    assertTrue(isVisible(postSelectionRenderer.$.postSelection));
  });
});
