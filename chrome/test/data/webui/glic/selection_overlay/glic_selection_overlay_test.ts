// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import {GenericAssociatedInterface, GenericAssociatedInterfaceRemote} from '//resources/mojo/mojo/public/mojom/base/generic_pending_associated_receiver.mojom-webui.js';
import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {browserProxyFactory, ScreenshotBitmapBrowserProxyImpl, SelectionOverlayBaseHandler, SelectionOverlayBaseHandlerImpl, SelectionOverlayPageHandlerRemote, SuggestedActionIcon} from 'chrome-untrusted://glic/selection_overlay.js';
import type {GlicSelectionOverlayElement, InlineFulfillmentHostElement, SelectionOverlayPageRemote, SuggestedAction, SuggestedActionsListenerRemote} from 'chrome-untrusted://glic/selection_overlay.js';
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
  let page: SelectionOverlayPageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      enableSelectionOverlayPromptBox: true,
      enableShimmer: false,
    });

    handler = TestMock.fromClass(SelectionOverlayPageHandlerRemote);
    const {instance, remote} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    page = remote;
    SelectionOverlayBaseHandler.setInstance(
        new SelectionOverlayBaseHandlerImpl());

    overlay = document.createElement('glic-selection-overlay');
    document.body.appendChild(overlay);
    overlay.showFloatingPrompt = true;
    await overlay.updateComplete;
  });

  // Input before the screenshot is shown is ignored, so a click can't draw a
  // region or, as here, dismiss the overlay.
  test('IgnoresClicksUntilScreenshotIsRendered', async () => {
    overlay.activeRegionId = REGION_ID;
    await overlay.updateComplete;
    function click() {
      const init = {
        bubbles: true,
        composed: true,
        isPrimary: true,
        button: 0,
        clientX: 10,
        clientY: 10,
      };
      overlay.dispatchEvent(new PointerEvent('pointerdown', init));
      document.dispatchEvent(new PointerEvent('pointerup', init));
    }

    click();
    assertEquals(0, handler.getCallCount('dismissOverlay'));

    overlay.isScreenshotRendered = true;
    await overlay.updateComplete;
    click();
    assertEquals(1, handler.getCallCount('dismissOverlay'));
  });

  // A prewarmed overlay is on screen while the screenshot is still being
  // captured, so it draws nothing over the page until the screenshot is shown.
  test('HidesOverlayElementsUntilScreenshotIsRendered', async () => {
    const flashScrim = overlay.$.initialFlashScrim;
    const selectionElements =
        overlay.shadowRoot.querySelector<HTMLElement>('#selectionElements')!;
    assertEquals('hidden', getComputedStyle(flashScrim).visibility);
    assertEquals('hidden', getComputedStyle(selectionElements).visibility);

    overlay.isScreenshotRendered = true;
    await overlay.updateComplete;
    assertEquals('visible', getComputedStyle(flashScrim).visibility);
    assertEquals('visible', getComputedStyle(selectionElements).visibility);
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

  async function receivePageContext() {
    page.pageContextReceived();
    await page.$.flushForTesting();
    await overlay.updateComplete;
  }

  test('ClickingSubmitButtonSubmitsWithoutFocusingInput', async () => {
    await receivePageContext();
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

  test('SubmitDisabledUntilPageContextReceived', async () => {
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    const submitButton =
        overlay.shadowRoot.querySelector<HTMLElement>('#submitPromptButton')!;
    input.value = 'Explain this region';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    await overlay.updateComplete;

    assertFalse(submitButton.hidden);
    assertTrue(submitButton.hasAttribute('disabled'));
    assertEquals(
        loadTimeData.getString('fetchingPageContent'), submitButton.title);
    // The tooltip needs hover.
    assertEquals('auto', getComputedStyle(submitButton).pointerEvents);
    submitButton.click();
    input.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    assertEquals(0, handler.getCallCount('submitPrompt'));

    await receivePageContext();

    assertFalse(submitButton.hasAttribute('disabled'));
    assertEquals(loadTimeData.getString('submitPrompt'), submitButton.title);
    input.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    assertEquals(1, handler.getCallCount('submitPrompt'));
    assertEquals('Explain this region', handler.getArgs('submitPrompt')[0]);
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

  test('SuggestionChipWidthAndSingleLineEllipsis', async () => {
    overlay.suggestedActions = [
      createSuggestedAction('1', 'Short'),
      createSuggestedAction(
          '2',
          'A longer suggestion title that is truncated with an ellipsis on a single line'),
    ];
    await overlay.updateComplete;

    const pill =
        overlay.shadowRoot.querySelector<HTMLElement>('.searchbox-pill')!;
    const chips = Array.from(
        overlay.shadowRoot.querySelectorAll<HTMLElement>('.action-chip'));
    assertEquals(2, chips.length);

    const pillRect = pill.getBoundingClientRect();
    const chip0Rect = chips[0]!.getBoundingClientRect();
    const chip1Rect = chips[1]!.getBoundingClientRect();

    // No chip can exceed the width of the input box.
    assertLE(chip0Rect.width, pillRect.width);
    assertEquals(pillRect.width, chip1Rect.width);

    // Both chips remain a single line (32px tall), with long text ellipsised.
    assertEquals(32, Math.round(chip0Rect.height));
    assertEquals(32, Math.round(chip1Rect.height));

    const label1 = chips[1]!.querySelector<HTMLElement>('.chip-label')!;
    assertEquals('ellipsis', getComputedStyle(label1).textOverflow);
    assertTrue(label1.scrollWidth > label1.clientWidth);
  });

  test('SearchboxPillUsesThemeColors', () => {
    const pill =
        overlay.shadowRoot.querySelector<HTMLElement>('.searchbox-pill')!;
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;

    document.documentElement.style.setProperty(
        '--cr-fallback-color-surface', 'rgb(31, 31, 31)');
    document.documentElement.style.setProperty(
        '--cr-primary-text-color', 'rgb(227, 227, 227)');

    assertEquals('rgb(31, 31, 31)', getComputedStyle(pill).backgroundColor);
    assertEquals('rgb(227, 227, 227)', getComputedStyle(input).color);

    document.documentElement.style.removeProperty(
        '--cr-fallback-color-surface');
    document.documentElement.style.removeProperty('--cr-primary-text-color');
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

const LEFT_BUTTON = 0;
const RIGHT_BUTTON = 2;

function pointerEvent(
    type: string, x: number, y: number, button: number): PointerEvent {
  return new PointerEvent(type, {
    pointerId: 1,
    bubbles: true,
    composed: true,
    button,
    clientX: x,
    clientY: y,
    isPrimary: true,
  });
}

// Region ids are tokens: 32 hex digits.
const REGION_ID = '0123456789ABCDEF0123456789ABCDEF';
const REGION_ID_2 = 'FEDCBA9876543210FEDCBA9876543210';

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

  async function click(x: number, y: number, button: number = LEFT_BUTTON) {
    const point = toClient(x, y);
    overlay.dispatchEvent(
        pointerEvent('pointerdown', point.x, point.y, button));
    overlay.dispatchEvent(pointerEvent('pointerup', point.x, point.y, button));
    await flushTasks();
  }

  async function drag(
      fromX: number, fromY: number, toX: number, toY: number,
      button: number = LEFT_BUTTON) {
    const from = toClient(fromX, fromY);
    const to = toClient(toX, toY);
    overlay.dispatchEvent(pointerEvent('pointerdown', from.x, from.y, button));
    overlay.dispatchEvent(pointerEvent('pointermove', to.x, to.y, button));
    // Pointer moves are handled once per frame.
    await waitAfterNextRender(overlay);
    overlay.dispatchEvent(pointerEvent('pointerup', to.x, to.y, button));
    await flushTasks();
  }

  // Answers as the browser does once it holds the regions with `ids`.
  async function setRegions(ids: string[]) {
    page.setPostRegionSelections(ids.map(
        (id, i) => ({
          id,
          shape: {rect: {x: 0.1 + 0.4 * i, y: 0.2, width: 0.2, height: 0.2}},
        })));
    await page.$.flushForTesting();
    await overlay.updateComplete;
  }

  async function setRegion(id: string) {
    await setRegions([id]);
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
    await waitAfterNextRender(overlay);
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

  test('LeftDragReplacesRegion', async () => {
    await showOverlay(/*enablePromptBox=*/ true);
    page.setSidePanelOpen(true);
    page.setInteractionOptions({
      hideHandles: true,
      skipIntroAnimation: true,
    });
    await setRegion(REGION_ID);
    assertTrue(overlay.hideHandles);

    await drag(0.5, 0.5, 0.7, 0.7);

    assertFalse(overlay.hideHandles);
    assertEquals(1, handler.getCallCount('adjustRegion'));
    assertEquals(REGION_ID, handler.getArgs('adjustRegion')[0][0].id);
    assertEquals(0, handler.getCallCount('dismissOverlay'));
  });

  test('RightDragAddsRegionWithSidePanelOpen', async () => {
    await showOverlay(/*enablePromptBox=*/ true);
    page.setSidePanelOpen(true);
    await setRegion(REGION_ID);

    await drag(0.5, 0.5, 0.7, 0.7, RIGHT_BUTTON);

    assertEquals(1, handler.getCallCount('adjustRegion'));
    assertNotEquals(REGION_ID, handler.getArgs('adjustRegion')[0][0].id);
    assertEquals(0, handler.getCallCount('dismissOverlay'));
  });

  test('RightDragIgnoredWithSidePanelClosed', async () => {
    await showOverlay(/*enablePromptBox=*/ true);

    await drag(0.1, 0.1, 0.3, 0.3, RIGHT_BUTTON);
    assertEquals(0, handler.getCallCount('adjustRegion'));
    assertEquals(0, handler.getCallCount('dismissOverlay'));

    await setRegion(REGION_ID);
    await drag(0.5, 0.5, 0.7, 0.7, RIGHT_BUTTON);

    assertEquals(0, handler.getCallCount('adjustRegion'));
    assertEquals(0, handler.getCallCount('dismissOverlay'));
  });

  test('RightClickWithRegionDoesNothing', async () => {
    await showOverlay(/*enablePromptBox=*/ true);
    page.setSidePanelOpen(true);
    await setRegion(REGION_ID);

    await click(0.8, 0.8, RIGHT_BUTTON);

    assertEquals(0, handler.getCallCount('adjustRegion'));
    assertEquals(0, handler.getCallCount('dismissOverlay'));
  });

  test('ContextMenuBlockedWithPromptBox', async () => {
    await showOverlay(/*enablePromptBox=*/ true);

    const event =
        new MouseEvent('contextmenu', {bubbles: true, cancelable: true});
    overlay.dispatchEvent(event);

    assertTrue(event.defaultPrevented);
  });

  test('DragWithRegionAndSidePanelClosedDismisses', async () => {
    await showOverlay(/*enablePromptBox=*/ true);
    await setRegion(REGION_ID);

    await drag(0.5, 0.5, 0.7, 0.7);

    assertEquals(0, handler.getCallCount('adjustRegion'));
    assertEquals(1, handler.getCallCount('dismissOverlay'));
  });

  test('CursorDefaultWithRegionAndSidePanelClosed', async () => {
    await showOverlay(/*enablePromptBox=*/ true);
    overlay.$.selectionOverlay.dispatchEvent(new PointerEvent('pointerenter'));
    await overlay.updateComplete;

    assertEquals(
        'crosshair', getComputedStyle(overlay.$.selectionOverlay).cursor);
    assertEquals('visible', getComputedStyle(overlay.$.cursor).visibility);

    await drag(0.1, 0.1, 0.3, 0.3);
    await waitAfterNextRender(overlay);
    overlay.$.selectionOverlay.dispatchEvent(new PointerEvent('pointerenter'));
    await overlay.updateComplete;
    assertEquals(1, handler.getCallCount('adjustRegion'));
    assertEquals(
        'default', getComputedStyle(overlay.$.selectionOverlay).cursor);
    assertEquals('hidden', getComputedStyle(overlay.$.cursor).visibility);

    page.setSidePanelOpen(true);
    await page.$.flushForTesting();
    await overlay.updateComplete;
    assertEquals(
        'crosshair', getComputedStyle(overlay.$.selectionOverlay).cursor);
    assertEquals('visible', getComputedStyle(overlay.$.cursor).visibility);

    page.setSidePanelOpen(false);
    await page.$.flushForTesting();
    await overlay.updateComplete;
    assertEquals(
        'default', getComputedStyle(overlay.$.selectionOverlay).cursor);
    assertEquals('hidden', getComputedStyle(overlay.$.cursor).visibility);
  });

  test('DragAddsRegionWithoutPromptBox', async () => {
    await showOverlay(/*enablePromptBox=*/ false);
    page.setSidePanelOpen(true);
    await setRegion(REGION_ID);

    await drag(0.5, 0.5, 0.7, 0.7);

    assertEquals(1, handler.getCallCount('adjustRegion'));
    assertNotEquals(REGION_ID, handler.getArgs('adjustRegion')[0][0].id);
  });

  test('ClickSelectsAreaWithoutPromptBox', async () => {
    await showOverlay(/*enablePromptBox=*/ false);

    await click(0.5, 0.5);

    assertEquals(1, handler.getCallCount('adjustRegion'));
    assertEquals(0, handler.getCallCount('dismissOverlay'));
  });

  test('MultipleRegionsHidePromptAndSuggestions', async () => {
    await showOverlay(/*enablePromptBox=*/ true);
    await setRegion(REGION_ID);
    overlay.showFloatingPrompt = true;
    overlay.suggestedActions = [{title: 'Explain'} as SuggestedAction];

    await setRegions([REGION_ID, REGION_ID_2]);

    assertFalse(overlay.showFloatingPrompt);
    assertEquals(0, overlay.suggestedActions.length);
  });

  test('NoSuggestionsFetchedWithMultipleRegions', async () => {
    await showOverlay(/*enablePromptBox=*/ true);
    page.setSidePanelOpen(true);
    await setRegions([REGION_ID, REGION_ID_2]);
    const fetches = handler.getCallCount('getSuggestedActions');

    await drag(0.5, 0.5, 0.7, 0.7);

    assertEquals(fetches, handler.getCallCount('getSuggestedActions'));
    assertFalse(overlay.showFloatingPrompt);
  });

  test('BackToOneRegionFetchesSuggestions', async () => {
    await showOverlay(/*enablePromptBox=*/ true);
    await setRegions([REGION_ID, REGION_ID_2]);
    const fetches = handler.getCallCount('getSuggestedActions');

    await setRegion(REGION_ID);

    assertEquals(fetches + 1, handler.getCallCount('getSuggestedActions'));
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

  // Converts viewport pixel bounds into a normalized center-based `RectF`.
  function regionFromPx(
      left: number, top: number, width: number, height: number): RectF {
    return {
      x: (left + width / 2) / window.innerWidth,
      y: (top + height / 2) / window.innerHeight,
      width: width / window.innerWidth,
      height: height / window.innerHeight,
    };
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

  test('PromptBoxBelowSelectionLeftAligned', async () => {
    const region = {x: 0.3, y: 0.3, width: 0.1, height: 0.1};
    await showOverlay({promptBox: true});
    await select(region);

    const rect = promptRect();
    const selection = toViewport(region);
    assertNear(selection.left, rect.left, 'left');
    assertNear(selection.bottom + MARGIN, rect.top, 'top');
    assertFalse(overlay.promptAboveSelection);
  });

  test('PromptBoxBelowSelectionRightAlignedNearRightEdge', async () => {
    await showOverlay({promptBox: true});
    const region = regionFromPx(window.innerWidth - 120, 100, 80, 60);
    await select(region);

    const rect = promptRect();
    const selection = toViewport(region);
    assertNear(selection.right, rect.right, 'right');
    assertNear(selection.bottom + MARGIN, rect.top, 'top');
    assertFalse(overlay.promptAboveSelection);
  });

  test('PromptBoxMovesAboveSelectionNearBottom', async () => {
    const region = {x: 0.3, y: 0.9, width: 0.1, height: 0.1};
    await showOverlay({promptBox: true});
    await select(region, [chip('Summarize'), chip('Ask Gemini')]);

    const rect = promptRect();
    const selection = toViewport(region);
    assertTrue(overlay.promptAboveSelection);
    assertNear(selection.left, rect.left, 'left');
    assertNear(selection.top - MARGIN, rect.bottom, 'bottom');
    // The pill stays closest to the selection, with the first chip next to it.
    const pillRect = query('.searchbox-pill').getBoundingClientRect();
    const [chip0Rect, chip1Rect] = chips().map(c => c.getBoundingClientRect());
    assertTrue(chip0Rect!.bottom <= pillRect.top);
    assertTrue(chip1Rect!.bottom <= chip0Rect!.top);
  });

  test('PromptBoxAboveSelectionRightAlignedNearBottomRight', async () => {
    await showOverlay({promptBox: true});
    const region =
        regionFromPx(window.innerWidth - 120, window.innerHeight - 120, 80, 60);
    await select(region, [chip('Summarize'), chip('Ask Gemini')]);

    const rect = promptRect();
    const selection = toViewport(region);
    assertTrue(overlay.promptAboveSelection);
    assertNear(selection.right, rect.right, 'right');
    assertNear(selection.top - MARGIN, rect.bottom, 'bottom');
  });

  test('PromptBoxReservesSpaceForSuggestionsBeforeTheyArrive', async () => {
    await showOverlay({promptBox: true});
    // Leave 100px below the selection: enough for the 44px input box alone,
    // but not enough for the 120px maximum height with two 1-line suggestions.
    const region = regionFromPx(100, window.innerHeight - 160, 80, 60);
    await select(region);

    // Even before suggestions arrive, the input box is placed above the
    // selection so it does not jump when suggestions stream in.
    assertTrue(overlay.promptAboveSelection);
    const selection = toViewport(region);
    assertNear(selection.top - MARGIN, promptRect().bottom, 'bottom before');

    const listener: SuggestedActionsListenerRemote =
        await handler.whenCalled('getSuggestedActions');
    listener.onSuggestedActionsAvailable(
        [chip('Summarize'), chip('Ask Gemini')]);
    await flushTasks();
    await waitForPromptToSettle();

    assertTrue(overlay.promptAboveSelection);
    assertNear(selection.top - MARGIN, promptRect().bottom, 'bottom after');
  });

  test('PromptBoxToRightOfTallSelection', async () => {
    await showOverlay({promptBox: true});
    // Tall selection near the left edge leaving < 120px above and below.
    const region = regionFromPx(40, 100, 80, window.innerHeight - 200);
    await select(region);

    const rect = promptRect();
    const selection = toViewport(region);
    assertFalse(overlay.promptAboveSelection);
    assertNear(selection.right + MARGIN, rect.left, 'left');
    assertNear(selection.top, rect.top, 'top');
  });

  test('PromptBoxToLeftOfTallSelectionNearRightEdge', async () => {
    await showOverlay({promptBox: true});
    // Tall selection near the right edge leaving < 120px above/below and
    // < 320px to the right.
    const region = regionFromPx(
        window.innerWidth - 140, 100, 80, window.innerHeight - 200);
    await select(region);

    const rect = promptRect();
    const selection = toViewport(region);
    assertFalse(overlay.promptAboveSelection);
    assertNear(selection.left - MARGIN, rect.right, 'right');
    assertNear(selection.top, rect.top, 'top');
  });

  test('PromptBoxWithinLargeSelection', async () => {
    await showOverlay({promptBox: true});
    // Selection covering most of the viewport leaving insufficient space on all
    // four outside edges.
    const region =
        regionFromPx(60, 60, window.innerWidth - 120, window.innerHeight - 120);
    await select(region);

    const rect = promptRect();
    const selection = toViewport(region);
    assertFalse(overlay.promptAboveSelection);
    assertNear(selection.left + MARGIN, rect.left, 'left');
    assertNear(selection.top + MARGIN, rect.top, 'top');
  });

  test('PromptBoxFallsBackToWindowTopLeft', async () => {
    await showOverlay({promptBox: true});
    // Selection where none of positions 1-7 has sufficient space.
    const region = regionFromPx(4, window.innerHeight - 100, 80, 50);
    await select(region);

    const rect = promptRect();
    assertFalse(overlay.promptAboveSelection);
    assertNear(MARGIN, rect.left, 'left');
    assertNear(MARGIN, rect.top, 'top');
  });

  test('PromptBoxPositioningRtl', async () => {
    await showOverlay({promptBox: true});
    overlay.dir = 'rtl';

    // 1. Below selection, right edges aligned in RTL.
    let region = {x: 0.7, y: 0.3, width: 0.1, height: 0.1};
    await select(region);
    let rect = promptRect();
    let selection = toViewport(region);
    assertNear(selection.right, rect.right, 'rtl below right-aligned');
    assertNear(selection.bottom + MARGIN, rect.top, 'rtl below top');

    // 2. Below selection, left edges aligned when near left viewport edge in
    // RTL.
    region = regionFromPx(40, 100, 80, 60);
    await select(region);
    rect = promptRect();
    selection = toViewport(region);
    assertNear(selection.left, rect.left, 'rtl below left-aligned');
    assertNear(selection.bottom + MARGIN, rect.top, 'rtl below left top');

    // 5. To the left of selection in RTL when tall on the right side.
    region = regionFromPx(
        window.innerWidth - 120, 100, 80, window.innerHeight - 200);
    await select(region);
    rect = promptRect();
    selection = toViewport(region);
    assertNear(selection.left - MARGIN, rect.right, 'rtl left of selection');
    assertNear(selection.top, rect.top, 'rtl left top');

    // 6. To the right of selection in RTL when tall on the left side.
    region = regionFromPx(40, 100, 80, window.innerHeight - 200);
    await select(region);
    rect = promptRect();
    selection = toViewport(region);
    assertNear(selection.right + MARGIN, rect.left, 'rtl right of selection');
    assertNear(selection.top, rect.top, 'rtl right top');

    // 7. Within large selection, top-right corners aligned in RTL.
    region =
        regionFromPx(60, 60, window.innerWidth - 120, window.innerHeight - 120);
    await select(region);
    rect = promptRect();
    selection = toViewport(region);
    assertNear(selection.right - MARGIN, rect.right, 'rtl within right');
    assertNear(selection.top + MARGIN, rect.top, 'rtl within top');

    // 8. Top-right corner of the window fallback in RTL.
    region =
        regionFromPx(window.innerWidth - 84, window.innerHeight - 100, 80, 50);
    await select(region);
    rect = promptRect();
    assertNear(window.innerWidth - MARGIN, rect.right, 'rtl fallback right');
    assertNear(MARGIN, rect.top, 'rtl fallback top');
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

  test('PromptBoxInlineFulfillmentPrefers380AndShrinksToFit', async () => {
    await showOverlay({promptBox: true});

    // When >= 380px is available, the inline fulfillment host uses its
    // preferred 380px width.
    let region = regionFromPx(100, 100, 80, 60);
    await select(region, [cardChip()]);
    await clickCardChip(fakeCard(100));
    let hostRect = query('#inlineFulfillmentHost').getBoundingClientRect();
    let selection = toViewport(region);
    assertNear(380, hostRect.width, 'preferred width');
    assertNear(selection.left, promptRect().left, 'left with 380px');

    // When between 320px and 380px (340px) is available from selection.left to
    // the right viewport margin, the inline fulfillment host shrinks to 340px
    // and stays left-aligned below the selection without jumping.
    region = regionFromPx(window.innerWidth - MARGIN - 340, 100, 80, 60);
    await select(region, [cardChip()]);
    await clickCardChip(fakeCard(100));
    hostRect = query('#inlineFulfillmentHost').getBoundingClientRect();
    selection = toViewport(region);
    assertNear(340, hostRect.width, 'shrunk width');
    assertNear(selection.left, promptRect().left, 'left stays aligned');
    assertNear(window.innerWidth - MARGIN, promptRect().right, 'right margin');

    // When right-aligned below the selection near the right edge, both the
    // pill and the 380px inline fulfillment host stay aligned to
    // selection.right.
    region = regionFromPx(window.innerWidth - 120, 100, 80, 60);
    await select(region, [cardChip()]);
    await clickCardChip(fakeCard(100));
    hostRect = query('#inlineFulfillmentHost').getBoundingClientRect();
    const pillRect = query('.searchbox-pill').getBoundingClientRect();
    selection = toViewport(region);
    assertNear(380, hostRect.width, 'right-aligned preferred width');
    assertNear(selection.right, hostRect.right, 'host right-aligned');
    assertNear(selection.right, pillRect.right, 'pill right-aligned');
  });

  test('HandlesStayHiddenWithCard', async () => {
    await showOverlay({prompt: true});
    page.setInteractionOptions({
      hideHandles: true,
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
