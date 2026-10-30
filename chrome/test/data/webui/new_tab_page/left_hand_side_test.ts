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
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

const EXPANDED_LHS_ORIGIN = 'https://www.google.com';
const EXPANDED_LHS_URL = `${EXPANDED_LHS_ORIGIN}/search?nem=341`;

suite('NewTabPageLeftHandSideTest', () => {
  let leftHandSide: LeftHandSideElement;
  let windowProxy: TestMock<WindowProxy>;

  function getExpandedIframeWindow(): Window {
    const iframe = leftHandSide.$.expanded.shadowRoot.querySelector('iframe');
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
    loadTimeData.overrideValues({
      enableThreadsRailHybrid: true,
      expandedLhsUrl: EXPANDED_LHS_URL,
      lhsRailConfig: {items: []},
    });
    windowProxy = installMock(WindowProxy);
    // Prevents the iframe from actually loading the LHS page.
    windowProxy.setResultFor('createIframeSrc', '');
  });

  async function createLeftHandSide() {
    leftHandSide = document.createElement('cr-left-hand-side');
    document.body.appendChild(leftHandSide);
    await microtasksFinished();
  }

  function getExpandedIframe(): IframeElement {
    return leftHandSide.$.expanded;
  }

  test('updates color-scheme on isDark changes', async () => {
    await createLeftHandSide();

    const iframe = getExpandedIframe();
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

      const iframe = getExpandedIframe();
      assertTrue(!!iframe);
      assertEquals('NTP-IFRAME', iframe.tagName);
      assertEquals(EXPANDED_LHS_URL, iframe.src);
      assertEquals('unbounded-element', iframe.allow);
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
      assertEquals(enableThreadsRailHybrid, iframe.hidden);
      assertTrue(iframe.inert);
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
    assertFalse(iframe.inert);
    assertTrue(iframe.classList.contains('ready'));
  });

  test(
      'keeps rail folded while updating expanded width on NTP_LHS_READY in' +
          ' hybrid mode',
      async () => {
        await createLeftHandSide();

        postLhsMessage({
          type: NtpLhsMessageType.NTP_LHS_READY,
          width: 320,
        });
        await microtasksFinished();

        const folded =
            leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
        const iframe = getExpandedIframe();
        assertTrue(!!iframe);
        assertEquals('320px', iframe.style.width);
        assertEquals(`${folded.offsetWidth}px`, getHostWidthProperty());
        assertFalse(folded.hidden);
        assertTrue(iframe.hidden);
        assertFalse(iframe.inert);
        assertTrue(iframe.classList.contains('ready'));
      });

  test(
      'expands once NTP_LHS_READY arrives if clicked before ready',
      async () => {
        await createLeftHandSide();

        const folded =
            leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
        const iframe = getExpandedIframe();

        folded.click();
        await microtasksFinished();
        assertFalse(folded.hidden);
        assertTrue(iframe.hidden);

        postLhsMessage({
          type: NtpLhsMessageType.NTP_LHS_READY,
          width: 320,
        });
        await microtasksFinished();
        assertTrue(folded.hidden);
        assertFalse(iframe.hidden);
      });

  test('falls back to default width on invalid or missing width', async () => {
    await createLeftHandSide();
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
    folded.click();
    await microtasksFinished();

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

        postLhsMessage({
          type: NtpLhsMessageType.NTP_LHS_READY,
          width: 320,
        });
        await microtasksFinished();

        const iframe = getExpandedIframe();
        assertTrue(!!iframe);
        assertEquals('261px', iframe.style.width);
      });

  test('handles NTP_LHS_CLOSE message in hybrid mode', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: true});
    await createLeftHandSide();

    const expanded = leftHandSide.$.expanded;
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
    assertTrue(folded.offsetWidth > 0 && folded.offsetWidth < 261);
    const collapsedWidth = `${folded.offsetWidth}px`;
    assertEquals(collapsedWidth, getHostWidthProperty());

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
    folded.click();
    await microtasksFinished();
    assertFalse(expanded.hidden);
    assertTrue(folded.hidden);
    assertEquals('320px', getHostWidthProperty());

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    await microtasksFinished();
    assertTrue(expanded.hidden);
    assertFalse(folded.hidden);
    assertEquals(collapsedWidth, getHostWidthProperty());
  });

  test('expands LHS when folded rail is clicked in hybrid mode', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: true});
    await createLeftHandSide();

    const expanded = leftHandSide.$.expanded;
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
    await microtasksFinished();
    assertTrue(expanded.hidden);
    assertFalse(folded.hidden);
    assertEquals(`${folded.offsetWidth}px`, getHostWidthProperty());

    folded.click();
    await microtasksFinished();
    assertFalse(expanded.hidden);
    assertTrue(folded.hidden);
  });

  test('expands LHS on expand-rail event in hybrid mode', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: true});
    await createLeftHandSide();

    const expanded = leftHandSide.$.expanded;
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
    await microtasksFinished();
    assertTrue(expanded.hidden);
    assertFalse(folded.hidden);

    folded.dispatchEvent(
        new CustomEvent('expand-rail', {bubbles: true, composed: true}));
    await microtasksFinished();
    assertFalse(expanded.hidden);
    assertTrue(folded.hidden);
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

        const expanded = leftHandSide.$.expanded;
        const folded =
            leftHandSide.shadowRoot.querySelector<ThreadsRailElement>(
                '#folded')!;

        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
        await microtasksFinished();
        assertTrue(expanded.hidden);
        assertFalse(folded.hidden);

        // Clicking FOCUS_REALBOX_AIM bubbles open-composebox without expanding.
        const newSearchButton =
            folded.shadowRoot.querySelector<HTMLElement>('#railItem-1')!;
        const whenOpenComposebox =
            eventToPromise('open-composebox', leftHandSide);
        newSearchButton.click();
        await whenOpenComposebox;
        await microtasksFinished();
        assertTrue(expanded.hidden);
        assertFalse(folded.hidden);

        // Clicking EXPAND_RAIL expands the LHS iframe.
        const expandButton =
            folded.shadowRoot.querySelector<HTMLElement>('#railItem-0')!;
        expandButton.click();
        await microtasksFinished();
        assertFalse(expanded.hidden);
        assertTrue(folded.hidden);
        assertEquals('320px', getHostWidthProperty());
      });

  test(
      'keeps folded width until expanded iframe is ready in hybrid mode',
      async () => {
        loadTimeData.overrideValues({enableThreadsRailHybrid: true});
        await createLeftHandSide();

        const expanded = leftHandSide.$.expanded;
        const folded =
            leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;
        assertTrue(folded.offsetWidth > 0 && folded.offsetWidth < 261);
        const collapsedWidth = `${folded.offsetWidth}px`;

        // Clicking the folded rail before the embedded page is ready keeps
        // showing the folded rail, so the host must keep its width too.
        folded.click();
        await microtasksFinished();
        assertTrue(expanded.hidden);
        assertFalse(folded.hidden);
        assertEquals(collapsedWidth, getHostWidthProperty());

        postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY, width: 320});
        await microtasksFinished();
        assertFalse(expanded.hidden);
        assertTrue(folded.hidden);
        assertEquals('320px', getHostWidthProperty());
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

    const whenOpenComposebox = eventToPromise('open-composebox', leftHandSide);
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_NEW_THREAD});
    await whenOpenComposebox;
  });

  test('handles NTP_LHS_NAVIGATE message', async () => {
    await createLeftHandSide();

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
    assertEquals(
        getExpandedIframe().shadowRoot.querySelector('#iframe'), iframe);
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
    assertEquals(
        getExpandedIframe().shadowRoot.querySelector('#iframe'), iframe);
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
