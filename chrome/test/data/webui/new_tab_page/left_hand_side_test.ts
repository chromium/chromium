// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <cr-left-hand-side> renders an <ntp-iframe>, which is defined in the lazily
// loaded part of the NTP.
import 'chrome://new-tab-page/lazy_load.js';

import type {IframeElement, ThreadsRailElement} from 'chrome://new-tab-page/lazy_load.js';
import {LhsClientAction} from 'chrome://new-tab-page/lazy_load.js';
import type {LeftHandSideElement} from 'chrome://new-tab-page/new_tab_page.js';
import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater, NtpLhsMessageType, ThreadsRailExpandedAction, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNear, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock, LAYOUT_TOLERANCE_PX} from './test_support.js';

const EXPANDED_LHS_ORIGIN = 'https://www.google.com';
const EXPANDED_LHS_URL = `${EXPANDED_LHS_ORIGIN}/search?nem=341`;

suite('NewTabPageLeftHandSideTest', () => {
  let leftHandSide: LeftHandSideElement;
  let metrics: MetricsTracker;
  let windowProxy: TestMock<WindowProxy>;

  function getExpandedIframe(): IframeElement|null {
    return leftHandSide.shadowRoot.querySelector<IframeElement>('#expanded');
  }

  function getGhostLoader(): HTMLElement|null {
    return leftHandSide.shadowRoot.querySelector<HTMLElement>('#ghostLoader');
  }

  // The ghost loader is only in the DOM while the rail is expanded, so a
  // missing ghost loader is not visible.
  function isGhostLoaderVisible(): boolean {
    const ghostLoader = getGhostLoader();
    return !!ghostLoader &&
        getComputedStyle(ghostLoader).visibility === 'visible';
  }

  function getExpandedIframeWindow(): Window {
    const iframeElement = getExpandedIframe();
    assertTrue(!!iframeElement);
    const iframe = iframeElement.shadowRoot.querySelector('iframe');
    assertTrue(!!iframe);
    assertTrue(!!iframe.contentWindow);
    return iframe.contentWindow;
  }

  function postLhsMessage(
      data: unknown, origin: string = EXPANDED_LHS_ORIGIN,
      source: MessageEventSource|null = getExpandedIframeWindow()) {
    window.dispatchEvent(new MessageEvent('message', {
      data,
      origin,
      source,
    }));
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // So that width and fade changes apply immediately instead of
    // transitioning.
    document.documentElement.style.setProperty('--cr-animations-disabled', '1');
    loadTimeData.overrideValues({
      expandedLhsUrl: EXPANDED_LHS_URL,
      lhsRailConfig: {items: []},
      threadsRailPreloadDebounceMs: 0,
    });
    metrics = fakeMetricsPrivate();
    windowProxy = installMock(WindowProxy);
    // Prevents the iframe from actually loading the LHS page.
    windowProxy.setResultFor('createIframeSrc', '');
  });

  teardown(() => {
    document.documentElement.style.removeProperty('--cr-animations-disabled');
    document.documentElement.dir = '';
  });

  async function createLeftHandSide() {
    leftHandSide = document.createElement('cr-left-hand-side');
    document.body.appendChild(leftHandSide);
    await microtasksFinished();
  }

  async function expandLeftHandSide() {
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded');
    assertTrue(!!folded);
    folded.click();
    await microtasksFinished();
  }

  test('updates color-scheme on isDark changes', async () => {
    await createLeftHandSide();
    await expandLeftHandSide();

    const iframe = getExpandedIframe();
    assertTrue(!!iframe);
    assertEquals('light', getComputedStyle(iframe).colorScheme);

    leftHandSide.isDark = true;
    await microtasksFinished();
    assertTrue(leftHandSide.hasAttribute('is-dark'));
    assertEquals('dark', getComputedStyle(iframe).colorScheme);

    leftHandSide.isDark = false;
    await microtasksFinished();
    assertFalse(leftHandSide.hasAttribute('is-dark'));
    assertEquals('light', getComputedStyle(iframe).colorScheme);
  });

  test('renders folded rail', async () => {
    await createLeftHandSide();

    const folded =
        leftHandSide.shadowRoot.querySelector<ThreadsRailElement>('#folded');
    assertTrue(!!folded);
    assertEquals('CR-THREADS-RAIL', folded.tagName);
  });

  test('renders expanded LHS iframe on expand', async () => {
    await createLeftHandSide();

    assertFalse(!!getExpandedIframe());
    assertEquals(0, windowProxy.getCallCount('ensureLazyLoaded'));
    await expandLeftHandSide();
    assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));

    const iframe = getExpandedIframe();
    assertTrue(!!iframe);
    assertEquals('NTP-IFRAME', iframe.tagName);
    assertEquals(EXPANDED_LHS_URL, iframe.src);
    assertEquals('clipboard-write; unbounded-element', iframe.allow);
    assertEquals('261px', iframe.style.width);
    // The host expands to the default expanded width while the ghost loader is
    // shown.
    assertEquals('261px', getHostWidthProperty());
    assertTrue(!!getGhostLoader());
    assertTrue(isGhostLoaderVisible());
    assertTrue(iframe.hidden);
    assertFalse(iframe.classList.contains('ready'));
  });

  test('updates expanded width and ready state on NTP_LHS_READY', async () => {
    await createLeftHandSide();
    await expandLeftHandSide();

    postLhsMessage({
      type: NtpLhsMessageType.NTP_LHS_READY,
      width: 320,
    });
    await microtasksFinished();

    const iframe = getExpandedIframe();
    assertTrue(!!iframe);
    assertEquals('320px', iframe.style.width);
    assertEquals('320px', getHostWidthProperty());
    assertFalse(iframe.hidden);
    assertTrue(iframe.classList.contains('ready'));
  });

  test('falls back to default width on invalid or missing width', async () => {
    await createLeftHandSide();
    await expandLeftHandSide();

    for (const invalidWidth of [undefined, 0, -100, '320']) {
      postLhsMessage({
        type: NtpLhsMessageType.NTP_LHS_READY,
        width: 320,
      });
      await microtasksFinished();
      const iframe = getExpandedIframe();
      assertTrue(!!iframe);
      assertEquals('320px', iframe.style.width);
      assertEquals('320px', getHostWidthProperty());

      postLhsMessage({
        type: NtpLhsMessageType.NTP_LHS_READY,
        width: invalidWidth,
      });
      await microtasksFinished();

      assertEquals('261px', iframe.style.width);
      assertEquals('261px', getHostWidthProperty());
      assertFalse(iframe.hidden);
    }
  });

  test('ignores messages from unexpected origin or invalid type', async () => {
    await createLeftHandSide();
    await expandLeftHandSide();

    postLhsMessage(
        {
          type: NtpLhsMessageType.NTP_LHS_READY,
          width: 320,
        },
        'https://evil.example.com');
    postLhsMessage(
        {
          type: NtpLhsMessageType.NTP_LHS_READY,
          width: 320,
        },
        EXPANDED_LHS_ORIGIN, window);
    postLhsMessage({
      type: 'UNKNOWN_MESSAGE_TYPE',
      width: 320,
    });
    postLhsMessage(null);
    await microtasksFinished();

    const iframe = getExpandedIframe();
    assertTrue(!!iframe);
    assertEquals('261px', iframe.style.width);
    assertTrue(iframe.hidden);
  });

  test(
      'does not listen for messages when expandedLhsUrl is empty', async () => {
        loadTimeData.overrideValues({expandedLhsUrl: ''});
        await createLeftHandSide();
        await expandLeftHandSide();

        const iframe = getExpandedIframe();
        assertTrue(!!iframe);
        postLhsMessage({
          type: NtpLhsMessageType.NTP_LHS_READY,
          width: 320,
        });
        await microtasksFinished();

        assertEquals('261px', iframe.style.width);
      });

  test('handles NTP_LHS_CLOSE message', async () => {
    await createLeftHandSide();

    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
    assertTrue(folded.offsetWidth > 0 && folded.offsetWidth < 261);
    const collapsedWidth = `${folded.offsetWidth}px`;

    await expandLeftHandSide();
    const expanded = getExpandedIframe();
    assertTrue(!!expanded);

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
    folded.click();
    await microtasksFinished();
    assertFalse(expanded.hidden);
    assertFalse(expanded.inert);
    assertTrue(folded.inert);
    assertEquals('320px', getHostWidthProperty());

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    await microtasksFinished();
    assertEquals(expanded, getExpandedIframe());
    assertFalse(expanded.hidden);
    assertTrue(expanded.inert);
    assertFalse(folded.inert);
    assertEquals(collapsedWidth, getHostWidthProperty());
  });

  test('expands LHS when folded rail is clicked', async () => {
    await createLeftHandSide();

    assertFalse(!!getExpandedIframe());
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
    assertFalse(isGhostLoaderVisible());
    const collapsedWidth = `${folded.offsetWidth}px`;

    await expandLeftHandSide();
    assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
    const initialExpanded = getExpandedIframe();
    assertTrue(!!initialExpanded);
    assertTrue(initialExpanded.hidden);
    assertEquals('hidden', getComputedStyle(folded).visibility);
    assertTrue(isGhostLoaderVisible());
    assertEquals('261px', getHostWidthProperty());

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
    await microtasksFinished();
    assertFalse(initialExpanded.hidden);
    assertFalse(initialExpanded.inert);
    assertTrue(folded.inert);
    assertFalse(isGhostLoaderVisible());
    assertEquals('320px', getHostWidthProperty());

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    await microtasksFinished();
    assertEquals(initialExpanded, getExpandedIframe());
    assertFalse(initialExpanded.hidden);
    assertTrue(initialExpanded.inert);
    assertFalse(folded.inert);
    assertFalse(isGhostLoaderVisible());
    assertEquals(collapsedWidth, getHostWidthProperty());

    await expandLeftHandSide();
    assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
    assertEquals(initialExpanded, getExpandedIframe());
    assertFalse(initialExpanded.hidden);
    assertFalse(initialExpanded.inert);
    assertTrue(folded.inert);
    assertFalse(isGhostLoaderVisible());
    assertEquals('320px', getHostWidthProperty());
  });

  test('expands LHS on expand-rail event', async () => {
    await createLeftHandSide();

    assertFalse(!!getExpandedIframe());
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
    assertFalse(isGhostLoaderVisible());

    folded.dispatchEvent(
        new CustomEvent('expand-rail', {bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
    const expanded = getExpandedIframe();
    assertTrue(!!expanded);
    assertTrue(expanded.hidden);
    assertEquals('hidden', getComputedStyle(folded).visibility);
    assertTrue(isGhostLoaderVisible());

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
    await microtasksFinished();
    assertFalse(expanded.hidden);
    assertFalse(expanded.inert);
    assertTrue(folded.inert);
    assertFalse(isGhostLoaderVisible());
  });

  ['pointerenter', 'focusin'].forEach(eventName => {
    test(`preloads expanded LHS iframe on ${eventName}`, async () => {
      await createLeftHandSide();

      assertFalse(!!getExpandedIframe());
      const folded =
          leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
      const collapsedWidth = `${folded.offsetWidth}px`;

      // Hovering or focusing the folded rail preloads the expanded iframe
      // while keeping the rail folded.
      folded.dispatchEvent(new Event(eventName));
      await microtasksFinished();
      assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
      const preloaded = getExpandedIframe();
      assertTrue(!!preloaded);
      assertTrue(preloaded.hidden);
      assertFalse(folded.inert);
      assertEquals(collapsedWidth, getHostWidthProperty());

      // Receiving NTP_LHS_READY while preloaded marks the iframe ready and
      // updates its width, but keeps it visually hidden and inert until
      // clicked.
      postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
      await microtasksFinished();
      assertEquals('320px', preloaded.style.width);
      assertTrue(preloaded.classList.contains('ready'));
      assertFalse(preloaded.hidden);
      assertTrue(preloaded.inert);
      assertFalse(folded.inert);
      assertEquals(collapsedWidth, getHostWidthProperty());

      // Clicking the folded rail expands immediately since the preloaded
      // iframe is already ready.
      folded.click();
      await microtasksFinished();
      assertFalse(preloaded.hidden);
      assertFalse(preloaded.inert);
      assertTrue(folded.inert);
      assertEquals('320px', getHostWidthProperty());
    });
  });

  ['pointerenter', 'focusin'].forEach(eventName => {
    test(
        `debounces preloading on ${eventName} when debounce delay is set`,
        async () => {
          loadTimeData.overrideValues({
            threadsRailPreloadDebounceMs: 100,
          });
          windowProxy.setResultFor('setTimeout', 42);
          await createLeftHandSide();

          const folded =
              leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;

          folded.dispatchEvent(new Event(eventName));
          await microtasksFinished();

          assertEquals(1, windowProxy.getCallCount('setTimeout'));
          const [callback, duration] = windowProxy.getArgs('setTimeout')[0];
          assertEquals(100, duration);
          assertEquals(0, windowProxy.getCallCount('ensureLazyLoaded'));
          assertFalse(!!getExpandedIframe());

          // Triggering the callback completes the preload.
          await callback();
          await microtasksFinished();
          assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
          assertTrue(!!getExpandedIframe());
        });
  });

  ['pointerleave', 'focusout'].forEach(eventName => {
    test(`cancels preload timer on ${eventName}`, async () => {
      loadTimeData.overrideValues({
        threadsRailPreloadDebounceMs: 100,
      });
      windowProxy.setResultFor('setTimeout', 42);
      await createLeftHandSide();

      const folded =
          leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;

      folded.dispatchEvent(new Event('pointerenter'));
      await microtasksFinished();
      assertEquals(1, windowProxy.getCallCount('setTimeout'));

      folded.dispatchEvent(new Event(eventName));
      await microtasksFinished();
      assertEquals(1, windowProxy.getCallCount('clearTimeout'));
      assertEquals(42, windowProxy.getArgs('clearTimeout')[0]);
      assertEquals(0, windowProxy.getCallCount('ensureLazyLoaded'));
      assertFalse(!!getExpandedIframe());
    });
  });

  test(
      'cancels pending preload timer and expands immediately on click',
      async () => {
        loadTimeData.overrideValues({
          threadsRailPreloadDebounceMs: 100,
        });
        windowProxy.setResultFor('setTimeout', 42);
        await createLeftHandSide();

        const folded =
            leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;

        folded.dispatchEvent(new Event('pointerenter'));
        await microtasksFinished();
        assertEquals(1, windowProxy.getCallCount('setTimeout'));
        assertEquals(0, windowProxy.getCallCount('ensureLazyLoaded'));

        folded.click();
        await microtasksFinished();
        assertEquals(1, windowProxy.getCallCount('clearTimeout'));
        assertEquals(42, windowProxy.getArgs('clearTimeout')[0]);
        assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
        assertTrue(!!getExpandedIframe());
      });

  test(
      'does not restart timer on repeated pointerenter or focusin while active',
      async () => {
        loadTimeData.overrideValues({
          threadsRailPreloadDebounceMs: 100,
        });
        windowProxy.setResultFor('setTimeout', 42);
        await createLeftHandSide();

        const folded =
            leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;

        folded.dispatchEvent(new Event('pointerenter'));
        await microtasksFinished();
        assertEquals(1, windowProxy.getCallCount('setTimeout'));

        folded.dispatchEvent(new Event('pointerenter'));
        folded.dispatchEvent(new Event('focusin'));
        await microtasksFinished();
        assertEquals(1, windowProxy.getCallCount('setTimeout'));
      });

  test('cancels pending preload timer when disconnected', async () => {
    loadTimeData.overrideValues({
      threadsRailPreloadDebounceMs: 100,
    });
    windowProxy.setResultFor('setTimeout', 42);
    await createLeftHandSide();

    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;

    folded.dispatchEvent(new Event('pointerenter'));
    await microtasksFinished();
    assertEquals(1, windowProxy.getCallCount('setTimeout'));

    leftHandSide.remove();
    assertEquals(1, windowProxy.getCallCount('clearTimeout'));
    assertEquals(42, windowProxy.getArgs('clearTimeout')[0]);
  });

  test(
      'waits for ensureLazyLoaded before stamping expanded iframe',
      async () => {
        const lazyLoadResolver = new PromiseResolver<void>();
        windowProxy.setResultFor('ensureLazyLoaded', lazyLoadResolver.promise);
        await createLeftHandSide();

        const folded =
            leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
        folded.click();
        await microtasksFinished();
        assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
        assertFalse(!!getExpandedIframe());

        lazyLoadResolver.resolve();
        await microtasksFinished();
        assertTrue(!!getExpandedIframe());
      });

  test(
      'handles folded rail item clicks without spurious expansion',
      async () => {
        loadTimeData.overrideValues({
          lhsRailConfig: {
            items: [
              {
                iconSvg: '<svg></svg>',
                name: 'AI Mode history',
                clientAction: LhsClientAction.EXPAND_RAIL,
              },
              {
                iconSvg: '<svg></svg>',
                name: 'Start new AI Mode search',
                clientAction: LhsClientAction.FOCUS_REALBOX_AIM,
              },
            ],
          },
        });
        await createLeftHandSide();

        const folded =
            leftHandSide.shadowRoot.querySelector<ThreadsRailElement>(
                '#folded')!;

        assertFalse(!!getExpandedIframe());
        assertFalse(isGhostLoaderVisible());

        // Clicking FOCUS_REALBOX_AIM bubbles open-composebox without expanding.
        const newSearchButton =
            folded.shadowRoot.querySelector<HTMLElement>('#railItem-1')!;
        const whenOpenComposebox =
            eventToPromise('open-composebox', leftHandSide);
        newSearchButton.click();
        await whenOpenComposebox;
        await microtasksFinished();
        assertEquals(0, windowProxy.getCallCount('ensureLazyLoaded'));
        assertFalse(!!getExpandedIframe());
        assertFalse(folded.inert);
        assertFalse(isGhostLoaderVisible());

        // Clicking EXPAND_RAIL expands the LHS iframe.
        const expandButton =
            folded.shadowRoot.querySelector<HTMLElement>('#railItem-0')!;
        expandButton.click();
        await microtasksFinished();
        assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
        const expanded = getExpandedIframe();
        assertTrue(!!expanded);
        assertTrue(expanded.hidden);
        assertEquals('hidden', getComputedStyle(folded).visibility);
        assertTrue(isGhostLoaderVisible());

        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
        await microtasksFinished();
        assertFalse(expanded.hidden);
        assertFalse(expanded.inert);
        assertTrue(folded.inert);
        assertFalse(isGhostLoaderVisible());
        assertEquals('320px', getHostWidthProperty());
      });

  test(
      'does not replay tooltip on NTP_LHS_CLOSE after expanding via rail item',
      async () => {
        loadTimeData.overrideValues({
          lhsRailConfig: {
            items: [
              {
                iconSvg: '<svg></svg>',
                name: 'Open sidebar',
                clientAction: LhsClientAction.EXPAND_RAIL,
              },
            ],
          },
        });
        await createLeftHandSide();

        const folded =
            leftHandSide.shadowRoot.querySelector<ThreadsRailElement>(
                '#folded')!;
        folded.dispatchEvent(new PointerEvent('pointerenter'));
        await microtasksFinished();
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
        await microtasksFinished();

        const expandButton =
            folded.shadowRoot.querySelector<HTMLElement>('#railItem-0')!;
        const initialTooltip =
            folded.shadowRoot.querySelector<CrTooltipElement>(
                'cr-tooltip[for="railItem-0"]')!;
        expandButton.dispatchEvent(new PointerEvent('pointerenter'));
        await eventToPromise('animationend', initialTooltip.$.tooltip);
        assertFalse(initialTooltip.$.tooltip.hidden);

        // Clicking expand starts cr-tooltip's fade-out animation and makes
        // #folded inert in the same turn, unmounting its tooltips.
        expandButton.click();
        await microtasksFinished();
        assertTrue(folded.inert);
        assertEquals(
            0, folded.shadowRoot.querySelectorAll('cr-tooltip').length);

        // Closing the expanded LHS restores #folded with fresh hidden tooltips
        // while suppressing stale post-layout pointer hit-testing until the
        // next real pointer move.
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
        await microtasksFinished();
        assertFalse(folded.inert);
        assertTrue(folded.hasAttribute('suppress-pointer-hover_'));
        assertEquals('none', getComputedStyle(expandButton).pointerEvents);
        const restoredTooltip =
            folded.shadowRoot.querySelector<CrTooltipElement>(
                'cr-tooltip[for="railItem-0"]')!;
        assertTrue(restoredTooltip.$.tooltip.hidden);
        assertFalse(
            restoredTooltip.$.tooltip.classList.contains('fade-in-animation'));
        assertFalse(
            restoredTooltip.$.tooltip.classList.contains('fade-out-animation'));

        // Once a real pointermove occurs, pointer hit-testing is restored.
        window.dispatchEvent(new PointerEvent('pointermove'));
        await microtasksFinished();
        assertFalse(folded.hasAttribute('suppress-pointer-hover_'));
        assertEquals('auto', getComputedStyle(expandButton).pointerEvents);
        assertTrue(restoredTooltip.$.tooltip.hidden);
      });

  test(
      'shows ghost loader and expands width before iframe is ready',
      async () => {
        await createLeftHandSide();

        const folded =
            leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
        assertTrue(folded.offsetWidth > 0 && folded.offsetWidth < 261);
        // The ghost loader is not in the DOM while the rail is folded.
        assertFalse(!!getGhostLoader());

        // Clicking the folded rail before the embedded page is ready hides the
        // folded rail, shows the ghost loader, and expands the host width
        // immediately.
        await expandLeftHandSide();
        const expanded = getExpandedIframe();
        assertTrue(!!expanded);
        assertTrue(expanded.hidden);
        assertEquals('hidden', getComputedStyle(folded).visibility);
        const ghostLoader = getGhostLoader();
        assertTrue(!!ghostLoader);
        assertTrue(isGhostLoaderVisible());
        assertEquals('261px', ghostLoader.style.width);
        // The ghost loader exposes a named, busy region to screen readers,
        // while its decorative placeholders stay hidden from the a11y tree.
        assertEquals('region', ghostLoader.getAttribute('role'));
        assertEquals('true', ghostLoader.getAttribute('aria-busy'));
        assertEquals(
            loadTimeData.getString('aimThreadsHistoryLabel'),
            ghostLoader.getAttribute('aria-label'));
        assertFalse(ghostLoader.hasAttribute('aria-hidden'));
        assertTrue(!!ghostLoader.shadowRoot);
        assertEquals(3, ghostLoader.shadowRoot.children.length);
        for (const child of ghostLoader.shadowRoot.children) {
          assertEquals('true', child.getAttribute('aria-hidden'));
        }
        assertEquals('261px', getHostWidthProperty());

        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
        await microtasksFinished();
        assertFalse(expanded.hidden);
        assertFalse(expanded.inert);
        assertTrue(folded.inert);
        assertFalse(isGhostLoaderVisible());
        assertEquals('320px', getHostWidthProperty());

        // Collapsing the rail removes the ghost loader from the DOM.
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
        await microtasksFinished();
        assertFalse(!!getGhostLoader());
      });

  test('toggles folded and expanded visibility on open and close', async () => {
    await createLeftHandSide();

    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
    folded.dispatchEvent(new Event('pointerenter'));
    await microtasksFinished();
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 261});
    await microtasksFinished();

    const expanded = getExpandedIframe()!;
    // setup() sets --cr-animations-disabled: 1, disabling transitions.
    assertEquals('0s', getComputedStyle(leftHandSide).transitionDuration);
    assertEquals('0s', getComputedStyle(folded).transitionDuration);
    assertEquals('0s', getComputedStyle(expanded).transitionDuration);

    // Overriding --cr-animations-disabled enables transitions via
    // @container not style(--cr-animations-disabled: 1). Set it to 0
    // instead of removing it, so that the test does not depend on
    // prefers-reduced-motion (cr_shared_vars.css sets it to 1 under
    // prefers-reduced-motion: reduce).
    document.documentElement.style.setProperty('--cr-animations-disabled', '0');
    assertEquals('0.3s', getComputedStyle(leftHandSide).transitionDuration);
    assertEquals('0.15s, 0s', getComputedStyle(folded).transitionDuration);
    assertEquals('0.24s, 0s', getComputedStyle(expanded).transitionDuration);
    document.documentElement.style.setProperty('--cr-animations-disabled', '1');

    assertFalse(folded.inert);
    assertEquals('visible', getComputedStyle(folded).visibility);
    assertEquals('1', getComputedStyle(folded).opacity);
    assertFalse(expanded.hidden);
    assertTrue(expanded.inert);
    assertEquals('hidden', getComputedStyle(expanded).visibility);
    assertEquals('0', getComputedStyle(expanded).opacity);

    folded.click();
    await microtasksFinished();
    assertTrue(folded.inert);
    assertEquals('hidden', getComputedStyle(folded).visibility);
    assertEquals('0', getComputedStyle(folded).opacity);
    assertFalse(expanded.inert);
    assertEquals('visible', getComputedStyle(expanded).visibility);
    assertEquals('1', getComputedStyle(expanded).opacity);

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    await microtasksFinished();
    assertFalse(folded.inert);
    assertEquals('visible', getComputedStyle(folded).visibility);
    assertEquals('1', getComputedStyle(folded).opacity);
    assertFalse(expanded.hidden);
    assertTrue(expanded.inert);
    assertEquals('hidden', getComputedStyle(expanded).visibility);
    assertEquals('0', getComputedStyle(expanded).opacity);
  });

  test(
      'keeps expanded width stationary and preserves tooltip overflow',
      async () => {
        await createLeftHandSide();

        const folded =
            leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
        // Neither :host nor #folded may create an absolute containing block or
        // resting stacking context, so cr-tooltip inside #folded can overflow
        // above #rhsContent.
        assertEquals('static', getComputedStyle(leftHandSide).position);
        assertEquals('static', getComputedStyle(folded).position);
        assertEquals('1', getComputedStyle(folded).opacity);

        await expandLeftHandSide();
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 261});
        await microtasksFinished();

        const expanded = getExpandedIframe()!;
        leftHandSide.style.width = '180px';
        const lhsLtrRect = leftHandSide.getBoundingClientRect();
        const expandedLtrRect = expanded.getBoundingClientRect();
        assertNear(180, lhsLtrRect.width, LAYOUT_TOLERANCE_PX);
        assertNear(261, expandedLtrRect.width, LAYOUT_TOLERANCE_PX);
        assertNear(lhsLtrRect.left, expandedLtrRect.left, LAYOUT_TOLERANCE_PX);

        document.documentElement.dir = 'rtl';
        const lhsRtlRect = leftHandSide.getBoundingClientRect();
        const expandedRtlRect = expanded.getBoundingClientRect();
        assertNear(
            lhsRtlRect.right, expandedRtlRect.right, LAYOUT_TOLERANCE_PX);
      });

  test('does not post NTP_LHS_SET_THEME while collapsed', async () => {
    await createLeftHandSide();
    ColorChangeUpdater.forDocument().eventTarget.dispatchEvent(
        new CustomEvent(COLOR_PROVIDER_CHANGED));
    assertEquals(0, windowProxy.getCallCount('postMessage'));
  });

  function getHostWidthProperty(): string {
    return leftHandSide.style.getPropertyValue('--cr-left-hand-side-width');
  }

  test('handles NTP_LHS_NEW_THREAD message', async () => {
    await createLeftHandSide();
    await expandLeftHandSide();

    const whenOpenComposebox = eventToPromise('open-composebox', leftHandSide);
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_NEW_THREAD});
    await whenOpenComposebox;
  });

  test('handles NTP_LHS_NAVIGATE message', async () => {
    await createLeftHandSide();
    await expandLeftHandSide();

    const targetUrl = 'https://www.google.com/search?q=hello';
    postLhsMessage({
      type: NtpLhsMessageType.NTP_LHS_NAVIGATE,
      url: targetUrl,
    });

    assertEquals(1, windowProxy.getCallCount('navigate'));
    assertEquals(targetUrl, windowProxy.getArgs('navigate')[0]);
  });

  test('ignores NTP_LHS_NAVIGATE with invalid or non-http(s) URL', async () => {
    await createLeftHandSide();
    await expandLeftHandSide();

    postLhsMessage({
      type: NtpLhsMessageType.NTP_LHS_NAVIGATE,
      url: 'javascript:alert(1)',
    });
    postLhsMessage({
      type: NtpLhsMessageType.NTP_LHS_NAVIGATE,
      url: 'not a valid url',
    });

    assertEquals(0, windowProxy.getCallCount('navigate'));
  });

  test('posts NTP_LHS_SET_THEME message on theme change', async () => {
    await createLeftHandSide();
    await expandLeftHandSide();
    leftHandSide.isDark = true;
    leftHandSide.style.setProperty(
        '--color-new-tab-page-threads-rail-primary-font',
        'rgba(31, 31, 31, 1)');
    leftHandSide.style.setProperty(
        '--color-new-tab-page-threads-rail-background',
        'rgba(241, 243, 244, 1)');

    ColorChangeUpdater.forDocument().eventTarget.dispatchEvent(
        new CustomEvent(COLOR_PROVIDER_CHANGED));

    assertEquals(1, windowProxy.getCallCount('postMessage'));
    const [iframe, message, targetOrigin] =
        windowProxy.getArgs('postMessage')[0];
    const expandedIframe = getExpandedIframe();
    assertTrue(!!expandedIframe);
    assertEquals(expandedIframe.shadowRoot.querySelector('#iframe'), iframe);
    assertDeepEquals(
        {
          type: NtpLhsMessageType.NTP_LHS_SET_THEME,
          theme: {
            primaryFont: 'rgba(31, 31, 31, 1)',
            railBackground: 'rgba(241, 243, 244, 1)',
          },
          isDark: true,
          iconSize: 20,
        },
        message);
    assertEquals(EXPANDED_LHS_ORIGIN, targetOrigin);
  });

  test('posts NTP_LHS_SET_THEME on NTP_LHS_READY', async () => {
    await createLeftHandSide();
    await expandLeftHandSide();
    leftHandSide.isDark = false;
    leftHandSide.style.setProperty(
        '--color-new-tab-page-threads-rail-primary-font',
        'rgba(31, 31, 31, 1)');
    leftHandSide.style.setProperty(
        '--color-new-tab-page-threads-rail-background',
        'rgba(241, 243, 244, 1)');

    postLhsMessage({
      type: NtpLhsMessageType.NTP_LHS_READY,
      width: 320,
    });
    await microtasksFinished();

    assertEquals(1, windowProxy.getCallCount('postMessage'));
    const [iframe, message, targetOrigin] =
        windowProxy.getArgs('postMessage')[0];
    const expandedIframe = getExpandedIframe();
    assertTrue(!!expandedIframe);
    assertEquals(expandedIframe.shadowRoot.querySelector('#iframe'), iframe);
    assertDeepEquals(
        {
          type: NtpLhsMessageType.NTP_LHS_SET_THEME,
          theme: {
            primaryFont: 'rgba(31, 31, 31, 1)',
            railBackground: 'rgba(241, 243, 244, 1)',
          },
          isDark: false,
          iconSize: 20,
        },
        message);
    assertEquals(EXPANDED_LHS_ORIGIN, targetOrigin);
  });

  test('records expanded iframe loaded and shown metrics', async () => {
    await createLeftHandSide();
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;

    assertEquals(
        0, metrics.count('NewTabPage.ThreadsRail.ExpandedIframeLoaded'));
    assertEquals(0, metrics.count('NewTabPage.ThreadsRail.ExpandedShown'));

    // Preloading and receiving NTP_LHS_READY while collapsed records
    // ExpandedIframeLoaded once without recording ExpandedShown.
    folded.dispatchEvent(new Event('pointerenter'));
    await microtasksFinished();
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 280});
    await microtasksFinished();
    assertEquals(
        1, metrics.count('NewTabPage.ThreadsRail.ExpandedIframeLoaded', true));
    assertEquals(0, metrics.count('NewTabPage.ThreadsRail.ExpandedShown'));

    // First expand records ExpandedShown.
    folded.click();
    await microtasksFinished();
    assertEquals(
        1, metrics.count('NewTabPage.ThreadsRail.ExpandedShown', true));

    // Closing and re-expanding records ExpandedShown again without
    // re-recording ExpandedIframeLoaded.
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    await microtasksFinished();
    assertEquals(
        1, metrics.count('NewTabPage.ThreadsRail.ExpandedShown', true));

    folded.click();
    await microtasksFinished();
    assertEquals(
        2, metrics.count('NewTabPage.ThreadsRail.ExpandedShown', true));
    assertEquals(
        1, metrics.count('NewTabPage.ThreadsRail.ExpandedIframeLoaded', true));
  });

  test(
      'records ExpandedShown once when clicked repeatedly before ready',
      async () => {
        await createLeftHandSide();
        const folded =
            leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;

        folded.click();
        folded.click();
        await microtasksFinished();
        assertEquals(0, metrics.count('NewTabPage.ThreadsRail.ExpandedShown'));

        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
        await microtasksFinished();
        assertEquals(
            1, metrics.count('NewTabPage.ThreadsRail.ExpandedShown', true));
        assertEquals(
            1,
            metrics.count('NewTabPage.ThreadsRail.ExpandedIframeLoaded', true));
      });

  test('records ExpandedAction metrics', async () => {
    await createLeftHandSide();
    await expandLeftHandSide();
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
    await microtasksFinished();

    // 1. NTP_LHS_NEW_THREAD records NEW_THREAD_CLICKED.
    const whenOpenComposebox = eventToPromise('open-composebox', leftHandSide);
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_NEW_THREAD});
    await whenOpenComposebox;
    assertEquals(1, metrics.count('NewTabPage.ThreadsRail.ExpandedAction'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.ExpandedAction',
            ThreadsRailExpandedAction.NEW_THREAD_CLICKED));

    // 2. Invalid NTP_LHS_NAVIGATE URLs do not record NAVIGATE_CLICKED, while
    // valid http(s) URLs do.
    postLhsMessage({
      type: NtpLhsMessageType.NTP_LHS_NAVIGATE,
      url: 'javascript:alert(1)',
    });
    postLhsMessage({
      type: NtpLhsMessageType.NTP_LHS_NAVIGATE,
      url: 'https://www.google.com/search?q=hello',
    });
    assertEquals(2, metrics.count('NewTabPage.ThreadsRail.ExpandedAction'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.ExpandedAction',
            ThreadsRailExpandedAction.NAVIGATE_CLICKED));

    // 3. NTP_LHS_CLOSE records CLOSE_CLICKED only while expanded.
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    await microtasksFinished();
    assertEquals(3, metrics.count('NewTabPage.ThreadsRail.ExpandedAction'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.ExpandedAction',
            ThreadsRailExpandedAction.CLOSE_CLICKED));
  });
});
