// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <cr-left-hand-side> renders an <ntp-iframe>, which is defined in the lazily
// loaded part of the NTP.
import 'chrome://new-tab-page/lazy_load.js';

import type {IframeElement, ThreadsRailElement} from 'chrome://new-tab-page/lazy_load.js';
import type {LeftHandSideElement} from 'chrome://new-tab-page/new_tab_page.js';
import {NtpLhsMessageType, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

const EXPANDED_LHS_ORIGIN = 'https://www.google.com';
const EXPANDED_LHS_URL = `${EXPANDED_LHS_ORIGIN}/search?nem=341`;

suite('NewTabPageLeftHandSideTest', () => {
  let leftHandSide: LeftHandSideElement;
  let windowProxy: TestMock<WindowProxy>;

  function getExpandedIframeWindow(): Window {
    const expanded =
        leftHandSide.shadowRoot.querySelector<IframeElement>('#expanded');
    assertTrue(!!expanded);
    const iframe = expanded.shadowRoot.querySelector('iframe');
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

  function getExpandedIframe(): IframeElement|null {
    return leftHandSide.shadowRoot.querySelector<IframeElement>('#expanded');
  }

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
      assertEquals(enableThreadsRailHybrid, iframe.hidden);
      assertTrue(iframe.inert);
      assertFalse(iframe.classList.contains('ready'));
    });
  });

  test('updates expanded LHS iframe URL when isDark changes', async () => {
    await createLeftHandSide();

    const iframe = getExpandedIframe();
    assertTrue(!!iframe);
    assertEquals(EXPANDED_LHS_URL, iframe.src);

    leftHandSide.isDark = true;
    await microtasksFinished();
    assertEquals(`${EXPANDED_LHS_URL}&cs=1`, iframe.src);

    leftHandSide.isDark = false;
    await microtasksFinished();
    assertEquals(`${EXPANDED_LHS_URL}&cs=0`, iframe.src);
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

  test('updates expanded width and ready state on NTP_LHS_READY', async () => {
    await createLeftHandSide();

    postLhsMessage({
      type: NtpLhsMessageType.NTP_LHS_READY,
      width: 320,
    });
    await microtasksFinished();

    const iframe = getExpandedIframe();
    assertTrue(!!iframe);
    assertEquals('320px', iframe.style.width);
    assertFalse(iframe.hidden);
    assertFalse(iframe.inert);
    assertTrue(iframe.classList.contains('ready'));
  });

  test('falls back to default width on invalid or missing width', async () => {
    await createLeftHandSide();

    for (const invalidWidth of [undefined, 0, -100, '320']) {
      postLhsMessage({
        type: NtpLhsMessageType.NTP_LHS_READY,
        width: 320,
      });
      await microtasksFinished();
      const iframe = getExpandedIframe();
      assertTrue(!!iframe);
      assertEquals('320px', iframe.style.width);

      postLhsMessage({
        type: NtpLhsMessageType.NTP_LHS_READY,
        width: invalidWidth,
      });
      await microtasksFinished();

      assertEquals('261px', iframe.style.width);
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

    const expanded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#expanded')!;
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
    await microtasksFinished();
    assertFalse(expanded.hidden);
    assertTrue(folded.hidden);

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    await microtasksFinished();
    assertTrue(expanded.hidden);
    assertFalse(folded.hidden);
  });

  test('expands LHS when folded rail is clicked in hybrid mode', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: true});
    await createLeftHandSide();

    const expanded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#expanded')!;
    const folded =
        leftHandSide.shadowRoot.querySelector<HTMLElement>('#folded')!;

    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_READY});
    postLhsMessage({type: NtpLhsMessageType.NTP_LHS_CLOSE});
    await microtasksFinished();
    assertTrue(expanded.hidden);
    assertFalse(folded.hidden);

    folded.click();
    await microtasksFinished();
    assertFalse(expanded.hidden);
    assertTrue(folded.hidden);
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
});
