// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <cr-left-hand-side> renders an <ntp-iframe>, which is defined in the lazily
// loaded part of the NTP.
import 'chrome://new-tab-page/lazy_load.js';

import type {IframeElement} from 'chrome://new-tab-page/lazy_load.js';
import {LhsClientAction} from 'chrome://new-tab-page/lazy_load.js';
import type {LeftHandSideElement} from 'chrome://new-tab-page/new_tab_page.js';
import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater, NtpLhsMessageType, ThreadsRailExpandedAction, TooltipPosition, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNear, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock, keydown, LAYOUT_TOLERANCE_PX} from './test_support.js';

const EXPANDED_LHS_ORIGIN = 'https://www.google.com';
const EXPANDED_LHS_URL = `${EXPANDED_LHS_ORIGIN}/search?nem=341`;
const EXPAND_RAIL_CONFIG = {
  items: [
    {
      iconSvg: '<svg></svg>',
      name: 'Open sidebar',
      clientAction: LhsClientAction.EXPAND_RAIL,
    },
  ],
};

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

  function getGhostLoaderShadowRoot(): ShadowRoot {
    const ghostLoader = getGhostLoader();
    assertTrue(!!ghostLoader);
    const element =
        ghostLoader.querySelector<HTMLElement>('ntp-lhs-ghost-loader');
    assertTrue(!!element);
    assertTrue(!!element.shadowRoot);
    return element.shadowRoot;
  }

  function getExpandedIframeWindow(): Window {
    const iframeElement = getExpandedIframe();
    assertTrue(!!iframeElement);
    const iframe = iframeElement.shadowRoot.querySelector('iframe');
    assertTrue(!!iframe);
    assertTrue(!!iframe.contentWindow);
    return iframe.contentWindow;
  }

  function getFocusMessages() {
    return windowProxy.getArgs('postMessage')
        .filter(
            ([, message]) => message.type === NtpLhsMessageType.NTP_LHS_FOCUS);
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
    // Like new_tab_page.html, so that cr-tooltip's offsetParent (body) aligns
    // with the initial containing block at x = 0.
    document.body.style.margin = '0';
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
    document.body.style.margin = '';
    document.documentElement.style.removeProperty('--cr-animations-disabled');
    document.documentElement.dir = '';
    Reflect.deleteProperty(document, 'visibilityState');
  });

  // Overrides document.visibilityState and fires visibilitychange. Passing
  // false removes the override so that the native getter applies again.
  function setDocumentHidden(hidden: boolean) {
    if (hidden) {
      Object.defineProperty(
          document, 'visibilityState', {value: 'hidden', configurable: true});
    } else {
      Reflect.deleteProperty(document, 'visibilityState');
    }
    document.dispatchEvent(new Event('visibilitychange'));
  }

  async function createLeftHandSide() {
    leftHandSide = document.createElement('cr-left-hand-side');
    document.body.appendChild(leftHandSide);
    await microtasksFinished();
  }

  async function expandLeftHandSide() {
    leftHandSide.$.folded.click();
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

    assertEquals('CR-THREADS-RAIL', leftHandSide.$.folded.tagName);
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

    const folded = leftHandSide.$.folded;
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
    const folded = leftHandSide.$.folded;
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
    const folded = leftHandSide.$.folded;
    assertFalse(isGhostLoaderVisible());

    folded.fire('expand-rail', new MouseEvent('click'));
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
      const folded = leftHandSide.$.folded;
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

          const folded = leftHandSide.$.folded;

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

  [{enterEvent: 'pointerenter', leaveEvent: 'pointerleave'},
   {enterEvent: 'focusin', leaveEvent: 'focusout'},
  ].forEach(({enterEvent, leaveEvent}) => {
    test(`cancels preload timer on ${leaveEvent}`, async () => {
      loadTimeData.overrideValues({
        threadsRailPreloadDebounceMs: 100,
      });
      windowProxy.setResultFor('setTimeout', 42);
      await createLeftHandSide();

      const folded = leftHandSide.$.folded;

      folded.dispatchEvent(new Event(enterEvent));
      await microtasksFinished();
      assertEquals(1, windowProxy.getCallCount('setTimeout'));

      folded.dispatchEvent(new Event(leaveEvent));
      await microtasksFinished();
      assertEquals(1, windowProxy.getCallCount('clearTimeout'));
      assertEquals(42, windowProxy.getArgs('clearTimeout')[0]);
      assertEquals(0, windowProxy.getCallCount('ensureLazyLoaded'));
      assertFalse(!!getExpandedIframe());
    });
  });

  [{
    firstEnter: 'pointerenter',
    secondEnter: 'focusin',
    firstLeave: 'pointerleave',
    secondLeave: 'focusout',
  },
   {
     firstEnter: 'focusin',
     secondEnter: 'pointerenter',
     firstLeave: 'focusout',
     secondLeave: 'pointerleave',
   },
  ].forEach(({firstEnter, secondEnter, firstLeave, secondLeave}) => {
    test(
        `keeps preload timer on ${firstLeave} while active until ${
            secondLeave}`,
        async () => {
          loadTimeData.overrideValues({
            threadsRailPreloadDebounceMs: 100,
          });
          windowProxy.setResultFor('setTimeout', 42);
          await createLeftHandSide();

          const folded = leftHandSide.$.folded;

          folded.dispatchEvent(new Event(firstEnter));
          folded.dispatchEvent(new Event(secondEnter));
          await microtasksFinished();
          assertEquals(1, windowProxy.getCallCount('setTimeout'));

          // Ending only one interaction keeps the preload timer running.
          folded.dispatchEvent(new Event(firstLeave));
          await microtasksFinished();
          assertEquals(0, windowProxy.getCallCount('clearTimeout'));

          // Ending the remaining interaction cancels the preload timer.
          folded.dispatchEvent(new Event(secondLeave));
          await microtasksFinished();
          assertEquals(1, windowProxy.getCallCount('clearTimeout'));
          assertEquals(42, windowProxy.getArgs('clearTimeout')[0]);
          assertEquals(0, windowProxy.getCallCount('ensureLazyLoaded'));
          assertFalse(!!getExpandedIframe());
        });
  });

  test('keeps preload timer when focus moves within the rail', async () => {
    loadTimeData.overrideValues({
      threadsRailPreloadDebounceMs: 100,
    });
    windowProxy.setResultFor('setTimeout', 42);
    await createLeftHandSide();

    const folded = leftHandSide.$.folded;
    const child = document.createElement('button');
    folded.appendChild(child);

    folded.dispatchEvent(new FocusEvent('focusin'));
    await microtasksFinished();
    assertEquals(1, windowProxy.getCallCount('setTimeout'));

    // Focus moves to another element inside the rail.
    folded.dispatchEvent(new FocusEvent('focusout', {relatedTarget: child}));
    await microtasksFinished();
    assertEquals(0, windowProxy.getCallCount('clearTimeout'));

    // Focus leaves the rail.
    folded.dispatchEvent(new FocusEvent('focusout', {relatedTarget: null}));
    await microtasksFinished();
    assertEquals(1, windowProxy.getCallCount('clearTimeout'));
    assertEquals(42, windowProxy.getArgs('clearTimeout')[0]);
  });

  test(
      'keeps hover/focus interval when focus moves within the rail',
      async () => {
        await createLeftHandSide();
        const folded = leftHandSide.$.folded;
        const child = document.createElement('button');
        folded.appendChild(child);

        // Focus at t=100, move focus within the rail at t=300.
        windowProxy.setResultFor('now', 100);
        folded.dispatchEvent(new FocusEvent('focusin'));
        windowProxy.setResultFor('now', 300);
        folded.dispatchEvent(
            new FocusEvent('focusout', {relatedTarget: child}));
        folded.dispatchEvent(new FocusEvent('focusin'));
        await microtasksFinished();

        // Expand via keyboard at t=1000.
        windowProxy.setResultFor('now', 1000);
        folded.fire('expand-rail', new MouseEvent('click'));
        await microtasksFinished();

        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ThreadsRail.LongestFocusDurationBeforeExpand',
                900));
      });

  test(
      'cancels pending preload timer and expands immediately on click',
      async () => {
        loadTimeData.overrideValues({
          threadsRailPreloadDebounceMs: 100,
        });
        windowProxy.setResultFor('setTimeout', 42);
        await createLeftHandSide();

        const folded = leftHandSide.$.folded;

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

        const folded = leftHandSide.$.folded;

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

    const folded = leftHandSide.$.folded;

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

        const folded = leftHandSide.$.folded;
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

        const folded = leftHandSide.$.folded;

        assertFalse(!!getExpandedIframe());
        assertFalse(isGhostLoaderVisible());

        // Clicking FOCUS_REALBOX_AIM bubbles open-composebox without expanding.
        const newSearchButton =
            folded.shadowRoot.querySelector<HTMLElement>('[data-index="1"]')!;
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
            folded.shadowRoot.querySelector<HTMLElement>('[data-index="0"]')!;
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
      'suppresses stale hover on NTP_LHS_CLOSE after expanding via rail item',
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

        const folded = leftHandSide.$.folded;
        folded.dispatchEvent(new PointerEvent('pointerenter'));
        await microtasksFinished();
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
        await microtasksFinished();

        const expandButton =
            folded.shadowRoot.querySelector<HTMLElement>('[data-index="0"]')!;
        expandButton.click();
        await microtasksFinished();
        assertTrue(folded.inert);

        // Closing the expanded LHS restores #folded while suppressing stale
        // post-layout pointer hit-testing until the next real pointer move.
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
        await microtasksFinished();
        assertFalse(folded.inert);
        assertTrue(folded.hasAttribute('suppress-pointer-hover_'));
        assertEquals('none', getComputedStyle(expandButton).pointerEvents);

        // Once a real pointermove occurs, pointer hit-testing is restored.
        window.dispatchEvent(new PointerEvent('pointermove'));
        await microtasksFinished();
        assertFalse(folded.hasAttribute('suppress-pointer-hover_'));
        assertEquals('auto', getComputedStyle(expandButton).pointerEvents);
      });

  test(
      'shows ghost loader and expands width before iframe is ready',
      async () => {
        await createLeftHandSide();
        leftHandSide.style.height = '400px';

        const folded = leftHandSide.$.folded;
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
        // Only the close button is exposed.
        assertEquals('region', ghostLoader.getAttribute('role'));
        assertEquals('true', ghostLoader.getAttribute('aria-busy'));
        assertEquals(
            loadTimeData.getString('aimThreadsHistoryLabel'),
            ghostLoader.getAttribute('aria-label'));
        assertFalse(ghostLoader.hasAttribute('aria-hidden'));
        assertFalse(ghostLoader.inert);
        const ghostLoaderShadowRoot = getGhostLoaderShadowRoot();
        const ghostFooter =
            ghostLoaderShadowRoot.querySelector<HTMLElement>('.ghost-footer');
        assertTrue(!!ghostFooter);
        assertNear(
            leftHandSide.getBoundingClientRect().bottom,
            ghostFooter.getBoundingClientRect().bottom, LAYOUT_TOLERANCE_PX);
        const placeholders = ghostLoaderShadowRoot.querySelectorAll(
            '.ghost-logo, .ghost-actions, .ghost-threads, .ghost-footer');
        assertEquals(4, placeholders.length);
        for (const placeholder of placeholders) {
          assertEquals('true', placeholder.getAttribute('aria-hidden'));
        }
        const closeButton =
            ghostLoaderShadowRoot.querySelector<HTMLElement>('#closeButton');
        assertTrue(!!closeButton);
        assertFalse(!!closeButton.closest('[aria-hidden]'));
        assertEquals(
            loadTimeData.getString('aimThreadsCloseSidebarLabel'),
            closeButton.getAttribute('aria-label'));
        const closeTooltip =
            ghostLoaderShadowRoot.querySelector<CrTooltipElement>(
                '#closeTooltip');
        assertTrue(!!closeTooltip);
        assertEquals('closeButton', closeTooltip.for);
        assertEquals(TooltipPosition.RIGHT, closeTooltip.position);
        assertEquals(
            loadTimeData.getString('aimThreadsCloseSidebarLabel'),
            closeTooltip.textContent.trim());
        closeTooltip.show();
        assertNear(
            closeButton.getBoundingClientRect().right + 16,
            closeTooltip.$.tooltip.getBoundingClientRect().left,
            LAYOUT_TOLERANCE_PX);
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

  test('positions ghost loader close tooltip to the left in RTL', async () => {
    document.documentElement.dir = 'rtl';
    await createLeftHandSide();
    await expandLeftHandSide();

    const ghostLoader = getGhostLoader();
    assertTrue(!!ghostLoader);
    const closeButton =
        getGhostLoaderShadowRoot().querySelector<HTMLElement>('#closeButton');
    assertTrue(!!closeButton);
    const closeTooltip =
        getGhostLoaderShadowRoot().querySelector<CrTooltipElement>(
            '#closeTooltip');
    assertTrue(!!closeTooltip);
    assertEquals(TooltipPosition.LEFT, closeTooltip.position);

    closeTooltip.show();
    const closeButtonRect = closeButton.getBoundingClientRect();
    const tooltipRect = closeTooltip.$.tooltip.getBoundingClientRect();
    const ghostLoaderRect = ghostLoader.getBoundingClientRect();
    assertNear(
        closeButtonRect.left - 16, tooltipRect.right, LAYOUT_TOLERANCE_PX);
    assertTrue(tooltipRect.right < ghostLoaderRect.left);
  });

  test(
      'closes rail from ghost loader close button when iframe does not load',
      async () => {
        await createLeftHandSide();

        const folded = leftHandSide.$.folded;
        const collapsedWidth = `${folded.offsetWidth}px`;

        // The embedded page never sends NTP_LHS_READY, so the ghost loader
        // stays up until the user closes it.
        await expandLeftHandSide();
        const ghostLoader = getGhostLoader();
        assertTrue(!!ghostLoader);
        assertTrue(isGhostLoaderVisible());
        assertTrue(folded.inert);
        assertEquals('auto', getComputedStyle(ghostLoader).pointerEvents);
        const closeButton =
            getGhostLoaderShadowRoot().querySelector<HTMLElement>(
                '#closeButton');
        assertTrue(!!closeButton);

        closeButton.click();
        await microtasksFinished();
        assertFalse(!!getGhostLoader());
        assertFalse(folded.inert);
        assertEquals('visible', getComputedStyle(folded).visibility);
        assertEquals(collapsedWidth, getHostWidthProperty());
        const expanded = getExpandedIframe();
        assertTrue(!!expanded);
        assertTrue(expanded.hidden);

        // If the embedded page loads after the user closed the rail, the rail
        // stays folded.
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
        await microtasksFinished();
        assertFalse(expanded.hidden);
        assertTrue(expanded.inert);
        assertFalse(folded.inert);
        assertEquals(collapsedWidth, getHostWidthProperty());
      });

  test('toggles folded and expanded visibility on open and close', async () => {
    await createLeftHandSide();

    const folded = leftHandSide.$.folded;
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

        // :host may not create an absolute containing block, so cr-tooltip
        // inside #ghostLoader can overflow above #rhsContent.
        assertEquals('static', getComputedStyle(leftHandSide).position);

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

  const TEST_THEME_CSS_VARS: Record<string, string> = {
    '--color-new-tab-page-threads-rail-primary-font': 'rgba(31, 31, 31, 1)',
    '--color-new-tab-page-threads-rail-secondary-font': 'rgba(94, 94, 94, 1)',
    '--color-new-tab-page-threads-rail-background': 'rgba(241, 243, 244, 1)',
    '--color-new-tab-page-threads-rail-flyout-menu-background':
        'rgba(255, 255, 255, 1)',
    '--color-new-tab-page-threads-rail-surface-container':
        'rgba(255, 255, 255, 1)',
    '--color-new-tab-page-threads-rail-surface-container-high':
        'rgba(243, 246, 252, 1)',
    '--color-new-tab-page-threads-rail-surface-container-highest':
        'rgba(31, 31, 31, 0.06)',
    '--color-new-tab-page-threads-rail-primary': 'rgba(11, 87, 208, 1)',
    '--color-new-tab-page-threads-rail-tooltip-container':
        'rgba(255, 255, 255, 1)',
    '--color-new-tab-page-threads-rail-hover-background':
        'rgba(31, 31, 31, 0.06)',
    '--color-new-tab-page-threads-rail-selected-background':
        'rgba(31, 31, 31, 0.12)',
    '--color-new-tab-page-threads-rail-loader-base': 'rgba(235, 239, 247, 1)',
    '--color-new-tab-page-threads-rail-loader-highlight':
        'rgba(225, 227, 225, 1)',
    '--color-new-tab-page-threads-rail-brand-primary-font':
        'rgba(31, 31, 31, 1)',
    '--color-new-tab-page-threads-rail-brand-secondary-font':
        'rgba(94, 94, 94, 1)',
    '--color-new-tab-page-threads-rail-menu-open-background':
        'rgba(31, 31, 31, 0.08)',
    '--color-new-tab-page-threads-rail-container-background':
        'rgba(242, 242, 242, 1)',
    '--color-new-tab-page-threads-rail-divider': 'rgba(199, 199, 199, 1)',
    '--color-new-tab-page-threads-rail-scrim': 'rgba(0, 0, 0, 0.6)',
    '--color-new-tab-page-threads-rail-scrollbar-thumb': 'rgba(0, 0, 0, 0.62)',
    '--color-new-tab-page-threads-rail-update-dot': 'rgba(11, 87, 208, 1)',
  };

  const EXPECTED_THEME_PALETTE = {
    primaryFont: 'rgba(31, 31, 31, 1)',
    secondaryFont: 'rgba(94, 94, 94, 1)',
    railBackground: 'rgba(241, 243, 244, 1)',
    flyoutMenuBackground: 'rgba(255, 255, 255, 1)',
    surfaceContainer: 'rgba(255, 255, 255, 1)',
    surfaceContainerHigh: 'rgba(243, 246, 252, 1)',
    surfaceContainerHighest: 'rgba(31, 31, 31, 0.06)',
    primary: 'rgba(11, 87, 208, 1)',
    tooltipContainer: 'rgba(255, 255, 255, 1)',
    hoverBackgroundColor: 'rgba(31, 31, 31, 0.06)',
    selectedBackgroundColor: 'rgba(31, 31, 31, 0.12)',
    loaderBaseColor: 'rgba(235, 239, 247, 1)',
    loaderHighlightColor: 'rgba(225, 227, 225, 1)',
    brandPrimaryFont: 'rgba(31, 31, 31, 1)',
    brandSecondaryFont: 'rgba(94, 94, 94, 1)',
    menuOpenBackgroundColor: 'rgba(31, 31, 31, 0.08)',
    containerBackground: 'rgba(242, 242, 242, 1)',
    dividerColor: 'rgba(199, 199, 199, 1)',
    scrimColor: 'rgba(0, 0, 0, 0.6)',
    scrollbarThumbColor: 'rgba(0, 0, 0, 0.62)',
    updateDotColor: 'rgba(11, 87, 208, 1)',
  };

  test('posts NTP_LHS_SET_THEME message on theme change', async () => {
    await createLeftHandSide();
    await expandLeftHandSide();
    leftHandSide.isDark = true;
    for (const [prop, val] of Object.entries(TEST_THEME_CSS_VARS)) {
      leftHandSide.style.setProperty(prop, val);
    }

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
          theme: EXPECTED_THEME_PALETTE,
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
    for (const [prop, val] of Object.entries(TEST_THEME_CSS_VARS)) {
      leftHandSide.style.setProperty(prop, val);
    }

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
          theme: EXPECTED_THEME_PALETTE,
          isDark: false,
          iconSize: 20,
        },
        message);
    assertEquals(EXPANDED_LHS_ORIGIN, targetOrigin);
  });

  test('records expanded iframe loaded and shown metrics', async () => {
    await createLeftHandSide();
    const folded = leftHandSide.$.folded;

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
        const folded = leftHandSide.$.folded;

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

  test(
      'records IframeLoadLatency once from iframe stamp to ready', async () => {
        await createLeftHandSide();
        const folded = leftHandSide.$.folded;

        // Hover at t=100 stamps the expanded iframe (no debounce in tests).
        windowProxy.setResultFor('now', 100);
        folded.dispatchEvent(new Event('pointerenter'));
        await microtasksFinished();
        assertTrue(!!getExpandedIframe());

        // Ready at t=400.
        windowProxy.setResultFor('now', 400);
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
        await microtasksFinished();
        assertEquals(
            1, metrics.count('NewTabPage.ThreadsRail.IframeLoadLatency', 300));

        // A repeated NTP_LHS_READY does not record again.
        windowProxy.setResultFor('now', 900);
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
        await microtasksFinished();
        assertEquals(
            1, metrics.count('NewTabPage.ThreadsRail.IframeLoadLatency'));
      });

  test('records GhostLoaderShownDuration until iframe is ready', async () => {
    await createLeftHandSide();
    const folded = leftHandSide.$.folded;

    // Click without prior hover at t=100 shows the ghost loader.
    windowProxy.setResultFor('now', 100);
    folded.click();
    await microtasksFinished();
    assertTrue(isGhostLoaderVisible());
    assertEquals(
        0, metrics.count('NewTabPage.ThreadsRail.GhostLoaderShownDuration'));

    // Ready at t=600 replaces the ghost loader with the expanded iframe.
    windowProxy.setResultFor('now', 600);
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
    await microtasksFinished();
    assertFalse(isGhostLoaderVisible());
    assertEquals(
        1,
        metrics.count('NewTabPage.ThreadsRail.GhostLoaderShownDuration', 500));
    assertEquals(
        1, metrics.count('NewTabPage.ThreadsRail.IframeLoadLatency', 500));
  });

  test('records GhostLoaderShownDuration until collapsed', async () => {
    await createLeftHandSide();
    const folded = leftHandSide.$.folded;

    // Expand at t=100, collapse at t=300 before the iframe is ready.
    windowProxy.setResultFor('now', 100);
    folded.click();
    await microtasksFinished();
    windowProxy.setResultFor('now', 300);
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    await microtasksFinished();
    assertEquals(
        1,
        metrics.count('NewTabPage.ThreadsRail.GhostLoaderShownDuration', 200));

    // Expand again at t=1000; ready at t=1300 records a second sample.
    windowProxy.setResultFor('now', 1000);
    folded.click();
    await microtasksFinished();
    windowProxy.setResultFor('now', 1300);
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
    await microtasksFinished();
    assertEquals(
        1,
        metrics.count('NewTabPage.ThreadsRail.GhostLoaderShownDuration', 300));
    assertEquals(
        2, metrics.count('NewTabPage.ThreadsRail.GhostLoaderShownDuration'));
  });

  test(
      'does not record GhostLoaderShownDuration if iframe was ready',
      async () => {
        await createLeftHandSide();
        const folded = leftHandSide.$.folded;

        folded.dispatchEvent(new Event('pointerenter'));
        await microtasksFinished();
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
        await microtasksFinished();

        folded.click();
        await microtasksFinished();
        assertFalse(isGhostLoaderVisible());
        assertEquals(
            0,
            metrics.count('NewTabPage.ThreadsRail.GhostLoaderShownDuration'));
      });

  test(
      'records FirstExpandWasPreloaded false if iframe was not ready',
      async () => {
        await createLeftHandSide();
        const folded = leftHandSide.$.folded;

        // Hover stamps the expanded iframe, but it is not ready yet.
        folded.dispatchEvent(new Event('pointerenter'));
        await microtasksFinished();
        folded.click();
        await microtasksFinished();
        assertTrue(isGhostLoaderVisible());
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ThreadsRail.FirstExpandWasPreloaded', false));

        // Collapsing and expanding again before the iframe is ready does not
        // record again.
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
        await microtasksFinished();
        folded.click();
        await microtasksFinished();
        assertEquals(
            1, metrics.count('NewTabPage.ThreadsRail.FirstExpandWasPreloaded'));
      });

  test('records FirstExpandWasPreloaded true if iframe was ready', async () => {
    await createLeftHandSide();
    const folded = leftHandSide.$.folded;

    folded.dispatchEvent(new Event('pointerenter'));
    await microtasksFinished();
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
    await microtasksFinished();
    assertEquals(
        0, metrics.count('NewTabPage.ThreadsRail.FirstExpandWasPreloaded'));

    folded.click();
    await microtasksFinished();
    assertFalse(isGhostLoaderVisible());
    assertEquals(
        1,
        metrics.count('NewTabPage.ThreadsRail.FirstExpandWasPreloaded', true));

    // Closing and re-expanding does not record again.
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    await microtasksFinished();
    folded.click();
    await microtasksFinished();
    assertEquals(
        1, metrics.count('NewTabPage.ThreadsRail.FirstExpandWasPreloaded'));
  });

  test(
      'records longest completed hover in LongestFocusDurationBeforeExpand',
      async () => {
        await createLeftHandSide();
        const folded = leftHandSide.$.folded;

        // Hover for 300ms, leave, then hover for 150ms before expanding.
        windowProxy.setResultFor('now', 100);
        folded.dispatchEvent(new Event('pointerenter'));
        windowProxy.setResultFor('now', 400);
        folded.dispatchEvent(new Event('pointerleave'));
        windowProxy.setResultFor('now', 1000);
        folded.dispatchEvent(new Event('pointerenter'));
        windowProxy.setResultFor('now', 1150);
        folded.click();
        await microtasksFinished();

        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ThreadsRail.LongestFocusDurationBeforeExpand',
                300));

        // Subsequent expands do not record again.
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
        await microtasksFinished();
        windowProxy.setResultFor('now', 5000);
        folded.click();
        await microtasksFinished();
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ThreadsRail.LongestFocusDurationBeforeExpand'));
      });

  test('records active hover in LongestFocusDurationBeforeExpand', async () => {
    await createLeftHandSide();
    const folded = leftHandSide.$.folded;

    // Hover for 100ms, leave, then hover for 500ms until expanding.
    windowProxy.setResultFor('now', 100);
    folded.dispatchEvent(new Event('pointerenter'));
    windowProxy.setResultFor('now', 200);
    folded.dispatchEvent(new Event('pointerleave'));
    windowProxy.setResultFor('now', 1000);
    folded.dispatchEvent(new Event('pointerenter'));
    windowProxy.setResultFor('now', 1500);
    folded.click();
    await microtasksFinished();

    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.LongestFocusDurationBeforeExpand', 500));
  });

  test(
      'keeps hover/focus interval on focusout while still hovered',
      async () => {
        await createLeftHandSide();
        const folded = leftHandSide.$.folded;

        // Focus at t=100, then hover at t=200.
        windowProxy.setResultFor('now', 100);
        folded.dispatchEvent(new FocusEvent('focusin'));
        windowProxy.setResultFor('now', 200);
        folded.dispatchEvent(new Event('pointerenter'));
        await microtasksFinished();

        // Clicking the rail background blurs the focused child before click.
        windowProxy.setResultFor('now', 300);
        folded.dispatchEvent(new FocusEvent('focusout'));
        await microtasksFinished();

        windowProxy.setResultFor('now', 1000);
        folded.click();
        await microtasksFinished();

        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ThreadsRail.LongestFocusDurationBeforeExpand',
                900));
      });

  test(
      'keeps hover/focus interval on pointerleave while still focused',
      async () => {
        await createLeftHandSide();
        const folded = leftHandSide.$.folded;

        // Hover at t=100, then focus at t=200.
        windowProxy.setResultFor('now', 100);
        folded.dispatchEvent(new Event('pointerenter'));
        windowProxy.setResultFor('now', 200);
        folded.dispatchEvent(new FocusEvent('focusin'));
        await microtasksFinished();

        // Pointer leaves while the rail keeps focus.
        windowProxy.setResultFor('now', 300);
        folded.dispatchEvent(new Event('pointerleave'));
        await microtasksFinished();

        // Expand via keyboard at t=1000.
        windowProxy.setResultFor('now', 1000);
        folded.fire('expand-rail', new MouseEvent('click'));
        await microtasksFinished();

        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ThreadsRail.LongestFocusDurationBeforeExpand',
                900));
      });

  test(
      'does not record LongestFocusDurationBeforeExpand without hover or focus',
      async () => {
        await createLeftHandSide();
        const folded = leftHandSide.$.folded;

        windowProxy.setResultFor('now', 100);
        folded.click();
        await microtasksFinished();

        assertEquals(
            0,
            metrics.count(
                'NewTabPage.ThreadsRail.LongestFocusDurationBeforeExpand'));
      });

  test('ends hover/focus interval when document becomes hidden', async () => {
    await createLeftHandSide();
    const folded = leftHandSide.$.folded;

    // Hover at t=100, hide the page at t=400 and show it again at t=2000.
    windowProxy.setResultFor('now', 100);
    folded.dispatchEvent(new Event('pointerenter'));
    windowProxy.setResultFor('now', 400);
    setDocumentHidden(true);
    windowProxy.setResultFor('now', 2000);
    setDocumentHidden(false);

    // Expanding at t=2100 only counts the hover until the page was hidden.
    windowProxy.setResultFor('now', 2100);
    folded.click();
    await microtasksFinished();

    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.LongestFocusDurationBeforeExpand', 300));
  });

  test(
      'cancels pending preload timer when document becomes hidden',
      async () => {
        loadTimeData.overrideValues({
          threadsRailPreloadDebounceMs: 100,
        });
        windowProxy.setResultFor('setTimeout', 42);
        await createLeftHandSide();

        const folded = leftHandSide.$.folded;

        folded.dispatchEvent(new Event('pointerenter'));
        await microtasksFinished();
        assertEquals(1, windowProxy.getCallCount('setTimeout'));

        setDocumentHidden(true);
        setDocumentHidden(false);
        assertEquals(1, windowProxy.getCallCount('clearTimeout'));
        assertEquals(42, windowProxy.getArgs('clearTimeout')[0]);
      });

  test(
      'resets hover state when document becomes hidden without pointerleave',
      async () => {
        await createLeftHandSide();
        const folded = leftHandSide.$.folded;

        // Hover at t=100 and hide the page at t=200 without a pointerleave.
        windowProxy.setResultFor('now', 100);
        folded.dispatchEvent(new Event('pointerenter'));
        windowProxy.setResultFor('now', 200);
        setDocumentHidden(true);
        windowProxy.setResultFor('now', 1000);
        setDocumentHidden(false);

        // Focus the rail at t=1000 and blur it at t=1500. The interval ends on
        // focusout, since the stale hover state was reset.
        folded.dispatchEvent(new FocusEvent('focusin'));
        windowProxy.setResultFor('now', 1500);
        folded.dispatchEvent(new FocusEvent('focusout'));
        await microtasksFinished();

        // Expanding at t=5000 only counts the 500ms focus interval.
        windowProxy.setResultFor('now', 5000);
        folded.click();
        await microtasksFinished();

        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ThreadsRail.LongestFocusDurationBeforeExpand',
                500));
      });

  test('records LongestFocusDurationWithoutExpand on pagehide', async () => {
    await createLeftHandSide();
    const folded = leftHandSide.$.folded;

    // Hover for 50ms, then for 600ms, without ever expanding.
    windowProxy.setResultFor('now', 100);
    folded.dispatchEvent(new Event('pointerenter'));
    windowProxy.setResultFor('now', 150);
    folded.dispatchEvent(new Event('pointerleave'));
    windowProxy.setResultFor('now', 1000);
    folded.dispatchEvent(new Event('pointerenter'));
    windowProxy.setResultFor('now', 1600);
    folded.dispatchEvent(new Event('pointerleave'));
    await microtasksFinished();

    windowProxy.setResultFor('now', 5000);
    window.dispatchEvent(new Event('pagehide'));

    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.LongestFocusDurationWithoutExpand', 600));
    assertEquals(
        0,
        metrics.count(
            'NewTabPage.ThreadsRail.LongestFocusDurationBeforeExpand'));
  });

  test(
      'records active hover in LongestFocusDurationWithoutExpand on pagehide',
      async () => {
        await createLeftHandSide();
        const folded = leftHandSide.$.folded;

        // Still hovered when the page is hidden and then unloaded.
        windowProxy.setResultFor('now', 100);
        folded.dispatchEvent(new Event('pointerenter'));
        await microtasksFinished();
        windowProxy.setResultFor('now', 800);
        setDocumentHidden(true);
        window.dispatchEvent(new Event('pagehide'));
        setDocumentHidden(false);

        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ThreadsRail.LongestFocusDurationWithoutExpand',
                700));
      });

  test(
      'skips LongestFocusDurationWithoutExpand if expanded or never hovered',
      async () => {
        await createLeftHandSide();
        const folded = leftHandSide.$.folded;

        // Never hovered or focused.
        window.dispatchEvent(new Event('pagehide'));
        assertEquals(
            0,
            metrics.count(
                'NewTabPage.ThreadsRail.LongestFocusDurationWithoutExpand'));

        // Hovered, then expanded.
        windowProxy.setResultFor('now', 100);
        folded.dispatchEvent(new Event('pointerenter'));
        windowProxy.setResultFor('now', 400);
        folded.click();
        await microtasksFinished();
        window.dispatchEvent(new Event('pagehide'));
        assertEquals(
            0,
            metrics.count(
                'NewTabPage.ThreadsRail.LongestFocusDurationWithoutExpand'));
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ThreadsRail.LongestFocusDurationBeforeExpand',
                300));
      });

  test(
      'does not record latency metrics if document was hidden during interval',
      async () => {
        await createLeftHandSide();
        const folded = leftHandSide.$.folded;

        // Expand at t=50, which stamps the iframe and shows the ghost loader.
        windowProxy.setResultFor('now', 50);
        folded.click();
        await microtasksFinished();
        assertTrue(isGhostLoaderVisible());

        // Document becomes hidden at t=70 and visible again at t=80.
        windowProxy.setResultFor('now', 70);
        setDocumentHidden(true);
        windowProxy.setResultFor('now', 80);
        setDocumentHidden(false);

        // Ready arrives at t=400. Both intervals started at 50 <=
        // lastHiddenTime_ (70), so both samples are skipped.
        windowProxy.setResultFor('now', 400);
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
        await microtasksFinished();
        assertFalse(isGhostLoaderVisible());

        assertEquals(
            0, metrics.count('NewTabPage.ThreadsRail.IframeLoadLatency'));
        assertEquals(
            0,
            metrics.count('NewTabPage.ThreadsRail.GhostLoaderShownDuration'));
      });

  test(
      'focuses labeled iframe on preloaded open and restores focus on close',
      async () => {
        loadTimeData.overrideValues({lhsRailConfig: EXPAND_RAIL_CONFIG});
        await createLeftHandSide();

        const folded = leftHandSide.$.folded;
        const expandButton =
            folded.shadowRoot.querySelector<HTMLElement>('[data-index="0"]')!;

        // Silent hover preload and NTP_LHS_READY while collapsed must not steal
        // focus.
        folded.dispatchEvent(new PointerEvent('pointerenter'));
        await microtasksFinished();
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 261});
        await microtasksFinished();
        assertEquals(null, leftHandSide.shadowRoot.activeElement);

        // Opening after preload moves focus into the expanded iframe container
        // and exposes the history label via aria-label.
        expandButton.focus();
        expandButton.click();
        await microtasksFinished();

        const expanded = getExpandedIframe()!;
        const iframe =
            expanded.shadowRoot.querySelector<HTMLIFrameElement>('#iframe')!;
        assertEquals(expanded, leftHandSide.shadowRoot.activeElement);
        assertEquals(iframe, expanded.shadowRoot.activeElement);
        const calls = getFocusMessages();
        assertEquals(1, calls.length);
        const [, message] = calls[0];
        assertDeepEquals({type: NtpLhsMessageType.NTP_LHS_FOCUS}, message);
        assertEquals(
            loadTimeData.getString('aimThreadsHistoryLabel'),
            iframe.getAttribute('aria-label'));

        // Closing via NTP_LHS_CLOSE returns focus to the Open sidebar button.
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
        await microtasksFinished();
        assertEquals(folded, leftHandSide.shadowRoot.activeElement);
        assertEquals(expandButton, folded.shadowRoot.activeElement);
      });

  test(
      'focuses ghost loader container on mouse open and iframe on ready',
      async () => {
        loadTimeData.overrideValues({lhsRailConfig: EXPAND_RAIL_CONFIG});
        await createLeftHandSide();

        const folded = leftHandSide.$.folded;
        const expandButton =
            folded.shadowRoot.querySelector<HTMLElement>('[data-index="0"]')!;
        expandButton.focus();
        expandButton.dispatchEvent(new PointerEvent('click', {
          bubbles: true,
          composed: true,
          detail: 1,
          sourceCapabilities:
              new InputDeviceCapabilities({firesTouchEvents: false}),
        }));
        await microtasksFinished();

        // Focus lands on the ghost loader container itself rather than its
        // close button so the close tooltip does not pop up immediately.
        const ghostLoader = getGhostLoader()!;
        assertEquals('-1', ghostLoader.getAttribute('tabindex'));
        assertEquals('none', getComputedStyle(ghostLoader).outlineStyle);
        assertEquals(ghostLoader, leftHandSide.shadowRoot.activeElement);
        assertEquals(0, getFocusMessages().length);

        // Once NTP_LHS_READY arrives, focus transfers from the ghost loader to
        // the expanded iframe.
        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 261});
        await microtasksFinished();

        const expanded = getExpandedIframe()!;
        const iframe =
            expanded.shadowRoot.querySelector<HTMLIFrameElement>('#iframe')!;
        assertEquals(expanded, leftHandSide.shadowRoot.activeElement);
        assertEquals(iframe, expanded.shadowRoot.activeElement);
        assertEquals(0, getFocusMessages().length);
      });

  [{input: 'mouse', firesTouchEvents: false, expectedCount: 1},
   {input: 'accessibility', firesTouchEvents: null, expectedCount: 2},
  ].forEach(({input, firesTouchEvents, expectedCount}) => {
    test(`focus messages count after ${input} reopen`, async () => {
      loadTimeData.overrideValues({lhsRailConfig: EXPAND_RAIL_CONFIG});
      await createLeftHandSide();
      const folded = leftHandSide.$.folded;
      const expandButton =
          folded.shadowRoot.querySelector<HTMLElement>('[data-index="0"]')!;
      expandButton.click();
      await microtasksFinished();
      postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
      await microtasksFinished();
      assertEquals(1, getFocusMessages().length);
      postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
      await microtasksFinished();

      expandButton.dispatchEvent(new PointerEvent('click', {
        bubbles: true,
        composed: true,
        detail: 1,
        sourceCapabilities: firesTouchEvents === null ?
            null :
            new InputDeviceCapabilities({firesTouchEvents}),
      }));
      await microtasksFinished();
      assertFalse(getExpandedIframe()!.inert);
      assertEquals(expectedCount, getFocusMessages().length);
    });
  });

  test(
      'does not steal external focus on NTP_LHS_READY or NTP_LHS_CLOSE',
      async () => {
        loadTimeData.overrideValues({lhsRailConfig: EXPAND_RAIL_CONFIG});
        await createLeftHandSide();
        const externalButton = document.createElement('button');
        document.body.appendChild(externalButton);

        const folded = leftHandSide.$.folded;
        const expandButton =
            folded.shadowRoot.querySelector<HTMLElement>('[data-index="0"]')!;
        expandButton.click();
        await microtasksFinished();
        assertEquals(leftHandSide, document.activeElement);

        // User moves focus outside the rail while the iframe is still loading.
        externalButton.focus();
        assertEquals(externalButton, document.activeElement);
        assertEquals(null, leftHandSide.shadowRoot.activeElement);

        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 261});
        await microtasksFinished();
        assertEquals(externalButton, document.activeElement);
        assertEquals(null, leftHandSide.shadowRoot.activeElement);
        assertEquals(0, getFocusMessages().length);

        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
        await microtasksFinished();
        assertEquals(externalButton, document.activeElement);
        assertEquals(null, leftHandSide.shadowRoot.activeElement);
      });

  test(
      'can close the loading sidebar with Enter immediately after keyboard open',
      async () => {
        loadTimeData.overrideValues({lhsRailConfig: EXPAND_RAIL_CONFIG});
        await createLeftHandSide();

        const folded = leftHandSide.$.folded;
        const expandButton =
            folded.shadowRoot.querySelector<HTMLElement>('[data-index="0"]')!;

        expandButton.focus();
        keydown(expandButton, 'Enter');
        await microtasksFinished();

        const closeButton =
            getGhostLoaderShadowRoot().querySelector<HTMLElement>(
                '#closeButton')!;
        assertEquals(closeButton, getGhostLoaderShadowRoot().activeElement);
        keydown(closeButton, 'Enter');
        await microtasksFinished();
        assertEquals(folded, leftHandSide.shadowRoot.activeElement);
        assertEquals(expandButton, folded.shadowRoot.activeElement);
      });
});
