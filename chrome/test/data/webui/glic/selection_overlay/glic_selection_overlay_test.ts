// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://glic/glic_selection_overlay.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {BrowserProxy} from 'chrome-untrusted://glic/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome-untrusted://glic/browser_proxy.js';
import type {GlicSelectionOverlayElement} from 'chrome-untrusted://glic/glic_selection_overlay.js';
import {SelectionOverlayBaseHandler} from 'chrome-untrusted://glic/lens/selection_overlay_base_handler.js';
import {SelectionOverlayPageCallbackRouter, SelectionOverlayPageHandlerRemote} from 'chrome-untrusted://glic/selection_overlay.mojom-webui.js';
import {SelectionOverlayBaseHandlerImpl} from 'chrome-untrusted://glic/selection_overlay_base_handler_impl.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

class FakeBrowserProxy implements BrowserProxy {
  callbackRouter: SelectionOverlayPageCallbackRouter =
      new SelectionOverlayPageCallbackRouter();
  handler: SelectionOverlayPageHandlerRemote =
      new SelectionOverlayPageHandlerRemote();
  submittedPrompts: string[] = [];
  dismissCount: number = 0;

  constructor() {
    this.handler.getSuggestedActions = () => {};
    this.handler.submitPrompt = (prompt: string) => {
      this.submittedPrompts.push(prompt);
    };
    this.handler.dismissOverlay = () => {
      this.dismissCount++;
    };
  }
}

suite('GlicSelectionOverlay', () => {
  let overlay: GlicSelectionOverlayElement;
  let fakeProxy: FakeBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      enableSelectionOverlayPromptBox: true,
      enableShimmer: false,
    });

    fakeProxy = new FakeBrowserProxy();
    BrowserProxyImpl.setInstance(fakeProxy);
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
    assertEquals(0, fakeProxy.dismissCount);
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
    assertEquals(0, fakeProxy.dismissCount);
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

    assertEquals(1, fakeProxy.submittedPrompts.length);
    assertEquals('Explain this region', fakeProxy.submittedPrompts[0]);
    assertNotEquals(input, overlay.shadowRoot.activeElement);
  });
});
