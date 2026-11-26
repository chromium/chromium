// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import {GenericAssociatedInterface, GenericAssociatedInterfaceRemote} from '//resources/mojo/mojo/public/mojom/base/generic_pending_associated_receiver.mojom-webui.js';
import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {browserProxyFactory, PromptEndpointId, ScreenshotBitmapBrowserProxyImpl, SelectionOverlayBaseHandler, SelectionOverlayBaseHandlerImpl, SelectionOverlayPageHandlerRemote, SuggestedActionIcon} from 'chrome-untrusted://glic/selection_overlay.js';
import type {GlicSelectionOverlayElement, InlineFulfillmentHostElement, PromptEndpointInfo, SelectionOverlayPageRemote, SuggestedAction, SuggestedActionsListenerRemote} from 'chrome-untrusted://glic/selection_overlay.js';
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
      pageScrollbarWidth: 0,
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
    overlay.enableSelectionOverlayPromptBox = false;
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

  test('ClickingEndpointIconFocusesPromptInput', () => {
    const endpointIcon =
        overlay.shadowRoot.querySelector<HTMLElement>('#endpointIcon')!;
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    assertTrue(!!endpointIcon);
    assertTrue(!!input);

    input.blur();
    assertNotEquals(input, overlay.shadowRoot.activeElement);

    endpointIcon.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true, composed: true}));
    assertTrue(overlay.showFloatingPrompt);
    endpointIcon.click();

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

  function twoEndpoints(): PromptEndpointInfo[] {
    return [
      {
        id: PromptEndpointId.kGemini,
        name: 'Gemini',
        placeholder: 'Ask Gemini',
      },
      {
        id: PromptEndpointId.kGoogleSearch,
        name: 'Google Search',
        placeholder: 'Ask Google',
      },
    ];
  }

  async function setEndpoints(
      endpoints: PromptEndpointInfo[], defaultId: PromptEndpointId) {
    page.setPromptEndpoints(endpoints, defaultId);
    await page.$.flushForTesting();
    await overlay.updateComplete;
  }

  // Give the overlay a real selection so `floatingPromptResizeObserver_`
  // keeps `showFloatingPrompt` true across async frames when endpoints change
  // the prompt row width.
  async function selectRegionForPrompt() {
    page.setPostRegionSelections([{
      id: REGION_ID,
      shape: {rect: {x: 0.5, y: 0.5, width: 0.2, height: 0.2}},
    }]);
    await page.$.flushForTesting();
    await waitAfterNextRender(overlay);
    assertTrue(overlay.showFloatingPrompt);
  }

  test('SetPromptEndpointsRespectsLastUsedDefault', async () => {
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    const endpointIcon =
        overlay.shadowRoot.querySelector<HTMLElement>('#endpointIcon')!;
    const endpointIconImage =
        endpointIcon.querySelector<HTMLElement>('.endpoint-icon-image')!;
    assertEquals('true', endpointIcon.getAttribute('aria-hidden'));
    assertFalse(endpointIcon.hasAttribute('role'));
    assertFalse(endpointIcon.hasAttribute('title'));
    assertFalse(endpointIcon.hasAttribute('aria-label'));
    assertFalse(input.hasAttribute('aria-description'));
    assertTrue(endpointIconImage.classList.contains('gemini'));
    assertEquals(
        0,
        overlay.shadowRoot.querySelectorAll('.endpoint-switcher-button')
            .length);

    await setEndpoints(twoEndpoints(), PromptEndpointId.kGoogleSearch);

    const tooltip = loadTimeData.getString('switchProviderTooltip');
    assertEquals('Ask Google', input.placeholder);
    assertEquals('Ask Google', input.getAttribute('aria-label'));
    assertEquals(tooltip, input.getAttribute('aria-description'));
    assertEquals('true', endpointIcon.getAttribute('aria-hidden'));
    assertFalse(endpointIcon.hasAttribute('role'));
    assertFalse(endpointIcon.hasAttribute('title'));
    assertFalse(endpointIcon.hasAttribute('aria-label'));
    assertTrue(endpointIconImage.classList.contains('google-search'));
    assertFalse(endpointIconImage.classList.contains('gemini'));

    const switcherButtons = overlay.shadowRoot.querySelectorAll<HTMLElement>(
        '.endpoint-switcher-button');
    assertEquals(1, switcherButtons.length);
    assertEquals('Gemini', switcherButtons[0]!.getAttribute('title'));
    assertEquals('Gemini', switcherButtons[0]!.getAttribute('aria-label'));
    assertTrue(
        switcherButtons[0]!.querySelector<HTMLElement>('.endpoint-icon-image')!
            .classList.contains('gemini'));
  });

  test('TabAndShiftTabCycleEndpointsRequerySuggestionsAndSubmit', async () => {
    await receivePageContext();
    await setEndpoints(twoEndpoints(), PromptEndpointId.kGemini);

    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    input.focus();
    input.value = 'What is this?';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    await overlay.updateComplete;

    // IME composition Tab must not hijack candidate selection.
    const composingTab = new KeyboardEvent('keydown', {
      key: 'Tab',
      bubbles: true,
      cancelable: true,
      isComposing: true,
    });
    input.dispatchEvent(composingTab);
    assertFalse(composingTab.defaultPrevented);
    assertEquals('Ask Gemini', input.placeholder);

    // Modified Tab (Ctrl/Alt/Meta) must not hijack browser shortcuts.
    for (const modifier of ['ctrlKey', 'altKey', 'metaKey'] as const) {
      const modifiedTab = new KeyboardEvent('keydown', {
        key: 'Tab',
        [modifier]: true,
        bubbles: true,
        cancelable: true,
      });
      input.dispatchEvent(modifiedTab);
      assertFalse(modifiedTab.defaultPrevented);
      assertEquals('Ask Gemini', input.placeholder);
    }

    const initialSuggestionCalls = handler.getCallCount('getSuggestedActions');

    // Tab cycles forward: Gemini -> Google Search.
    const tabEvent = new KeyboardEvent(
        'keydown', {key: 'Tab', bubbles: true, cancelable: true});
    input.dispatchEvent(tabEvent);
    await overlay.updateComplete;
    assertTrue(tabEvent.defaultPrevented);
    assertEquals('Ask Google', input.placeholder);
    assertEquals('What is this?', input.value);
    assertEquals(
        'Google Search',
        overlay.shadowRoot.querySelector('#endpointLiveRegion')
            ?.textContent?.trim());
    assertEquals(1, handler.getCallCount('setActivePromptEndpoint'));
    assertEquals(
        PromptEndpointId.kGoogleSearch,
        handler.getArgs('setActivePromptEndpoint')[0]);
    assertEquals(
        initialSuggestionCalls + 1,
        handler.getCallCount('getSuggestedActions'));

    // Shift+Tab cycles backward: Google Search -> Gemini -> Google Search.
    input.dispatchEvent(new KeyboardEvent(
        'keydown',
        {key: 'Tab', shiftKey: true, bubbles: true, cancelable: true}));
    await overlay.updateComplete;
    assertEquals('Ask Gemini', input.placeholder);
    assertEquals(
        PromptEndpointId.kGemini,
        handler.getArgs('setActivePromptEndpoint').at(-1));

    input.dispatchEvent(new KeyboardEvent(
        'keydown',
        {key: 'Tab', shiftKey: true, bubbles: true, cancelable: true}));
    await overlay.updateComplete;
    assertEquals('Ask Google', input.placeholder);
    assertEquals(
        PromptEndpointId.kGoogleSearch,
        handler.getArgs('setActivePromptEndpoint').at(-1));

    // Tab wraps back to Gemini, then Tab to Google Search and submit.
    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Tab', bubbles: true, cancelable: true}));
    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Tab', bubbles: true, cancelable: true}));
    await overlay.updateComplete;
    assertEquals('Ask Google', input.placeholder);
    assertEquals(
        PromptEndpointId.kGoogleSearch,
        handler.getArgs('setActivePromptEndpoint').at(-1));

    input.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    assertEquals(1, handler.getCallCount('submitPrompt'));
    assertEquals('What is this?', handler.getArgs('submitPrompt')[0]);
  });

  test('ClickingEndpointIconDoesNotSwitchEndpoints', async () => {
    await selectRegionForPrompt();
    await setEndpoints(twoEndpoints(), PromptEndpointId.kGemini);
    const endpointIcon =
        overlay.shadowRoot.querySelector<HTMLElement>('#endpointIcon')!;
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    input.blur();
    assertNotEquals(input, overlay.shadowRoot.activeElement);

    endpointIcon.click();
    await overlay.updateComplete;

    assertEquals('Ask Gemini', input.placeholder);
    assertEquals(0, handler.getCallCount('setActivePromptEndpoint'));
    assertEquals(input, overlay.shadowRoot.activeElement);
  });

  async function withReducedMotion(
      reducedMotion: boolean, fn: () => Promise<void>) {
    const origMatchMedia = window.matchMedia.bind(window);
    window.matchMedia = (query: string) => {
      if (query === '(prefers-reduced-motion: reduce)') {
        return {matches: reducedMotion} as MediaQueryList;
      }
      return origMatchMedia(query);
    };
    try {
      await fn();
    } finally {
      window.matchMedia = origMatchMedia;
    }
  }

  async function finishAllOverlayAnimations() {
    const animations = overlay.shadowRoot.getAnimations();
    animations.forEach(a => a.finish());
    await Promise.all(animations.map(a => a.finished));
    await waitAfterNextRender(overlay);
  }

  test('ClickingEndpointSwitcherButtonSwitchesAndFocusesInput', async () => {
    await selectRegionForPrompt();
    await setEndpoints(twoEndpoints(), PromptEndpointId.kGemini);
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    input.value = 'Keep this text';

    let switcherButton = overlay.shadowRoot.querySelector<HTMLElement>(
        '.endpoint-switcher-button')!;
    assertTrue(!!switcherButton);
    assertEquals('Google Search', switcherButton.getAttribute('aria-label'));

    const mousedownEvent =
        new MouseEvent('mousedown', {bubbles: true, cancelable: true});
    switcherButton.dispatchEvent(mousedownEvent);
    assertTrue(mousedownEvent.defaultPrevented);

    // Move focus away from the input before clicking to verify observable
    // focus restoration back to `#promptInput`.
    switcherButton.focus();
    assertEquals(switcherButton, overlay.shadowRoot.activeElement);

    const initialSuggestionCalls = handler.getCallCount('getSuggestedActions');
    switcherButton.click();
    await overlay.updateComplete;

    assertEquals('Ask Google', input.placeholder);
    assertEquals('Keep this text', input.value);
    assertEquals(
        'Google Search',
        overlay.shadowRoot.querySelector('#endpointLiveRegion')
            ?.textContent?.trim());
    assertEquals(1, handler.getCallCount('setActivePromptEndpoint'));
    assertEquals(
        PromptEndpointId.kGoogleSearch,
        handler.getArgs('setActivePromptEndpoint')[0]);
    assertEquals(
        initialSuggestionCalls + 1,
        handler.getCallCount('getSuggestedActions'));
    assertEquals(input, overlay.shadowRoot.activeElement);

    // The circle button now represents Gemini; clicking switches back and
    // restores focus.
    switcherButton = overlay.shadowRoot.querySelector<HTMLElement>(
        '.endpoint-switcher-button')!;
    assertEquals('Gemini', switcherButton.getAttribute('aria-label'));
    switcherButton.focus();
    assertEquals(switcherButton, overlay.shadowRoot.activeElement);

    switcherButton.click();
    await overlay.updateComplete;

    assertEquals('Ask Gemini', input.placeholder);
    assertEquals(2, handler.getCallCount('setActivePromptEndpoint'));
    assertEquals(
        PromptEndpointId.kGemini,
        handler.getArgs('setActivePromptEndpoint')[1]);
    assertEquals(input, overlay.shadowRoot.activeElement);
  });

  test('ClickingNonAdjacentEndpointSwitcherButtonJumpsDirectly', async () => {
    await selectRegionForPrompt();
    const threeEndpoints: PromptEndpointInfo[] = [
      ...twoEndpoints(),
      {
        id: PromptEndpointId.kGemini,
        name: 'Third Endpoint',
        placeholder: 'Ask Third',
      },
    ];
    await setEndpoints(threeEndpoints, PromptEndpointId.kGemini);
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;

    let buttons = Array.from(overlay.shadowRoot.querySelectorAll<HTMLElement>(
        '.endpoint-switcher-button'));
    assertEquals(
        'Google Search,Third Endpoint',
        buttons.map(b => b.getAttribute('aria-label')).join());

    buttons[1]!.click();
    await overlay.updateComplete;

    assertEquals('Ask Third', input.placeholder);
    buttons = Array.from(overlay.shadowRoot.querySelectorAll<HTMLElement>(
        '.endpoint-switcher-button'));
    assertEquals(
        'Gemini,Google Search',
        buttons.map(b => b.getAttribute('aria-label')).join());
  });

  test('EscapeOnEndpointSwitcherButtonDismissesOverlay', async () => {
    await selectRegionForPrompt();
    await setEndpoints(twoEndpoints(), PromptEndpointId.kGemini);
    const switcherButton = overlay.shadowRoot.querySelector<HTMLElement>(
        '.endpoint-switcher-button')!;
    assertTrue(!!switcherButton);

    switcherButton.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Escape', bubbles: true, cancelable: true}));
    assertEquals(1, handler.getCallCount('dismissOverlay'));
  });

  test('EndpointSwitchAnimatesSlideInAndCleansUp', async () => {
    await selectRegionForPrompt();
    await setEndpoints(twoEndpoints(), PromptEndpointId.kGemini);
    const pill =
        overlay.shadowRoot.querySelector<HTMLElement>('.searchbox-pill')!;
    const switcherButton = overlay.shadowRoot.querySelector<HTMLElement>(
        '.endpoint-switcher-button')!;
    const initialPillRect = pill.getBoundingClientRect();
    const initialButtonRect = switcherButton.getBoundingClientRect();
    assertTrue(initialPillRect.width > 0);
    assertTrue(initialButtonRect.left >= initialPillRect.right);

    await withReducedMotion(false, async () => {
      switcherButton.click();
      await overlay.updateComplete;
      await Promise.resolve();

      const pillAnimations = pill.getAnimations();
      assertEquals(1, pillAnimations.length);
      const capAnim = pillAnimations[0]!;
      capAnim.pause();

      assertTrue(!!overlay.shadowRoot.querySelector('.searchbox-pill-ghost'));
      assertTrue(!!overlay.shadowRoot.querySelector('.searchbox-pill-spacer'));
      // Ghost must never duplicate element IDs.
      assertEquals(
          1, overlay.shadowRoot.querySelectorAll('#promptInput').length);
      assertEquals(
          1, overlay.shadowRoot.querySelectorAll('#endpointLiveRegion').length);

      // At t=0 the pill starts at the circle button's position on the right.
      capAnim.currentTime = 0;
      const startRect = pill.getBoundingClientRect();
      assertTrue(Math.abs(startRect.left - initialButtonRect.left) <= 2);
      assertTrue(Math.abs(startRect.width - initialButtonRect.width) <= 2);

      // Mid-animation the pill has slid partway left and expanded.
      capAnim.currentTime = 100;
      const midRect = pill.getBoundingClientRect();
      assertTrue(midRect.left < startRect.left);
      assertTrue(midRect.left > initialPillRect.left);
      assertTrue(midRect.width > startRect.width);
      assertTrue(midRect.width < initialPillRect.width);

      await finishAllOverlayAnimations();

      assertEquals(
          null, overlay.shadowRoot.querySelector('.searchbox-pill-ghost'));
      assertEquals(
          null, overlay.shadowRoot.querySelector('.searchbox-pill-spacer'));
      assertEquals('', pill.style.position);
    });
  });

  test('EndpointSwitchAnimatesSlideInFromLeftInRtl', async () => {
    overlay.dir = 'rtl';
    await selectRegionForPrompt();
    await setEndpoints(twoEndpoints(), PromptEndpointId.kGemini);
    const pill =
        overlay.shadowRoot.querySelector<HTMLElement>('.searchbox-pill')!;
    const switcherButton = overlay.shadowRoot.querySelector<HTMLElement>(
        '.endpoint-switcher-button')!;
    const initialPillRect = pill.getBoundingClientRect();
    const initialButtonRect = switcherButton.getBoundingClientRect();
    assertTrue(initialButtonRect.right <= initialPillRect.left);

    await withReducedMotion(false, async () => {
      switcherButton.click();
      await overlay.updateComplete;
      await Promise.resolve();

      const pillAnimations = pill.getAnimations();
      assertEquals(1, pillAnimations.length);
      const capAnim = pillAnimations[0]!;
      capAnim.pause();

      // At t=0 the pill starts at the circle button on the left in RTL.
      capAnim.currentTime = 0;
      const startRect = pill.getBoundingClientRect();
      assertTrue(Math.abs(startRect.right - initialButtonRect.right) <= 2);
      assertTrue(Math.abs(startRect.width - initialButtonRect.width) <= 2);

      // Mid-animation the pill expands rightward toward its resting right edge.
      capAnim.currentTime = 100;
      const midRect = pill.getBoundingClientRect();
      assertTrue(midRect.right > startRect.right);
      assertTrue(midRect.right < initialPillRect.right);
      assertTrue(midRect.width > startRect.width);
      assertTrue(midRect.width < initialPillRect.width);

      await finishAllOverlayAnimations();

      assertEquals(
          null, overlay.shadowRoot.querySelector('.searchbox-pill-ghost'));
      assertEquals(
          null, overlay.shadowRoot.querySelector('.searchbox-pill-spacer'));
      assertEquals('', pill.style.position);
    });
  });

  test('EndpointSwitchAnimatesCircleButtonsWithThreeEndpoints', async () => {
    await selectRegionForPrompt();
    const threeEndpoints: PromptEndpointInfo[] = [
      ...twoEndpoints(),
      {
        id: PromptEndpointId.kGemini,
        name: 'Third Endpoint',
        placeholder: 'Ask Third',
      },
    ];
    await setEndpoints(threeEndpoints, PromptEndpointId.kGemini);

    await withReducedMotion(false, async () => {
      const buttonsBefore = overlay.shadowRoot.querySelectorAll<HTMLElement>(
          '.endpoint-switcher-button');
      assertEquals(2, buttonsBefore.length);
      // Click Google Search: Third Endpoint FLIP-slides toward the start edge
      // with no delay, while Gemini enters from the far end with a delay.
      buttonsBefore[0]!.click();
      await overlay.updateComplete;
      await Promise.resolve();

      const buttonsAfter =
          Array.from(overlay.shadowRoot.querySelectorAll<HTMLElement>(
              '.endpoint-switcher-button'));
      assertEquals(
          'Third Endpoint,Gemini',
          buttonsAfter.map(b => b.getAttribute('aria-label')).join());

      const flipAnims = buttonsAfter[0]!.getAnimations();
      const enterAnims = buttonsAfter[1]!.getAnimations();
      assertEquals(1, flipAnims.length);
      assertEquals(1, enterAnims.length);
      assertEquals(0, flipAnims[0]!.effect!.getTiming().delay);
      assertTrue(Number(enterAnims[0]!.effect!.getTiming().delay) > 0);

      await finishAllOverlayAnimations();

      assertEquals(
          0,
          overlay.shadowRoot.querySelectorAll('.searchbox-pill-ghost').length);
      assertEquals(
          0,
          overlay.shadowRoot.querySelectorAll('.searchbox-pill-spacer').length);
    });
  });

  test(
      'RapidEndpointSwitchCancelsInFlightAnimationWithoutLeaking', async () => {
        await selectRegionForPrompt();
        await setEndpoints(twoEndpoints(), PromptEndpointId.kGemini);
        const pill =
            overlay.shadowRoot.querySelector<HTMLElement>('.searchbox-pill')!;
        const input =
            overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;

        await withReducedMotion(false, async () => {
          input.dispatchEvent(new KeyboardEvent(
              'keydown', {key: 'Tab', bubbles: true, cancelable: true}));
          await overlay.updateComplete;
          await Promise.resolve();
          assertEquals(
              1,
              overlay.shadowRoot.querySelectorAll('.searchbox-pill-ghost')
                  .length);
          assertEquals(
              1,
              overlay.shadowRoot.querySelectorAll('.searchbox-pill-spacer')
                  .length);

          // Interrupt the in-flight animation with a second Tab switch.
          input.dispatchEvent(new KeyboardEvent(
              'keydown', {key: 'Tab', bubbles: true, cancelable: true}));
          await overlay.updateComplete;
          await Promise.resolve();
          assertEquals(
              1,
              overlay.shadowRoot.querySelectorAll('.searchbox-pill-ghost')
                  .length);
          assertEquals(
              1,
              overlay.shadowRoot.querySelectorAll('.searchbox-pill-spacer')
                  .length);

          await finishAllOverlayAnimations();

          assertEquals('Ask Gemini', input.placeholder);
          assertEquals(
              0,
              overlay.shadowRoot.querySelectorAll('.searchbox-pill-ghost')
                  .length);
          assertEquals(
              0,
              overlay.shadowRoot.querySelectorAll('.searchbox-pill-spacer')
                  .length);
          assertEquals('', pill.style.position);
        });
      });

  test('EndpointSwitchSkipsAnimationWithReducedMotion', async () => {
    await selectRegionForPrompt();
    await setEndpoints(twoEndpoints(), PromptEndpointId.kGemini);
    const pill =
        overlay.shadowRoot.querySelector<HTMLElement>('.searchbox-pill')!;
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;

    await withReducedMotion(true, async () => {
      input.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'Tab', bubbles: true, cancelable: true}));
      await overlay.updateComplete;
      await Promise.resolve();

      assertEquals('Ask Google', input.placeholder);
      assertEquals(
          0,
          overlay.shadowRoot.querySelectorAll('.searchbox-pill-ghost').length);
      assertEquals(
          0,
          overlay.shadowRoot.querySelectorAll('.searchbox-pill-spacer').length);
      assertEquals(0, pill.getAnimations().length);
    });
  });

  test('EndpointSwitchAnimationCleansUpOnTeardown', async () => {
    await withReducedMotion(false, async () => {
      async function startInFlightSwitch() {
        await selectRegionForPrompt();
        await setEndpoints(twoEndpoints(), PromptEndpointId.kGemini);
        const switcherButton = overlay.shadowRoot.querySelector<HTMLElement>(
            '.endpoint-switcher-button')!;
        switcherButton.click();
        await overlay.updateComplete;
        await Promise.resolve();
        assertEquals(
            1,
            overlay.shadowRoot.querySelectorAll('.searchbox-pill-ghost')
                .length);
        assertEquals(
            1,
            overlay.shadowRoot.querySelectorAll('.searchbox-pill-spacer')
                .length);
      }

      const assertCleanedUp = () => {
        assertEquals(
            0,
            overlay.shadowRoot.querySelectorAll('.searchbox-pill-ghost')
                .length);
        assertEquals(
            0,
            overlay.shadowRoot.querySelectorAll('.searchbox-pill-spacer')
                .length);
        const pill =
            overlay.shadowRoot.querySelector<HTMLElement>('.searchbox-pill');
        if (pill) {
          assertEquals('', pill.style.position);
        }
      };

      // 1. Endpoint list update from browser cancels in-flight animation.
      await startInFlightSwitch();
      await setEndpoints(twoEndpoints(), PromptEndpointId.kGemini);
      assertCleanedUp();

      // 2. Hiding the floating prompt cancels in-flight animation.
      await startInFlightSwitch();
      overlay.showFloatingPrompt = false;
      await overlay.updateComplete;
      assertCleanedUp();
      overlay.showFloatingPrompt = true;
      await overlay.updateComplete;

      // 3. Disabling the prompt box cancels in-flight animation.
      await startInFlightSwitch();
      overlay.enableSelectionOverlayPromptBox = false;
      await overlay.updateComplete;
      assertCleanedUp();
      overlay.enableSelectionOverlayPromptBox = true;
      await overlay.updateComplete;

      // 4. Disconnecting the overlay element cancels in-flight animation.
      await startInFlightSwitch();
      overlay.remove();
      assertCleanedUp();
    });
  });

  test('TabAndShiftTabCycleDirectionWithThreeEndpoints', async () => {
    const threeEndpoints: PromptEndpointInfo[] = [
      ...twoEndpoints(),
      {
        id: PromptEndpointId.kGemini,
        name: 'Third Endpoint',
        placeholder: 'Ask Third',
      },
    ];
    await setEndpoints(threeEndpoints, PromptEndpointId.kGemini);

    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    input.focus();
    assertEquals('Ask Gemini', input.placeholder);

    // Forward cycling: 0 -> 1 -> 2 -> 0.
    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Tab', bubbles: true, cancelable: true}));
    await overlay.updateComplete;
    assertEquals('Ask Google', input.placeholder);

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Tab', bubbles: true, cancelable: true}));
    await overlay.updateComplete;
    assertEquals('Ask Third', input.placeholder);

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Tab', bubbles: true, cancelable: true}));
    await overlay.updateComplete;
    assertEquals('Ask Gemini', input.placeholder);

    // Backward cycling: 0 -> 2 -> 1 -> 0.
    input.dispatchEvent(new KeyboardEvent(
        'keydown',
        {key: 'Tab', shiftKey: true, bubbles: true, cancelable: true}));
    await overlay.updateComplete;
    assertEquals('Ask Third', input.placeholder);

    input.dispatchEvent(new KeyboardEvent(
        'keydown',
        {key: 'Tab', shiftKey: true, bubbles: true, cancelable: true}));
    await overlay.updateComplete;
    assertEquals('Ask Google', input.placeholder);

    input.dispatchEvent(new KeyboardEvent(
        'keydown',
        {key: 'Tab', shiftKey: true, bubbles: true, cancelable: true}));
    await overlay.updateComplete;
    assertEquals('Ask Gemini', input.placeholder);
  });

  test('SetPromptEndpointsUpdatesLiveRegionWithoutStealingFocus', async () => {
    // Give the overlay a real selection so `floatingPromptResizeObserver_`
    // keeps `showFloatingPrompt` true across async frames (`flushForTesting`).
    page.setPostRegionSelections([{
      id: REGION_ID,
      shape: {rect: {x: 0.5, y: 0.5, width: 0.2, height: 0.2}},
    }]);
    await page.$.flushForTesting();
    await waitAfterNextRender(overlay);
    assertTrue(overlay.showFloatingPrompt);

    await setEndpoints(twoEndpoints(), PromptEndpointId.kGemini);

    const liveRegion =
        overlay.shadowRoot.querySelector<HTMLElement>('#endpointLiveRegion')!;
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    assertEquals('', liveRegion.textContent?.trim());

    // Switch to Google Search via Tab -> announces "Google Search".
    input.focus();
    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Tab', bubbles: true, cancelable: true}));
    await overlay.updateComplete;
    assertEquals('Google Search', liveRegion.textContent?.trim());

    // Populate and focus an action chip after the `Tab`-triggered refetch
    // clears `suggestedActions`, then simulate `AdoptSession()` pinning
    // endpoints back to Gemini. Live region should announce "Gemini" and focus
    // must remain on the chip.
    overlay.suggestedActions = [createSuggestedAction('1', 'First chip')];
    await overlay.updateComplete;
    await waitAfterNextRender(overlay);
    assertTrue(overlay.showFloatingPrompt);
    const chip = overlay.shadowRoot.querySelector<HTMLElement>('.action-chip')!;
    chip.focus();
    assertEquals(chip, overlay.shadowRoot.activeElement);
    await setEndpoints([twoEndpoints()[0]!], PromptEndpointId.kGemini);
    assertEquals('Gemini', liveRegion.textContent?.trim());
    assertEquals(chip, overlay.shadowRoot.activeElement);

    // Re-sending endpoints with the same active endpoint clears stale text so
    // the next cycle announcement is not suppressed.
    await setEndpoints(twoEndpoints(), PromptEndpointId.kGemini);
    assertEquals('', liveRegion.textContent?.trim());
    assertEquals(chip, overlay.shadowRoot.activeElement);

    input.focus();
    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Tab', bubbles: true, cancelable: true}));
    await overlay.updateComplete;
    assertEquals('Google Search', liveRegion.textContent?.trim());
  });

  test('ArrowKeysNavigateBetweenInputAndActionChips', async () => {
    overlay.suggestedActions = [
      createSuggestedAction('1', 'First chip'),
      createSuggestedAction('2', 'Second chip'),
    ];
    overlay.promptAboveSelection = false;
    await overlay.updateComplete;

    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    const chips = Array.from(
        overlay.shadowRoot.querySelectorAll<HTMLElement>('.action-chip'));
    input.focus();
    assertEquals(input, overlay.shadowRoot.activeElement);

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, cancelable: true}));
    assertEquals(chips[0], overlay.shadowRoot.activeElement);

    chips[0]!.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, cancelable: true}));
    assertEquals(chips[1], overlay.shadowRoot.activeElement);

    chips[1]!.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowUp', bubbles: true, cancelable: true}));
    assertEquals(chips[0], overlay.shadowRoot.activeElement);

    chips[0]!.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowUp', bubbles: true, cancelable: true}));
    assertEquals(input, overlay.shadowRoot.activeElement);

    // Modifier keys on arrow events must not hijack text selection / shortcuts.
    for (const modifier of ['shiftKey', 'ctrlKey', 'altKey', 'metaKey'] as
         const) {
      const event = new KeyboardEvent('keydown', {
        key: 'ArrowDown',
        [modifier]: true,
        bubbles: true,
        cancelable: true,
      });
      input.dispatchEvent(event);
      assertFalse(event.defaultPrevented);
      assertEquals(input, overlay.shadowRoot.activeElement);
    }

    // When the prompt box is positioned above the selection, arrow directions
    // flip (ArrowUp moves from input into chips; ArrowDown moves back).
    overlay.promptAboveSelection = true;
    await overlay.updateComplete;
    input.focus();

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowUp', bubbles: true, cancelable: true}));
    assertEquals(chips[0], overlay.shadowRoot.activeElement);

    chips[0]!.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowUp', bubbles: true, cancelable: true}));
    assertEquals(chips[1], overlay.shadowRoot.activeElement);

    chips[1]!.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, cancelable: true}));
    assertEquals(chips[0], overlay.shadowRoot.activeElement);

    chips[0]!.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, cancelable: true}));
    assertEquals(input, overlay.shadowRoot.activeElement);

    // Modifier keys on arrow events targeting action chips must be ignored.
    chips[0]!.focus();
    for (const modifier of ['shiftKey', 'ctrlKey', 'altKey', 'metaKey'] as
         const) {
      const event = new KeyboardEvent('keydown', {
        key: 'ArrowDown',
        [modifier]: true,
        bubbles: true,
        cancelable: true,
      });
      chips[0]!.dispatchEvent(event);
      assertFalse(event.defaultPrevented);
      assertEquals(chips[0], overlay.shadowRoot.activeElement);
    }

    // Escape on an action chip dismisses the overlay.
    chips[0]!.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Escape', bubbles: true, cancelable: true}));
    assertEquals(1, handler.getCallCount('dismissOverlay'));
  });

  test('ArrowKeysIgnoredOnActionChipsWithoutPromptBox', async () => {
    overlay.enableSelectionOverlayPromptBox = false;
    overlay.enableSelectionOverlayPrompt = true;
    overlay.suggestedActions = [
      createSuggestedAction('1', 'First chip'),
      createSuggestedAction('2', 'Second chip'),
    ];
    overlay.promptAboveSelection = false;
    await overlay.updateComplete;

    const chips = Array.from(
        overlay.shadowRoot.querySelectorAll<HTMLElement>('.action-chip'));
    chips[0]!.focus();
    const upEvent = new KeyboardEvent(
        'keydown', {key: 'ArrowUp', bubbles: true, cancelable: true});
    chips[0]!.dispatchEvent(upEvent);
    assertFalse(upEvent.defaultPrevented);
    assertEquals(chips[0], overlay.shadowRoot.activeElement);
  });

  test('SingleEndpointTabDoesNotPreventDefault', async () => {
    await setEndpoints([twoEndpoints()[0]!], PromptEndpointId.kGemini);
    const input =
        overlay.shadowRoot.querySelector<HTMLInputElement>('#promptInput')!;
    const endpointIcon =
        overlay.shadowRoot.querySelector<HTMLElement>('#endpointIcon')!;
    assertEquals('true', endpointIcon.getAttribute('aria-hidden'));
    assertFalse(endpointIcon.hasAttribute('role'));
    assertFalse(endpointIcon.hasAttribute('title'));
    assertFalse(input.hasAttribute('aria-description'));
    assertFalse(input.hasAttribute('aria-keyshortcuts'));
    assertEquals(
        0,
        overlay.shadowRoot.querySelectorAll('.endpoint-switcher-button')
            .length);

    input.focus();
    const tabEvent = new KeyboardEvent(
        'keydown', {key: 'Tab', bubbles: true, cancelable: true});
    input.dispatchEvent(tabEvent);
    assertFalse(tabEvent.defaultPrevented);

    await setEndpoints(twoEndpoints(), PromptEndpointId.kGoogleSearch);
    assertEquals('true', endpointIcon.getAttribute('aria-hidden'));
    assertFalse(endpointIcon.hasAttribute('role'));
    assertEquals(
        1,
        overlay.shadowRoot.querySelectorAll('.endpoint-switcher-button')
            .length);
    assertEquals('Tab Shift+Tab', input.getAttribute('aria-keyshortcuts'));
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
      pageScrollbarWidth: 0,
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

  test('ClickWithRegionDoesNotDismiss', async () => {
    await showOverlay(/*enablePromptBox=*/ true);
    await setRegion(REGION_ID);
    assertTrue(overlay.showFloatingPrompt);

    await click(0.8, 0.8);

    assertEquals(0, handler.getCallCount('adjustRegion'));
    assertEquals(0, handler.getCallCount('dismissOverlay'));
    assertTrue(overlay.showFloatingPrompt);

    page.setSidePanelOpen(true);
    await page.$.flushForTesting();
    await overlay.updateComplete;
    await click(0.8, 0.8);

    assertEquals(0, handler.getCallCount('adjustRegion'));
    assertEquals(0, handler.getCallCount('dismissOverlay'));
    assertTrue(overlay.showFloatingPrompt);
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

  test('DragWithRegionAndSidePanelClosedDoesNotDismiss', async () => {
    await showOverlay(/*enablePromptBox=*/ true);
    await setRegion(REGION_ID);

    await drag(0.5, 0.5, 0.7, 0.7);

    assertEquals(0, handler.getCallCount('adjustRegion'));
    assertEquals(0, handler.getCallCount('dismissOverlay'));
    assertTrue(overlay.showFloatingPrompt);
  });

  test('ClickWithRegionWithoutPromptBoxDismisses', async () => {
    await showOverlay(/*enablePromptBox=*/ false);
    await setRegion(REGION_ID);

    await click(0.8, 0.8);

    assertEquals(0, handler.getCallCount('adjustRegion'));
    assertEquals(1, handler.getCallCount('dismissOverlay'));
  });

  test(
      'DragWithRegionAndSidePanelClosedWithoutPromptBoxDismisses', async () => {
        await showOverlay(/*enablePromptBox=*/ false);
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
  async function showOverlay({
    prompt = false,
    promptBox = false,
    dogfoodFeedback = false,
    scrollbarWidth = 0,
  }) {
    loadTimeData.overrideValues({
      enableSelectionOverlayPrompt: prompt,
      enableSelectionOverlayPromptBox: promptBox,
      enableSelectionOverlayDogfoodFeedback: dogfoodFeedback,
      enableShimmer: false,
      pageScrollbarWidth: scrollbarWidth,
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

  // Keep each nested suite small (~8 tests or fewer) and wire new suites in
  // selection_overlay_browsertest.cc so slow bots do not hit the per-test
  // launcher timeout.
  suite('PromptBox', () => {
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
      const [chip0Rect, chip1Rect] =
          chips().map(c => c.getBoundingClientRect());
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
      // The pill stays closest to the selection, with the first chip next to
      // it.
      const pillRect = query('.searchbox-pill').getBoundingClientRect();
      const [chip0Rect, chip1Rect] =
          chips().map(c => c.getBoundingClientRect());
      assertTrue(chip0Rect!.bottom <= pillRect.top);
      assertTrue(chip1Rect!.bottom <= chip0Rect!.top);
    });

    test('PromptBoxAboveSelectionRightAlignedNearBottomRight', async () => {
      await showOverlay({promptBox: true});
      const region = regionFromPx(
          window.innerWidth - 120, window.innerHeight - 120, 80, 60);
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
      // but not enough for the 120px maximum height with two 1-line
      // suggestions.
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
  });

  suite('PromptBoxFallback', () => {
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
      // Selection covering most of the viewport leaving insufficient space on
      // all four outside edges.
      const region = regionFromPx(
          60, 60, window.innerWidth - 120, window.innerHeight - 120);
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
      region = regionFromPx(
          60, 60, window.innerWidth - 120, window.innerHeight - 120);
      await select(region);
      rect = promptRect();
      selection = toViewport(region);
      assertNear(selection.right - MARGIN, rect.right, 'rtl within right');
      assertNear(selection.top + MARGIN, rect.top, 'rtl within top');

      // 8. Top-right corner of the window fallback in RTL.
      region = regionFromPx(
          window.innerWidth - 84, window.innerHeight - 100, 80, 50);
      await select(region);
      rect = promptRect();
      assertNear(window.innerWidth - MARGIN, rect.right, 'rtl fallback right');
      assertNear(MARGIN, rect.top, 'rtl fallback top');
    });

    test('DogfoodFeedbackButtonBesidePill', async () => {
      await showOverlay({promptBox: true, dogfoodFeedback: true});
      await select({x: 0.3, y: 0.3, width: 0.1, height: 0.1});

      const pillRect = query('.searchbox-pill').getBoundingClientRect();
      const buttonRect =
          query('#dogfoodFeedbackButton').getBoundingClientRect();
      assertNear(
          (pillRect.top + pillRect.bottom) / 2,
          (buttonRect.top + buttonRect.bottom) / 2, 'center y');
      assertTrue(buttonRect.left >= pillRect.right);
    });

    test('EndpointSwitcherButtonsBesidePillLtrAndRtl', async () => {
      await showOverlay({promptBox: true});
      page.setPromptEndpoints(
          [
            {
              id: PromptEndpointId.kGemini,
              name: 'Gemini',
              placeholder: 'Ask Gemini',
            },
            {
              id: PromptEndpointId.kGoogleSearch,
              name: 'Google Search',
              placeholder: 'Ask Google',
            },
          ],
          PromptEndpointId.kGemini);
      await page.$.flushForTesting();
      await select(
          {x: 0.3, y: 0.3, width: 0.1, height: 0.1}, [chip('Explain')]);

      let pillRect = query('.searchbox-pill').getBoundingClientRect();
      let switcherRect =
          query('.endpoint-switcher-button').getBoundingClientRect();
      assertNear(44, switcherRect.width, 'circle width');
      assertNear(44, switcherRect.height, 'circle height');
      assertNear(
          (pillRect.top + pillRect.bottom) / 2,
          (switcherRect.top + switcherRect.bottom) / 2, 'center y');
      assertTrue(switcherRect.left >= pillRect.right);
      assertNear(
          pillRect.left, chips()[0]!.getBoundingClientRect().left,
          'ltr chip aligned with pill');

      overlay.dir = 'rtl';
      await select(
          {x: 0.7, y: 0.3, width: 0.1, height: 0.1}, [chip('Explain')]);
      pillRect = query('.searchbox-pill').getBoundingClientRect();
      switcherRect = query('.endpoint-switcher-button').getBoundingClientRect();
      assertTrue(switcherRect.right <= pillRect.left);
      assertNear(
          pillRect.right, chips()[0]!.getBoundingClientRect().right,
          'rtl chip aligned with pill');
    });

    test('PromptBoxExtraRowWidthTriggersRightAlignmentFallback', async () => {
      await showOverlay({promptBox: true});
      // Leave 340px from selection.left to the right viewport margin: wide
      // enough for the 320px pill alone, but not wide enough once an inactive
      // endpoint switcher circle button (+50px) is beside it.
      const region =
          regionFromPx(window.innerWidth - MARGIN - 340, 100, 80, 60);
      const selection = toViewport(region);

      await select(region);
      assertNear(selection.left, promptRect().left, 'single endpoint left');

      page.setPromptEndpoints(
          [
            {
              id: PromptEndpointId.kGemini,
              name: 'Gemini',
              placeholder: 'Ask Gemini',
            },
            {
              id: PromptEndpointId.kGoogleSearch,
              name: 'Google Search',
              placeholder: 'Ask Google',
            },
          ],
          PromptEndpointId.kGemini);
      await page.$.flushForTesting();
      await select(region);
      assertNear(
          selection.right, promptRect().right,
          'two endpoints right-aligned fallback');
    });
  });

  suite('Chips', () => {
    test('ChipsAlignWithPillWhenPromptIsEndAligned', async () => {
      await showOverlay({promptBox: true, dogfoodFeedback: true});
      const chipActions = [chip('Summarize'), chip('Ask Gemini')];

      // Each prompt is placed by its trailing edge, so the dogfood button (not
      // the pill) sits at the edge. The chips still start where the pill
      // starts.
      const assertChipsAtPillStart = (message: string) => {
        const pillRect = query('.searchbox-pill').getBoundingClientRect();
        for (const chipRect of chips().map(c => c.getBoundingClientRect())) {
          if (overlay.dir === 'rtl') {
            assertNear(pillRect.right, chipRect.right, message);
          } else {
            assertNear(pillRect.left, chipRect.left, message);
          }
        }
      };

      // 2. Below the selection, right edges aligned.
      let region = regionFromPx(window.innerWidth - 120, 100, 80, 60);
      await select(region, chipActions);
      assertNear(toViewport(region).right, promptRect().right, 'below right');
      assertChipsAtPillStart('below right');

      // 6. To the left of a tall selection near the right edge.
      region = regionFromPx(
          window.innerWidth - 140, 100, 80, window.innerHeight - 200);
      await select(region, chipActions);
      assertNear(
          toViewport(region).left - MARGIN, promptRect().right, 'left of');
      assertChipsAtPillStart('left of');

      // 2 in RTL: below the selection, left edges aligned.
      overlay.dir = 'rtl';
      region = regionFromPx(40, 100, 80, 60);
      await select(region, chipActions);
      assertNear(toViewport(region).left, promptRect().left, 'rtl below left');
      assertChipsAtPillStart('rtl below left');
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
      const [chip0Rect, chip1Rect] =
          chips().map(c => c.getBoundingClientRect());
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

    test('ChipsRowKeepsMarginFromPageScrollbar', async () => {
      const region = {x: 0.95, y: 0.4, width: 0.06, height: 0.1};
      await showOverlay({prompt: true, scrollbarWidth: 20});
      await select(region, [chip('Summarize'), chip('Ask Gemini')]);

      assertNear(window.innerWidth - 20 - MARGIN, promptRect().right, 'right');
    });

    test('WideChipsRowKeepsMarginFromPageScrollbar', async () => {
      const region = {x: 0.5, y: 0.4, width: 0.1, height: 0.1};
      await showOverlay({prompt: true, scrollbarWidth: 20});
      // A row wider than the page is capped, not pushed under the scrollbar.
      await select(region, [chip('x'.repeat(500))]);

      const rect = promptRect();
      assertNear(MARGIN, rect.left, 'left');
      assertNear(window.innerWidth - 20 - MARGIN, rect.right, 'right');
    });
  });

  suite('Card', () => {
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

      // When between 320px and 380px (340px) is available from selection.left
      // to the right viewport margin, the inline fulfillment host shrinks to
      // 340px and stays left-aligned below the selection without jumping.
      region = regionFromPx(window.innerWidth - MARGIN - 340, 100, 80, 60);
      await select(region, [cardChip()]);
      await clickCardChip(fakeCard(100));
      hostRect = query('#inlineFulfillmentHost').getBoundingClientRect();
      selection = toViewport(region);
      assertNear(340, hostRect.width, 'shrunk width');
      assertNear(selection.left, promptRect().left, 'left stays aligned');
      assertNear(
          window.innerWidth - MARGIN, promptRect().right, 'right margin');

      // When right-aligned below the selection near the right edge, the 380px
      // inline fulfillment host stays aligned to selection.right and the prompt
      // box is hidden.
      region = regionFromPx(window.innerWidth - 120, 100, 80, 60);
      await select(region, [cardChip()]);
      await clickCardChip(fakeCard(100));
      hostRect = query('#inlineFulfillmentHost').getBoundingClientRect();
      selection = toViewport(region);
      assertNear(380, hostRect.width, 'right-aligned preferred width');
      assertNear(selection.right, hostRect.right, 'host right-aligned');
      assertEquals('none', getComputedStyle(query('.searchbox-row')).display);
    });

    test('PromptBoxHiddenWhenInlineFulfillmentShows', async () => {
      await showOverlay({promptBox: true, dogfoodFeedback: true});
      const region = regionFromPx(100, 100, 80, 60);
      await select(region, [cardChip()]);

      const searchboxRow = query('.searchbox-row');
      const chipsRow = query('.action-chips-row');
      const host =
          query('#inlineFulfillmentHost') as InlineFulfillmentHostElement;
      assertNotEquals('none', getComputedStyle(searchboxRow).display);
      assertNotEquals('none', getComputedStyle(chipsRow).display);
      assertEquals('none', getComputedStyle(host).display);

      // If consent is declined, the card does not show and the prompt box stays
      // visible.
      host.loaders = new Map([[
        FAKE_CARD,
        () => Promise.resolve({
          create: () => ({
            element: fakeCard(100),
            interfaceName: GenericAssociatedInterface.$interfaceName,
            remote: new GenericAssociatedInterfaceRemote(),
            consent: () => Promise.resolve(false),
          }),
        }),
      ]]);
      handler.resetResolver('executeSuggestedAction');
      chips()[0]!.click();
      await handler.whenCalled('executeSuggestedAction');
      await flushTasks();
      assertEquals(null, host.firstElementChild);
      assertNotEquals('none', getComputedStyle(searchboxRow).display);
      assertNotEquals('none', getComputedStyle(chipsRow).display);

      // Once the inline fulfillment card shows, the prompt box and chips are
      // hidden and the card takes their place.
      await clickCardChip(fakeCard(100));
      assertEquals('none', getComputedStyle(searchboxRow).display);
      assertEquals('none', getComputedStyle(chipsRow).display);
      assertNotEquals('none', getComputedStyle(host).display);
      const hostRect = host.getBoundingClientRect();
      const selection = toViewport(region);
      assertNear(selection.left, hostRect.left, 'card left');
      assertNear(selection.bottom + MARGIN, hostRect.top, 'card top');

      // Selecting a new region clears the card and shows the prompt box again.
      await select(regionFromPx(200, 100, 80, 60), [cardChip()]);
      assertNotEquals('none', getComputedStyle(searchboxRow).display);
      assertNotEquals('none', getComputedStyle(chipsRow).display);
      assertEquals('none', getComputedStyle(host).display);
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
      const corners = overlay.$.postSelectionRenderer.shadowRoot!
                          .querySelector<HTMLElement>('#selectionCorners');
      assertTrue(!!corners);
      const boxes =
          [...corners.querySelectorAll<HTMLElement>('.corner-hit-box')];
      assertEquals('none', getComputedStyle(corners).backgroundImage);
      assertEquals(4, boxes.length);
      assertTrue(boxes.every(box => getComputedStyle(box).display === 'none'));
    });
  });
});
