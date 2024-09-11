// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/lens_overlay_app.js';

import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import type {CursorTooltipElement} from 'chrome-untrusted://lens-overlay/cursor_tooltip.js';
import type {LensPageRemote} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import type {LensOverlayAppElement} from 'chrome-untrusted://lens-overlay/lens_overlay_app.js';
import type {SelectionOverlayElement} from 'chrome-untrusted://lens-overlay/selection_overlay.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {fakeScreenshotBitmap, waitForScreenshotRendered} from '../utils/image_utils.js';
import {createObject} from '../utils/object_utils.js';
import {simulateStartDrag} from '../utils/selection_utils.js';
import {createLine, createParagraph, createText, createWord, dispatchTranslateStateEvent} from '../utils/text_utils.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

function isRendered(el: HTMLElement) {
  return isVisible(el) && getComputedStyle(el).visibility !== 'hidden';
}

suite('OverlayCursor', () => {
  let lensOverlayElement: LensOverlayAppElement;
  let selectionOverlayElement: SelectionOverlayElement;
  let cursorTooltip: CursorTooltipElement;
  let tooltipEl: HTMLElement;
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let callbackRouterRemote: LensPageRemote;

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    // Turn off the shimmer. Since the shimmer is resource intensive, turn off
    // to prevent from causing issues in the tests. Also override localized
    // strings so tests don't fail if the strings change.
    loadTimeData.overrideValues({
      'enableShimmer': false,
      'cursorTooltipTextHighlightMessage': 'Select text',
      'cursorTooltipClickMessage': 'Select object',
      'cursorTooltipDragMessage': 'Drag to search',
      'cursorTooltipLivePageMessage': 'Click to exit',
    });

    lensOverlayElement = document.createElement('lens-overlay-app');
    document.body.appendChild(lensOverlayElement);
    await waitAfterNextRender(lensOverlayElement);

    selectionOverlayElement = lensOverlayElement.$.selectionOverlay;
    cursorTooltip = lensOverlayElement.$.cursorTooltip;
    tooltipEl = cursorTooltip.$.cursorTooltip;

    // Send a fake screenshot to unhide the selection overlay.
    testBrowserProxy.page.screenshotDataReceived(fakeScreenshotBitmap());
    await waitForScreenshotRendered(selectionOverlayElement);

    // Since the size of the Selection Overlay is based on the screenshot which
    // is not loaded in the test, we need to force the overlay to take up the
    // viewport.
    selectionOverlayElement.$.selectionOverlay.style.width = '100%';
    selectionOverlayElement.$.selectionOverlay.style.height = '100%';
    await waitAfterNextRender(lensOverlayElement);

    await addWords();
    await addObjects();
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

  function addWords(): Promise<void> {
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

  function addObjects(): Promise<void> {
    const objects =
        [
          {x: 80, y: 20, width: 25, height: 10},
          {x: 70, y: 35, width: 20, height: 10},
        ]
            .map(
                (rect, i) => createObject(
                    i.toString(), normalizedBox(rect), /*isMaskClick=*/ true));
    callbackRouterRemote.objectsReceived(objects);
    return flushTasks();
  }

  async function simulateHover(el: Element) {
    const elBounds = el.getBoundingClientRect();
    const moveDetails = {
      clientX: (elBounds.top + elBounds.bottom) / 2,
      clientY: (elBounds.left + elBounds.right) / 2,
      bubbles: true,
      composed: true,
    };
    el.dispatchEvent(new PointerEvent('pointerenter'));
    el.dispatchEvent(new PointerEvent('pointermove', moveDetails));
    return waitAfterNextRender(lensOverlayElement);
  }

  async function simulateUnhover(el: Element) {
    el.dispatchEvent(new PointerEvent('pointerleave'));
    return waitAfterNextRender(lensOverlayElement);
  }

  async function simulateEnterViewport() {
    const appContainerEl =
        lensOverlayElement.shadowRoot!.querySelector('.app-container')!;
    simulateHover(appContainerEl);
    return waitAfterNextRender(lensOverlayElement);
  }

  test('Text', async () => {
    await simulateEnterViewport();

    // Hover over a text element.
    const textLayer = selectionOverlayElement.$.textSelectionLayer;
    const textElement = textLayer.shadowRoot!.querySelector('.word')!;
    await simulateHover(textElement);

    // Verify the cursor tooltip changed to text string.
    assertTrue(isRendered(tooltipEl));
    assertStringContains(tooltipEl.innerHTML, 'Select text');

    // Verify the cursor changed to text and the cursor image changed to text
    // icon.
    assertEquals('text', document.body.style.cursor);
    assertEquals(
        'url("text.svg")',
        selectionOverlayElement.style.getPropertyValue('--cursor-img-url'));

    await simulateUnhover(textElement);

    // Verify the cursor changed to unset and the cursor image is still Lens
    // icon.
    assertEquals('unset', document.body.style.cursor);
    assertEquals(
        'url("lens.svg")',
        selectionOverlayElement.style.getPropertyValue('--cursor-img-url'));

    // Now enable translate mode.
    dispatchTranslateStateEvent(
        selectionOverlayElement.$.textSelectionLayer, true, 'es');

    // Hover over the selection overlay.
    await simulateHover(selectionOverlayElement.$.selectionOverlay);

    // Verify the cursor tooltip is the text string, despite the cursor not
    // hovering over text.
    assertTrue(isRendered(tooltipEl));
    assertStringContains(tooltipEl.innerHTML, 'Select text');

    // Verify the cursor changed to text and the cursor image changed to text
    // icon.
    assertEquals('text', document.body.style.cursor);
    assertEquals(
        'url("text.svg")',
        selectionOverlayElement.style.getPropertyValue('--cursor-img-url'));
  });

  test('Object', async () => {
    await simulateEnterViewport();

    // Hover over a object element.
    const objectLayer = selectionOverlayElement.$.objectSelectionLayer;
    const objectElement = objectLayer.shadowRoot!.querySelector('.object')!;

    await simulateHover(objectElement);

    // Verify the cursor tooltip changed to object string.
    assertTrue(isRendered(tooltipEl));
    assertStringContains(tooltipEl.innerHTML, 'Select object');

    // Verify the cursor image changed to Lens icon.
    assertEquals(
        'url("lens.svg")',
        selectionOverlayElement.style.getPropertyValue('--cursor-img-url'));

    await simulateUnhover(objectElement);

    // Verify the cursor changed to unset and the cursor image is still Lens
    // icon.
    assertEquals('unset', document.body.style.cursor);
    assertEquals(
        'url("lens.svg")',
        selectionOverlayElement.style.getPropertyValue('--cursor-img-url'));
  });

  test('RegionSelection', async () => {
    await simulateEnterViewport();

    // Hover over a selection overlay.
    await simulateHover(selectionOverlayElement.$.selectionOverlay);

    // Verify the cursor tooltip changed to object string.
    assertTrue(isRendered(tooltipEl));
    assertStringContains(tooltipEl.innerHTML, 'Drag to search');

    // Start a drag that goes outside the overlay boundaries.
    const boundingRect = selectionOverlayElement.getBoundingClientRect();
    await simulateStartDrag(
        selectionOverlayElement, {x: 10, y: 10},
        {x: boundingRect.right + 2000, y: boundingRect.bottom + 2000});

    // Verify the cursor tooltip is still object string.
    assertTrue(isRendered(tooltipEl));
    assertStringContains(tooltipEl.innerHTML, 'Drag to search');

    // Verify the cursor changed to crosshair and the cursor image changed to
    // Lens icon.
    assertEquals('crosshair', document.body.style.cursor);
    assertEquals(
        'url("lens.svg")',
        selectionOverlayElement.style.getPropertyValue('--cursor-img-url'));
  });


  test('ExitScrimTooltip', async () => {
    await simulateEnterViewport();

    // Hover over the background scrim.
    await simulateHover(lensOverlayElement.$.backgroundScrim);

    // Verify the cursor tooltip changed to object string.
    assertTrue(isRendered(tooltipEl));
    assertStringContains(tooltipEl.innerHTML, 'Click to exit');
  });

  test('HideTooltip', async () => {
    await simulateEnterViewport();

    // Hover over some text.
    const textLayer = selectionOverlayElement.$.textSelectionLayer;
    const textElement = textLayer.shadowRoot!.querySelector('.word')!;
    await simulateHover(textElement);
    assertTrue(isRendered(tooltipEl));

    // Simulate changing hover from text to close button.
    await simulateUnhover(textElement);
    await simulateUnhover(selectionOverlayElement);
    await simulateHover(lensOverlayElement.$.closeButton);

    // Verify no tooltip.
    assertFalse(isRendered(tooltipEl));

    // Hover over the background scrim.
    await simulateUnhover(lensOverlayElement.$.closeButton);
    await simulateHover(lensOverlayElement.$.backgroundScrim);
    assertTrue(isRendered(tooltipEl));

    // Simulate changing hover from background scrim to close button.
    await simulateUnhover(lensOverlayElement.$.backgroundScrim);
    await simulateHover(lensOverlayElement.$.closeButton);

    // Verify no tooltip.
    assertFalse(isRendered(tooltipEl));

    // Simulate changing hover from close button back to background scrim.
    await simulateUnhover(lensOverlayElement.$.closeButton);
    await simulateHover(lensOverlayElement.$.backgroundScrim);

    // Verify tooltip shows again.
    assertTrue(isRendered(tooltipEl));

    // Simulate changing hover over info.
    await simulateUnhover(lensOverlayElement.$.backgroundScrim);
    await simulateHover(lensOverlayElement.$.moreOptionsButton);

    // Verify no tooltip.
    assertFalse(isRendered(tooltipEl));

    // Open the info menu.
    lensOverlayElement.$.moreOptionsButton.click();
    await flushTasks();
    assertTrue(isVisible(lensOverlayElement.$.moreOptionsMenu));

    // Hover the background.
    await simulateUnhover(lensOverlayElement.$.moreOptionsButton);
    await simulateHover(lensOverlayElement.$.backgroundScrim);

    // Simulate changing hover from background to info menu.
    await simulateUnhover(lensOverlayElement.$.backgroundScrim);
    await simulateHover(lensOverlayElement.$.moreOptionsMenu);

    // Verify no tooltip.
    assertFalse(isRendered(tooltipEl));
  });

  test('HideCursor', async () => {
    await simulateEnterViewport();

    // Hover over overlay.
    await simulateHover(selectionOverlayElement.$.selectionOverlay);

    // Verify cursor shows.
    const cursor = selectionOverlayElement.$.cursor;
    assertEquals('', cursor.className);

    // Simulate changing hover from text to close button.
    await simulateUnhover(selectionOverlayElement.$.selectionOverlay);
    await simulateHover(lensOverlayElement.$.closeButton);

    // Verify no cursor.
    assertEquals('hidden', cursor.className);
  });
});
