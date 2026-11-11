// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <cr-left-hand-side> renders an <ntp-iframe>, which is defined in the lazily
// loaded part of the NTP.
import 'chrome://new-tab-page/lazy_load.js';

import type {IframeElement, ThreadsRailElement} from 'chrome://new-tab-page/lazy_load.js';
import {LhsClientAction} from 'chrome://new-tab-page/lazy_load.js';
import type {LeftHandSideElement} from 'chrome://new-tab-page/new_tab_page.js';
import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater, NtpLhsMessageType, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNear, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock, LAYOUT_TOLERANCE_PX} from './test_support.js';

const EXPANDED_LHS_ORIGIN = 'https://www.google.com';
const EXPANDED_LHS_URL = `${EXPANDED_LHS_ORIGIN}/search?nem=341`;

suite('NewTabPageLeftHandSideTest', () => {
  let leftHandSide: LeftHandSideElement;
  let windowProxy: TestMock<WindowProxy>;

  function getExpandedIframe(): IframeElement|null {
    return leftHandSide.shadowRoot.querySelector<IframeElement>('#expanded');
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
      enableThreadsRailHybrid: true,
      expandedLhsUrl: EXPANDED_LHS_URL,
      lhsRailConfig: {items: []},
      threadsRailPreloadDebounceMs: 0,
    });
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

  test('renders folded rail when hybrid is enabled', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: true});
    await createLeftHandSide();

    const folded =
        leftHandSide.shadowRoot.querySelector<ThreadsRailElement>('#folded');
    assertTrue(!!folded);
    assertEquals('CR-THREADS-RAIL', folded.tagName);
  });

  test('does not render folded rail when hybrid is disabled', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: false});
    await createLeftHandSide();

    assertFalse(!!leftHandSide.shadowRoot.querySelector('#folded'));
  });

  [true, false].forEach(enableThreadsRailHybrid => {
    const mode = enableThreadsRailHybrid ? 'hybrid' : 'non-hybrid';

    test(`renders expanded LHS iframe in ${mode} mode`, async () => {
      loadTimeData.overrideValues({enableThreadsRailHybrid});
      await createLeftHandSide();

      if (enableThreadsRailHybrid) {
        assertFalse(!!getExpandedIframe());
        assertEquals(0, windowProxy.getCallCount('ensureLazyLoaded'));
        await expandLeftHandSide();
        assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
      }

      const iframe = getExpandedIframe();
      assertTrue(!!iframe);
      assertEquals('NTP-IFRAME', iframe.tagName);
      assertEquals(EXPANDED_LHS_URL, iframe.src);
      assertEquals('clipboard-write; unbounded-element', iframe.allow);
      assertEquals(enableThreadsRailHybrid ? '261px' : '', iframe.style.width);
      // In hybrid mode the host starts at the folded rail's width. In
      // full-iframe mode no width is known until the iframe reports one.
      const folded =
          leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded');
      if (enableThreadsRailHybrid) {
        assertTrue(!!folded);
        assertTrue(folded.offsetWidth > 0 && folded.offsetWidth < 261);
      }
      assertEquals(
          enableThreadsRailHybrid ? `${folded!.offsetWidth}px` : '',
          getHostWidthProperty());
      assertTrue(iframe.hidden);
      assertFalse(iframe.classList.contains('ready'));
    });
  });

  test('updates ready state on NTP_LHS_READY in non-hybrid mode', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: false});
    await createLeftHandSide();

    postLhsMessage({
      type: NtpLhsMessageType.NTP_LHS_READY,
    });
    await microtasksFinished();

    const iframe = getExpandedIframe();
    assertTrue(!!iframe);
    assertEquals('', iframe.style.width);
    assertFalse(iframe.hidden);
    assertTrue(iframe.classList.contains('ready'));
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

  test('handles NTP_LHS_CLOSE message in hybrid mode', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: true});
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
    assertFalse(folded.hidden);
    assertTrue(folded.inert);
    assertEquals('320px', getHostWidthProperty());

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    await microtasksFinished();
    assertEquals(expanded, getExpandedIframe());
    assertFalse(expanded.hidden);
    assertTrue(expanded.inert);
    assertFalse(folded.hidden);
    assertFalse(folded.inert);
    assertEquals(collapsedWidth, getHostWidthProperty());
  });

  test('expands LHS when folded rail is clicked in hybrid mode', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: true});
    await createLeftHandSide();

    assertFalse(!!getExpandedIframe());
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
    const collapsedWidth = `${folded.offsetWidth}px`;

    await expandLeftHandSide();
    assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
    const initialExpanded = getExpandedIframe();
    assertTrue(!!initialExpanded);
    assertTrue(initialExpanded.hidden);
    assertFalse(folded.hidden);
    assertFalse(folded.inert);
    assertEquals(collapsedWidth, getHostWidthProperty());

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
    await microtasksFinished();
    assertFalse(initialExpanded.hidden);
    assertFalse(initialExpanded.inert);
    assertFalse(folded.hidden);
    assertTrue(folded.inert);
    assertEquals('320px', getHostWidthProperty());

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    await microtasksFinished();
    assertEquals(initialExpanded, getExpandedIframe());
    assertFalse(initialExpanded.hidden);
    assertTrue(initialExpanded.inert);
    assertFalse(folded.hidden);
    assertFalse(folded.inert);
    assertEquals(collapsedWidth, getHostWidthProperty());

    await expandLeftHandSide();
    assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
    assertEquals(initialExpanded, getExpandedIframe());
    assertFalse(initialExpanded.hidden);
    assertFalse(initialExpanded.inert);
    assertFalse(folded.hidden);
    assertTrue(folded.inert);
    assertEquals('320px', getHostWidthProperty());
  });

  test('expands LHS on expand-rail event in hybrid mode', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: true});
    await createLeftHandSide();

    assertFalse(!!getExpandedIframe());
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;

    folded.dispatchEvent(
        new CustomEvent('expand-rail', {bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
    const expanded = getExpandedIframe();
    assertTrue(!!expanded);
    assertTrue(expanded.hidden);
    assertFalse(folded.hidden);
    assertFalse(folded.inert);

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
    await microtasksFinished();
    assertFalse(expanded.hidden);
    assertFalse(folded.hidden);
    assertTrue(folded.inert);
  });

  ['pointerenter', 'focusin'].forEach(eventName => {
    test(
        `preloads expanded LHS iframe on ${eventName} in hybrid mode`,
        async () => {
          loadTimeData.overrideValues({enableThreadsRailHybrid: true});
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
          assertFalse(folded.hidden);
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
          assertFalse(folded.hidden);
          assertFalse(folded.inert);
          assertEquals(collapsedWidth, getHostWidthProperty());

          // Clicking the folded rail expands immediately since the preloaded
          // iframe is already ready.
          folded.click();
          await microtasksFinished();
          assertFalse(preloaded.hidden);
          assertFalse(preloaded.inert);
          assertFalse(folded.hidden);
          assertTrue(folded.inert);
          assertEquals('320px', getHostWidthProperty());
        });
  });

  ['pointerenter', 'focusin'].forEach(eventName => {
    test(
        `debounces preloading on ${eventName} when debounce delay is set`,
        async () => {
          loadTimeData.overrideValues({
            enableThreadsRailHybrid: true,
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
    test(
        `cancels preload timer on ${eventName}`,
        async () => {
          loadTimeData.overrideValues({
            enableThreadsRailHybrid: true,
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
          enableThreadsRailHybrid: true,
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
          enableThreadsRailHybrid: true,
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

  test(
      'cancels pending preload timer when disconnected',
      async () => {
        loadTimeData.overrideValues({
          enableThreadsRailHybrid: true,
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
          enableThreadsRailHybrid: true,
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
        assertFalse(folded.hidden);
        assertFalse(folded.inert);

        // Clicking EXPAND_RAIL expands the LHS iframe.
        const expandButton =
            folded.shadowRoot.querySelector<HTMLElement>('#railItem-0')!;
        expandButton.click();
        await microtasksFinished();
        assertEquals(1, windowProxy.getCallCount('ensureLazyLoaded'));
        const expanded = getExpandedIframe();
        assertTrue(!!expanded);
        assertTrue(expanded.hidden);
        assertFalse(folded.hidden);
        assertFalse(folded.inert);

        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
        await microtasksFinished();
        assertFalse(expanded.hidden);
        assertFalse(folded.hidden);
        assertTrue(folded.inert);
        assertEquals('320px', getHostWidthProperty());
      });

  test(
      'keeps folded width until expanded iframe is ready in hybrid mode',
      async () => {
        loadTimeData.overrideValues({enableThreadsRailHybrid: true});
        await createLeftHandSide();

        const folded =
            leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
        assertTrue(folded.offsetWidth > 0 && folded.offsetWidth < 261);
        const collapsedWidth = `${folded.offsetWidth}px`;

        // Clicking the folded rail before the embedded page is ready keeps
        // showing the folded rail, so the host must keep its width too.
        await expandLeftHandSide();
        const expanded = getExpandedIframe();
        assertTrue(!!expanded);
        assertTrue(expanded.hidden);
        assertFalse(folded.hidden);
        assertFalse(folded.inert);
        assertEquals(collapsedWidth, getHostWidthProperty());

        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
        await microtasksFinished();
        assertFalse(expanded.hidden);
        assertFalse(folded.hidden);
        assertTrue(folded.inert);
        assertEquals('320px', getHostWidthProperty());
      });

  test(
      'toggles folded and expanded visibility on open and close in hybrid mode',
      async () => {
        loadTimeData.overrideValues({enableThreadsRailHybrid: true});
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

        // Removing --cr-animations-disabled enables transitions via
        // @container not style(--cr-animations-disabled: 1).
        document.documentElement.style.removeProperty(
            '--cr-animations-disabled');
        assertEquals('0.3s', getComputedStyle(leftHandSide).transitionDuration);
        assertEquals('0.15s, 0s', getComputedStyle(folded).transitionDuration);
        assertEquals(
            '0.24s, 0s', getComputedStyle(expanded).transitionDuration);
        document.documentElement.style.setProperty(
            '--cr-animations-disabled', '1');

        assertFalse(folded.hidden);
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
        loadTimeData.overrideValues({enableThreadsRailHybrid: true});
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

  test(
      'does not post NTP_LHS_SET_THEME while collapsed in hybrid mode',
      async () => {
        await createLeftHandSide();
        ColorChangeUpdater.forDocument().eventTarget.dispatchEvent(
            new CustomEvent(COLOR_PROVIDER_CHANGED));
        assertEquals(0, windowProxy.getCallCount('postMessage'));
      });

  // Resizes the inner <iframe> to simulate the embedded page reporting a new
  // width through frame-sizing, then waits for the ResizeObserver to report
  // and for the resulting re-render.
  async function setIframeWidth(width: number) {
    await setIframeSize({width});
  }

  async function setIframeSize(
      {width, height}: {width?: number, height?: number}) {
    const expanded = getExpandedIframe();
    assertTrue(!!expanded);
    const iframe = expanded.shadowRoot.querySelector('iframe');
    assertTrue(!!iframe);
    if (width !== undefined) {
      iframe.style.width = `${width}px`;
    }
    if (height !== undefined) {
      iframe.style.height = `${height}px`;
    }
    for (let i = 0; i < 2; i++) {
      await new Promise(resolve => requestAnimationFrame(resolve));
    }
    await microtasksFinished();
  }

  function getHostWidthProperty(): string {
    return leftHandSide.style.getPropertyValue('--cr-left-hand-side-width');
  }

  test('mirrors iframe width onto host in non-hybrid mode', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: false});
    await createLeftHandSide();

    // The iframe is 0px wide until ready; the host width is left alone.
    await setIframeWidth(0);
    assertEquals('', getHostWidthProperty());

    // NTP_LHS_READY does not infer an expanded state in non-hybrid mode: the
    // embedded page may be ready in its folded state.
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
    await microtasksFinished();
    assertFalse(leftHandSide.hasAttribute('is-expanded_'));

    // The first reported width is mirrored without changing the expanded
    // state.
    await setIframeWidth(320);
    assertEquals('320px', getHostWidthProperty());
    assertFalse(leftHandSide.hasAttribute('is-expanded_'));

    // Growing is treated as expanding, shrinking as collapsing.
    await setIframeWidth(400);
    assertEquals('400px', getHostWidthProperty());
    assertTrue(leftHandSide.hasAttribute('is-expanded_'));

    // A height-only resize (e.g. the browser window changing size) does not
    // change the width and must not flip the expanded state.
    await setIframeSize({height: 500});
    assertEquals('400px', getHostWidthProperty());
    assertTrue(leftHandSide.hasAttribute('is-expanded_'));

    await setIframeWidth(64);
    assertEquals('64px', getHostWidthProperty());
    assertFalse(leftHandSide.hasAttribute('is-expanded_'));
  });

  test('does not mirror iframe width onto host in hybrid mode', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: true});
    await createLeftHandSide();
    await expandLeftHandSide();

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
    folded.click();
    await microtasksFinished();
    assertEquals('261px', getHostWidthProperty());

    // The host keeps the reported expanded width; the iframe's own size is
    // not observed in hybrid mode.
    await setIframeWidth(320);
    assertEquals('261px', getHostWidthProperty());
  });

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
        },
        message);
    assertEquals(EXPANDED_LHS_ORIGIN, targetOrigin);
  });
});
