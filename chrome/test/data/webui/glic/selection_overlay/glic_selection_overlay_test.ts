// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://glic/glic_selection_overlay.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import {GenericAssociatedInterface, GenericAssociatedInterfaceRemote} from '//resources/mojo/mojo/public/mojom/base/generic_pending_associated_receiver.mojom-webui.js';
import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {GlicSelectionOverlayElement} from 'chrome-untrusted://glic/glic_selection_overlay.js';
import type {InlineFulfillmentHostElement} from 'chrome-untrusted://glic/inline_fulfillment_host.js';
import {ScreenshotBitmapBrowserProxyImpl} from 'chrome-untrusted://glic/lens/screenshot_bitmap_browser_proxy.js';
import {SelectionOverlayBaseHandler} from 'chrome-untrusted://glic/lens/selection_overlay_base_handler.js';
import {browserProxyFactory, SelectionOverlayPageHandlerRemote, SuggestedActionIcon} from 'chrome-untrusted://glic/selection_overlay.mojom-webui.js';
import type {SelectionOverlayPageRemote, SuggestedAction, SuggestedActionsListenerRemote} from 'chrome-untrusted://glic/selection_overlay.mojom-webui.js';
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

// Where the prompt and the chips go. The browser's messages are sent through
// the real page interface: a screenshot, then a selected region, then chips.
suite('GlicSelectionOverlayLayout', () => {
  // The prompt keeps this far from the viewport edges and the selection.
  const MARGIN = 16;

  let overlay: GlicSelectionOverlayElement;
  let handler: TestMock<SelectionOverlayPageHandlerRemote>&
      SelectionOverlayPageHandlerRemote;
  let page: SelectionOverlayPageRemote;
  let chipCount = 1;

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

  // Creates the overlay with the given flags and sends it a screenshot.
  async function showOverlay(
      {prompt = false, promptBox = false, dogfoodFeedback = false}) {
    loadTimeData.overrideValues({
      enableSelectionOverlayPrompt: prompt,
      enableSelectionOverlayPromptBox: promptBox,
      enableSelectionOverlayDogfoodFeedback: dogfoodFeedback,
      enableShimmer: false,
    });
    overlay = document.createElement('glic-selection-overlay');
    // Covers the viewport, as in `selection-overlay-app`.
    overlay.style.cssText = 'position: fixed; inset: 0;';
    document.body.appendChild(overlay);
    await overlay.updateComplete;

    // The overlay sizes itself to the viewport once the screenshot renders.
    const screenshotRendered = new Promise<void>(resolve => {
      const observer = new ResizeObserver(() => {
        if (overlay.$.selectionOverlay.offsetHeight > 0) {
          observer.disconnect();
          resolve();
        }
      });
      observer.observe(overlay.$.selectionOverlay);
    });
    // A screenshot the size of the viewport, so the overlay fills the viewport
    // like it does over a tab.
    page.screenshotReceived(fakeScreenshotBitmap(
        Math.round(window.innerWidth * window.devicePixelRatio),
        Math.round(window.innerHeight * window.devicePixelRatio)));
    await screenshotRendered;
  }

  // Selects `region` (normalized, centered at `x`, `y`) and answers the
  // overlay's request for chips with `chipActions`, if any.
  async function select(region: RectF, chipActions: SuggestedAction[] = []) {
    handler.resetResolver('getSuggestedActions');
    page.setPostRegionSelections(
        [{id: '1'.padStart(32, '0'), shape: {rect: region}}]);
    await flushTasks();
    if (chipActions.length > 0) {
      const listener: SuggestedActionsListenerRemote =
          await handler.whenCalled('getSuggestedActions');
      listener.onSuggestedActionsAvailable(chipActions);
      await flushTasks();
    }
    await waitForPromptToSettle();
  }

  // A chip that hands off to the side panel, or does `action`.
  function chip(
      title: string,
      action: SuggestedAction['action'] = {handoff: {}}): SuggestedAction {
    return {
      id: String(++chipCount).padStart(32, '0'),
      title,
      icon: SuggestedActionIcon.kNone,
      action,
    };
  }

  // The overlay re-measures the prompt for a frame or two after it shows or
  // its content changes. Waits until a frame goes by without it moving.
  async function waitForPromptToSettle() {
    let style;
    do {
      style = overlay.floatingPromptStyle;
      await waitAfterNextRender(overlay);
    } while (style !== overlay.floatingPromptStyle);
    // The prompt fades in from a small offset.
    overlay.shadowRoot.getAnimations().forEach(a => a.finish());
  }

  // Returns `region`'s bounds in the viewport.
  function toViewport(region: RectF): DOMRect {
    const overlayRect = overlay.$.selectionOverlay.getBoundingClientRect();
    return new DOMRect(
        overlayRect.left + (region.x - region.width / 2) * overlayRect.width,
        overlayRect.top + (region.y - region.height / 2) * overlayRect.height,
        region.width * overlayRect.width, region.height * overlayRect.height);
  }

  function query(selector: string): HTMLElement {
    const element = overlay.shadowRoot.querySelector<HTMLElement>(selector);
    assertTrue(!!element, selector);
    return element;
  }

  function promptRect(): DOMRect {
    const container = query('#floatingPromptContainer');
    assertFalse(container.hidden);
    return container.getBoundingClientRect();
  }

  function chips(): HTMLElement[] {
    return [...overlay.shadowRoot.querySelectorAll<HTMLElement>(
        '.action-chip')];
  }

  function chipTitles(): string[] {
    return chips().map(
        chip => chip.querySelector('.chip-label')?.textContent?.trim() ?? '');
  }

  function assertNear(expected: number, actual: number, message: string) {
    assertTrue(
        Math.abs(expected - actual) <= 2,
        `${message}: expected ${expected}, got ${actual}`);
  }

  test('NoPromptWhenDisabled', async () => {
    await showOverlay({});
    await select({x: 0.3, y: 0.3, width: 0.1, height: 0.1});

    assertTrue(overlay.$.postSelectionRenderer.hasSelection());
    assertEquals(
        null, overlay.shadowRoot.querySelector('#floatingPromptContainer'));
  });

  test('PromptBoxShowsInputAndStackedChips', async () => {
    await showOverlay({promptBox: true});
    await select(
        {x: 0.3, y: 0.3, width: 0.1, height: 0.1},
        [chip('Ask Gemini'), chip('Summarize')]);

    const input = query('#promptInput') as HTMLInputElement;
    assertEquals('Ask Gemini', input.placeholder);
    // "Ask Gemini" always goes last.
    assertEquals('Summarize,Ask Gemini', chipTitles().join());

    // The chips are stacked below the pill, lined up with it.
    const rect = promptRect();
    const pillRect = query('.searchbox-pill').getBoundingClientRect();
    const [chip0Rect, chip1Rect] = chips().map(c => c.getBoundingClientRect());
    assertTrue(rect.height > 96 && rect.height <= 130, `${rect.height}`);
    assertTrue(chip0Rect!.top >= pillRect.bottom);
    assertTrue(chip1Rect!.top >= chip0Rect!.bottom);
    assertNear(pillRect.left, chip0Rect!.left, 'chip 0 left');
    assertNear(pillRect.left, chip1Rect!.left, 'chip 1 left');
  });

  test('PromptBoxCenteredBelowSelection', async () => {
    const region = {x: 0.3, y: 0.3, width: 0.1, height: 0.1};
    await showOverlay({promptBox: true});
    await select(region);

    const rect = promptRect();
    const selection = toViewport(region);
    assertNear(
        selection.left + selection.width / 2, rect.left + rect.width / 2,
        'center');
    assertNear(selection.bottom + MARGIN, rect.top, 'top');
    assertFalse(overlay.promptAboveSelection);
  });

  test('PromptBoxMovesAboveSelectionNearBottom', async () => {
    const region = {x: 0.5, y: 0.9, width: 0.1, height: 0.1};
    await showOverlay({promptBox: true});
    await select(region, [chip('Summarize'), chip('Ask Gemini')]);

    assertTrue(overlay.promptAboveSelection);
    assertNear(toViewport(region).top - MARGIN, promptRect().bottom, 'bottom');
    // The pill stays closest to the selection, with the first chip next to it.
    const pillRect = query('.searchbox-pill').getBoundingClientRect();
    const [chip0Rect, chip1Rect] = chips().map(c => c.getBoundingClientRect());
    assertTrue(chip0Rect!.bottom <= pillRect.top);
    assertTrue(chip1Rect!.bottom <= chip0Rect!.top);
  });

  test('PromptBoxClampedToViewportRightEdge', async () => {
    await showOverlay({promptBox: true});
    await select({x: 0.95, y: 0.4, width: 0.06, height: 0.1});

    const rect = promptRect();
    assertNear(window.innerWidth - MARGIN, rect.right, 'right');
    assertTrue(rect.left >= 0 && rect.top >= 0);
    assertTrue(rect.bottom <= window.innerHeight);
  });

  test('DogfoodFeedbackButtonBesidePill', async () => {
    await showOverlay({promptBox: true, dogfoodFeedback: true});
    await select({x: 0.3, y: 0.3, width: 0.1, height: 0.1});

    const pillRect = query('.searchbox-pill').getBoundingClientRect();
    const buttonRect = query('#dogfoodFeedbackButton').getBoundingClientRect();
    assertNear(
        (pillRect.top + pillRect.bottom) / 2,
        (buttonRect.top + buttonRect.bottom) / 2, 'center y');
    assertTrue(buttonRect.left >= pillRect.right);
  });

  test('ChipsRowAtSelectionTopRight', async () => {
    const region = {x: 0.3, y: 0.3, width: 0.1, height: 0.1};
    await showOverlay({prompt: true});
    await select(region, [chip('Summarize'), chip('Ask Gemini')]);

    assertEquals(null, overlay.shadowRoot.querySelector('#promptInput'));
    assertEquals('Summarize,Ask Gemini', chipTitles().join());
    // The chips replace the region's close button.
    const renderer = overlay.$.postSelectionRenderer;
    assertTrue(renderer.hideCloseButton);
    const closeButton =
        renderer.shadowRoot!.querySelector<HTMLElement>('.close-button');
    assertTrue(!!closeButton);
    assertEquals('none', getComputedStyle(closeButton).display);

    // The row's bottom-left corner is at the region's top-right corner, and
    // the settings button comes after the chips.
    const rect = promptRect();
    const selection = toViewport(region);
    assertNear(selection.right, rect.left, 'left');
    assertNear(selection.top, rect.bottom, 'bottom');
    const [chip0Rect, chip1Rect] = chips().map(c => c.getBoundingClientRect());
    assertTrue(chip1Rect!.left >= chip0Rect!.right);
    assertTrue(
        query('#settingsButton').getBoundingClientRect().left >=
        chip1Rect!.right);
  });

  test('ChipsRowClampedToViewportRightEdge', async () => {
    const region = {x: 0.95, y: 0.4, width: 0.06, height: 0.1};
    await showOverlay({prompt: true});
    await select(region, [chip('Summarize'), chip('Ask Gemini')]);

    const rect = promptRect();
    assertNear(window.innerWidth - MARGIN, rect.right, 'right');
    assertTrue(rect.left < toViewport(region).right - 2);
  });

  // A card such as Explain replaces the chips row.
  const FAKE_CARD = 'fake_card.js';

  function cardChip(): SuggestedAction {
    return chip('Explain', {inlineFulfillment: {resourceName: FAKE_CARD}});
  }

  function fakeCard(height: number): HTMLElement {
    const card = document.createElement('div');
    card.style.cssText = `height: ${height}px; width: 300px;`;
    return card;
  }

  // Clicks the first chip, which must show the card from `FAKE_CARD`, and
  // waits for `card` to show in place of the chips.
  async function clickCardChip(card: HTMLElement) {
    const host =
        query('#inlineFulfillmentHost') as InlineFulfillmentHostElement;
    host.loaders = new Map([[
      FAKE_CARD,
      () => Promise.resolve({
        create: () => ({
          element: card,
          interfaceName: GenericAssociatedInterface.$interfaceName,
          remote: new GenericAssociatedInterfaceRemote(),
        }),
      }),
    ]]);
    handler.resetResolver('executeSuggestedAction');
    chips()[0]!.click();
    await handler.whenCalled('executeSuggestedAction');
    await waitForPromptToSettle();
    assertEquals(card, host.firstElementChild);
  }

  test('CardKeepsChipsRowTopLeftAndGrowsDownward', async () => {
    await showOverlay({prompt: true});
    await select({x: 0.5, y: 0.5, width: 0.1, height: 0.1}, [cardChip()]);
    const rowRect = promptRect();

    await clickCardChip(fakeCard(200));

    const rect = promptRect();
    assertNear(rowRect.left, rect.left, 'left');
    assertNear(rowRect.top, rect.top, 'top');
    assertTrue(rect.bottom > rowRect.bottom);
  });

  test('GrowingCardStaysInViewport', async () => {
    await showOverlay({prompt: true});
    await select({x: 0.5, y: 0.5, width: 0.1, height: 0.1}, [cardChip()]);
    const card = fakeCard(100);
    await clickCardChip(card);

    // Make the prompt as tall as the space inside the margins. It no longer
    // fits at the selection, so it has to move up.
    const grow = window.innerHeight - 2 * MARGIN - promptRect().height;
    card.style.height = `${card.offsetHeight + grow}px`;
    await waitForPromptToSettle();

    const rect = promptRect();
    assertNear(MARGIN, rect.top, 'top');
    assertNear(window.innerHeight - MARGIN, rect.bottom, 'bottom');
  });

  test('HandlesStayHiddenWithCard', async () => {
    await showOverlay({prompt: true});
    page.setInteractionOptions({
      hideHandles: true,
      disableMultiSelect: true,
      skipIntroAnimation: true,
    });
    await select({x: 0.5, y: 0.5, width: 0.1, height: 0.1}, [cardChip()]);

    await clickCardChip(fakeCard(100));

    // The corners aren't drawn, and their hit boxes are gone, so the region
    // can't be resized.
    const corners =
        overlay.$.postSelectionRenderer.shadowRoot!.querySelector<HTMLElement>(
            '#selectionCorners');
    assertTrue(!!corners);
    const boxes = [...corners.querySelectorAll<HTMLElement>('.corner-hit-box')];
    assertEquals('none', getComputedStyle(corners).backgroundImage);
    assertEquals(4, boxes.length);
    assertTrue(boxes.every(box => getComputedStyle(box).display === 'none'));
  });
});
