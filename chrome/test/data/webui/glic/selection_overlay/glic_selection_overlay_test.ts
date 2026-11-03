// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://glic/glic_selection_overlay.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import type {GlicSelectionOverlayElement} from 'chrome-untrusted://glic/glic_selection_overlay.js';
import {ScreenshotBitmapBrowserProxyImpl} from 'chrome-untrusted://glic/lens/screenshot_bitmap_browser_proxy.js';
import {SelectionOverlayBaseHandler} from 'chrome-untrusted://glic/lens/selection_overlay_base_handler.js';
import type {SelectionOverlayPageRemote} from 'chrome-untrusted://glic/selection_overlay.mojom-webui.js';
import {browserProxyFactory, SelectionOverlayPageHandlerRemote, SuggestedActionIcon} from 'chrome-untrusted://glic/selection_overlay.mojom-webui.js';
import type {SuggestedAction} from 'chrome-untrusted://glic/selection_overlay.mojom-webui.js';
import {SelectionOverlayBaseHandlerImpl} from 'chrome-untrusted://glic/selection_overlay_base_handler_impl.js';
import {assertEquals, assertFalse, assertLE, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome-untrusted://webui-test/test_mock.js';

function createSuggestedAction(id: string, title: string): SuggestedAction {
  return {
    id,
    title,
    icon: SuggestedActionIcon.kNone,
    action: {handoff: {}},
  };
}

suite('GlicSelectionOverlay', () => {
  let overlay: GlicSelectionOverlayElement;
  let handler: TestMock<SelectionOverlayPageHandlerRemote>&
      SelectionOverlayPageHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      enableSelectionOverlayPromptBox: true,
      enableShimmer: false,
    });

    handler = TestMock.fromClass(SelectionOverlayPageHandlerRemote);
    browserProxyFactory.setInstance(
        browserProxyFactory.createForTest(handler).instance);
    SelectionOverlayBaseHandler.setInstance(
        new SelectionOverlayBaseHandlerImpl());

    overlay = document.createElement('glic-selection-overlay');
    document.body.appendChild(overlay);
    overlay.showFloatingPrompt = true;
    await overlay.updateComplete;
  });

  test('ClickingSearchboxPillFocusesPromptInput', () => {
    const pill =
        overlay.shadowRoot.querySelector<HTMLElement>('.searchbox-pill')!;
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    assertTrue(!!pill);
    assertTrue(!!input);

    // Ensure input does not start focused for this assertion.
    input.blur();
    assertNotEquals(input, overlay.shadowRoot.activeElement);

    pill.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true, composed: true}));
    assertTrue(overlay.showFloatingPrompt);
    pill.click();

    assertEquals(input, overlay.shadowRoot.activeElement);
    assertEquals(0, handler.getCallCount('dismissOverlay'));
  });

  test('ClickingSparkleIconFocusesPromptInput', () => {
    const sparkleIcon =
        overlay.shadowRoot.querySelector<HTMLElement>('.sparkle-icon')!;
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    assertTrue(!!sparkleIcon);
    assertTrue(!!input);

    input.blur();
    assertNotEquals(input, overlay.shadowRoot.activeElement);

    sparkleIcon.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true, composed: true}));
    assertTrue(overlay.showFloatingPrompt);
    sparkleIcon.click();

    assertEquals(input, overlay.shadowRoot.activeElement);
    assertEquals(0, handler.getCallCount('dismissOverlay'));
  });

  test('ClickingSubmitButtonSubmitsWithoutFocusingInput', async () => {
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    const submitButton =
        overlay.shadowRoot.querySelector<HTMLElement>('#submitPromptButton')!;
    input.value = 'Explain this region';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    await overlay.updateComplete;

    input.blur();
    assertNotEquals(input, overlay.shadowRoot.activeElement);

    submitButton.click();

    assertEquals(1, handler.getCallCount('submitPrompt'));
    assertEquals('Explain this region', handler.getArgs('submitPrompt')[0]);
    assertNotEquals(input, overlay.shadowRoot.activeElement);
  });

  test('SuggestionsOrderBelowAndAboveInputBox', async () => {
    overlay.suggestedActions = [
      createSuggestedAction('1', 'First suggestion'),
      createSuggestedAction('2', 'Second suggestion'),
    ];
    overlay.promptAboveSelection = false;
    await overlay.updateComplete;

    const pill =
        overlay.shadowRoot.querySelector<HTMLElement>('.searchbox-pill')!;
    const chips = Array.from(
        overlay.shadowRoot.querySelectorAll<HTMLElement>('.action-chip'));
    assertEquals(2, chips.length);

    let pillRect = pill.getBoundingClientRect();
    let chip0Rect = chips[0]!.getBoundingClientRect();
    let chip1Rect = chips[1]!.getBoundingClientRect();

    // When input box is below selection, suggestions appear below the input box
    // with the first suggestion closest to the input box.
    assertLE(pillRect.bottom, chip0Rect.top);
    assertLE(chip0Rect.bottom, chip1Rect.top);

    overlay.promptAboveSelection = true;
    await overlay.updateComplete;

    pillRect = pill.getBoundingClientRect();
    chip0Rect = chips[0]!.getBoundingClientRect();
    chip1Rect = chips[1]!.getBoundingClientRect();

    // When input box is above selection, suggestions appear above the input box
    // with the first suggestion closest to the input box (and last at the top).
    assertLE(chip1Rect.bottom, chip0Rect.top);
    assertLE(chip0Rect.bottom, pillRect.top);
  });

  test('PromptAboveSelectionUpdatedFromSelectionPosition', async () => {
    overlay.$.selectionOverlay.style.width = `${window.innerWidth}px`;
    overlay.$.selectionOverlay.style.height = `${window.innerHeight}px`;
    (overlay as unknown as {
      updateSelectionOverlayRect(): void,
    }).updateSelectionOverlayRect();
    await new Promise(resolve => requestAnimationFrame(resolve));
    await new Promise(resolve => requestAnimationFrame(resolve));
    assertTrue(overlay.getBoundingRect().height > 0);

    overlay.selectionElements.postSelectionRenderer.hasSelection = () => true;
    overlay.suggestedActions = [
      createSuggestedAction('1', 'First suggestion'),
      createSuggestedAction('2', 'Second suggestion'),
    ];
    await overlay.updateComplete;

    // Selection near the top: prompt fits below the selection.
    document.dispatchEvent(new CustomEvent('post-selection-updated', {
      detail: {left: 0.1, top: 0.1, width: 0.2, height: 0.2},
    }));
    await overlay.updateComplete;
    assertFalse(overlay.promptAboveSelection);

    // Selection extending to the bottom: prompt is placed above the selection.
    document.dispatchEvent(new CustomEvent('post-selection-updated', {
      detail: {left: 0.1, top: 0.4, width: 0.5, height: 0.58},
    }));
    await overlay.updateComplete;
    assertTrue(overlay.promptAboveSelection);
  });

  test('SuggestionChipWidthAndTwoLineEllipsis', async () => {
    overlay.suggestedActions = [
      createSuggestedAction('1', 'Short'),
      createSuggestedAction(
          '2',
          'A longer suggestion title that wraps onto a second line in the chip'),
      createSuggestedAction(
          '3',
          'A very long suggestion title that definitely exceeds two lines ' +
              'and therefore is ellipsised at the end of the second line ' +
              'instead of growing to three or more lines in the floating prompt'),
    ];
    await overlay.updateComplete;

    const pill =
        overlay.shadowRoot.querySelector<HTMLElement>('.searchbox-pill')!;
    const chips = Array.from(
        overlay.shadowRoot.querySelectorAll<HTMLElement>('.action-chip'));
    assertEquals(3, chips.length);

    const pillRect = pill.getBoundingClientRect();
    const chip0Rect = chips[0]!.getBoundingClientRect();
    const chip1Rect = chips[1]!.getBoundingClientRect();
    const chip2Rect = chips[2]!.getBoundingClientRect();

    // No chip can exceed the width of the input box.
    assertLE(chip0Rect.width, pillRect.width);
    assertLE(chip1Rect.width, pillRect.width);
    assertLE(chip2Rect.width, pillRect.width);
    assertEquals(pillRect.width, chip1Rect.width);
    assertEquals(pillRect.width, chip2Rect.width);

    // Single-line chip is 32px tall; two-line chip is 52px tall; 3+ line text
    // is clamped and ellipsised at the end of the second line (also 52px tall).
    assertEquals(32, Math.round(chip0Rect.height));
    assertEquals(52, Math.round(chip1Rect.height));
    assertEquals(52, Math.round(chip2Rect.height));

    const label2 = chips[2]!.querySelector<HTMLElement>('.chip-label')!;
    assertEquals('2', getComputedStyle(label2).webkitLineClamp);
  });
});

function fakeScreenshotBitmap(
    width: number, height: number): BitmapMappedFromTrustedProcess {
  return {
    imageInfo: {
      width,
      height,
      colorType: 0,
      alphaType: 0,
      colorTransferFunction: [],
      colorToXyzMatrix: [],
    },
    pixelData: {bytes: Array(4 * width * height).fill(0)} as BigBuffer,
    uNUSEDRowBytes: BigInt(0),
  };
}

function pointerEvent(type: string, x: number, y: number): PointerEvent {
  return new PointerEvent(type, {
    pointerId: 1,
    bubbles: true,
    composed: true,
    button: 0,
    clientX: x,
    clientY: y,
    isPrimary: true,
  });
}

// Region ids are tokens: 32 hex digits.
const REGION_ID = '0123456789ABCDEF0123456789ABCDEF';

suite('GlicSelectionOverlayClick', () => {
  let overlay: GlicSelectionOverlayElement;
  let handler: TestMock<SelectionOverlayPageHandlerRemote>&
      SelectionOverlayPageHandlerRemote;
  let page: SelectionOverlayPageRemote;

  // Converts a point given as a fraction of the overlay to client coordinates.
  function toClient(x: number, y: number): {x: number, y: number} {
    const rect = overlay.getBoundingRect();
    return {x: rect.left + x * rect.width, y: rect.top + y * rect.height};
  }

  async function click(x: number, y: number) {
    const point = toClient(x, y);
    overlay.dispatchEvent(pointerEvent('pointerdown', point.x, point.y));
    overlay.dispatchEvent(pointerEvent('pointerup', point.x, point.y));
    await flushTasks();
  }

  async function drag(fromX: number, fromY: number, toX: number, toY: number) {
    const from = toClient(fromX, fromY);
    const to = toClient(toX, toY);
    overlay.dispatchEvent(pointerEvent('pointerdown', from.x, from.y));
    overlay.dispatchEvent(pointerEvent('pointermove', to.x, to.y));
    // Pointer moves are handled once per frame.
    await waitAfterNextRender(overlay);
    overlay.dispatchEvent(pointerEvent('pointerup', to.x, to.y));
    await flushTasks();
  }

  // Answers as the browser does once it holds the region with `id`.
  async function setRegion(id: string) {
    page.setPostRegionSelections(
        [{id, shape: {rect: {x: 0.2, y: 0.2, width: 0.2, height: 0.2}}}]);
    await page.$.flushForTesting();
    await overlay.updateComplete;
  }

  async function showOverlay(enablePromptBox: boolean) {
    loadTimeData.overrideValues({
      enableSelectionOverlayPromptBox: enablePromptBox,
      enableShimmer: false,
    });
    overlay = document.createElement('glic-selection-overlay');
    // The overlay fills its parent. Give it the viewport, as in the browser,
    // or its size keeps changing and drags land in the wrong place.
    overlay.style.position = 'fixed';
    overlay.style.inset = '0';
    document.body.appendChild(overlay);
    page.screenshotReceived(fakeScreenshotBitmap(100, 100));
    await page.$.flushForTesting();
    while (overlay.getBoundingRect().width === 0 ||
           overlay.getBoundingRect().height === 0) {
      await waitAfterNextRender(overlay);
    }
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(SelectionOverlayPageHandlerRemote);
    const {instance, remote} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    page = remote;
    SelectionOverlayBaseHandler.setInstance(
        new SelectionOverlayBaseHandlerImpl());
    ScreenshotBitmapBrowserProxyImpl.setInstance(
        new ScreenshotBitmapBrowserProxyImpl());
  });

  test('ClickWithoutRegionDoesNothing', async () => {
    await showOverlay(/*enablePromptBox=*/ true);

    await click(0.5, 0.5);

    assertEquals(0, handler.getCallCount('adjustRegion'));
    assertEquals(0, handler.getCallCount('dismissOverlay'));
  });

  test('ClickWithRegionDismisses', async () => {
    await showOverlay(/*enablePromptBox=*/ true);
    await setRegion(REGION_ID);

    await click(0.8, 0.8);

    assertEquals(0, handler.getCallCount('adjustRegion'));
    assertEquals(1, handler.getCallCount('dismissOverlay'));
  });

  test('DragAddsRegion', async () => {
    await showOverlay(/*enablePromptBox=*/ true);
    await setRegion(REGION_ID);

    await drag(0.5, 0.5, 0.7, 0.7);

    assertEquals(1, handler.getCallCount('adjustRegion'));
    assertNotEquals(REGION_ID, handler.getArgs('adjustRegion')[0][0].id);
    assertEquals(0, handler.getCallCount('dismissOverlay'));
  });

  test('ClickSelectsAreaWithoutPromptBox', async () => {
    await showOverlay(/*enablePromptBox=*/ false);

    await click(0.5, 0.5);

    assertEquals(1, handler.getCallCount('adjustRegion'));
    assertEquals(0, handler.getCallCount('dismissOverlay'));
  });
});
