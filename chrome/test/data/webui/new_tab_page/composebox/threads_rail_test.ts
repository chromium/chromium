// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LhsRailConfig, ThreadsRailElement} from 'chrome://new-tab-page/lazy_load.js';
import {ComposeboxWindowProxy, LhsClientAction} from 'chrome://new-tab-page/lazy_load.js';
import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../test_support.js';

const AIM_THREADS_GOOGLE_SEARCH_LABEL = 'Google Search';
const GOOGLE_BASE_URL = 'https://www.google.com/';

suite('NewTabPageThreadsRailTest', () => {
  let threadsRailElement: ThreadsRailElement;
  let windowProxy: TestMock<ComposeboxWindowProxy>;

  async function createThreadsRail(config: LhsRailConfig = {items: []}) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({lhsRailConfig: config});
    threadsRailElement = document.createElement('cr-threads-rail');
    document.body.appendChild(threadsRailElement);
    await microtasksFinished();
  }

  setup(async () => {
    windowProxy = installMock(ComposeboxWindowProxy);
    loadTimeData.overrideValues({
      aimThreadsGoogleSearchLabel: AIM_THREADS_GOOGLE_SEARCH_LABEL,
      googleBaseUrl: GOOGLE_BASE_URL,
    });
    await createThreadsRail();
  });

  test(
      'renders logo button and no rail items when config is empty',
      async () => {
        const logoButton =
            threadsRailElement.shadowRoot.querySelector<HTMLElement>(
                '#logoButton');
        assertTrue(!!logoButton);
        assertTrue(isVisible(logoButton));
        assertEquals(
            AIM_THREADS_GOOGLE_SEARCH_LABEL,
            logoButton.getAttribute('aria-label'));
        assertTrue(logoButton.hasAttribute('suppress-rtl-flip'));
        const logoTooltip =
            threadsRailElement.shadowRoot.querySelector<HTMLElement>(
                'cr-tooltip[for="logoButton"]');
        assertTrue(!!logoTooltip);
        assertEquals(
            AIM_THREADS_GOOGLE_SEARCH_LABEL, logoTooltip.textContent.trim());

        assertEquals(
            0,
            threadsRailElement.shadowRoot
                .querySelectorAll('#iconsContainer cr-icon-button')
                .length);

        await createThreadsRail({});
        assertEquals(
            0,
            threadsRailElement.shadowRoot
                .querySelectorAll('#iconsContainer cr-icon-button')
                .length);
      });

  test('navigates to Google base URL on logo button click', () => {
    let hostClicked = false;
    threadsRailElement.addEventListener('click', () => {
      hostClicked = true;
    });
    const logoButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>('#logoButton');
    assertTrue(!!logoButton);

    logoButton.click();

    const args = windowProxy.getArgs('navigate');
    assertEquals(1, args.length);
    assertEquals(GOOGLE_BASE_URL, args[0]);
    assertFalse(hostClicked);
  });

  test('renders items and tooltips from lhsRailConfig', async () => {
    const historySvg = '<svg viewBox="0 0 24 24"><path d="M1 1"/></svg>';
    const newThreadSvg = '<svg viewBox="0 0 24 24"><path d="M2 2"/></svg>';
    const tasksSvg = '<svg viewBox="0 0 24 24"><path d="M3 3"/></svg>';
    await createThreadsRail({
      items: [
        {
          iconSvg: historySvg,
          name: 'AI Mode history',
          clientAction: LhsClientAction.EXPAND_RAIL,
        },
        {
          iconSvg: newThreadSvg,
          name: 'Start new AI Mode search',
          clientAction: LhsClientAction.FOCUS_REALBOX_AIM,
        },
        {
          iconSvg: tasksSvg,
          name: 'Scheduled Tasks',
          clientAction: LhsClientAction.NAVIGATE,
          targetUrl: 'https://www.google.com/search?udm=50',
        },
        {
          iconSvg: '',
          name: '',
          clientAction: LhsClientAction.UNSPECIFIED,
        },
      ],
    });

    const itemButtons =
        threadsRailElement.shadowRoot.querySelectorAll<HTMLElement>(
            '#iconsContainer cr-icon-button');
    assertEquals(4, itemButtons.length);

    const historyButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>('#railItem-0');
    assertTrue(!!historyButton);
    assertEquals(
        `--cr-icon-image: url("data:image/svg+xml;charset=utf-8,${
            encodeURIComponent(historySvg)}");`,
        historyButton.getAttribute('style'));
    assertEquals('AI Mode history', historyButton.getAttribute('aria-label'));
    const historyTooltip =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            'cr-tooltip[for="railItem-0"]');
    assertTrue(!!historyTooltip);
    assertEquals('AI Mode history', historyTooltip.textContent.trim());

    const newSearchButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>('#railItem-1');
    assertTrue(!!newSearchButton);
    assertTrue(newSearchButton.getAttribute('style')!.includes(
        encodeURIComponent(newThreadSvg)));
    assertEquals(
        'Start new AI Mode search', newSearchButton.getAttribute('aria-label'));
    const newSearchTooltip =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            'cr-tooltip[for="railItem-1"]');
    assertTrue(!!newSearchTooltip);
    assertEquals(
        'Start new AI Mode search', newSearchTooltip.textContent.trim());

    const tasksButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>('#railItem-2');
    assertTrue(!!tasksButton);
    assertTrue(tasksButton.getAttribute('style')!.includes(
        encodeURIComponent(tasksSvg)));
    assertEquals('Scheduled Tasks', tasksButton.getAttribute('aria-label'));
    const tasksTooltip =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            'cr-tooltip[for="railItem-2"]');
    assertTrue(!!tasksTooltip);
    assertEquals('Scheduled Tasks', tasksTooltip.textContent.trim());

    const emptyItemButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>('#railItem-3');
    assertTrue(!!emptyItemButton);
    assertFalse(emptyItemButton.hasAttribute('style'));
    assertFalse(emptyItemButton.hasAttribute('aria-label'));
    const emptyItemTooltip =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            'cr-tooltip[for="railItem-3"]');
    assertFalse(!!emptyItemTooltip);
  });

  test('handles click actions for configured rail items', async () => {
    const tasksUrl = 'https://www.google.com/search?udm=50&tasks=1';
    const fallbackUrl = 'https://www.google.com/search?udm=50&creations=1';
    await createThreadsRail({
      items: [
        {
          iconSvg: '<svg></svg>',
          name: 'AI Mode history',
          clientAction: LhsClientAction.EXPAND_RAIL,
          targetUrl: 'https://www.google.com/search?udm=50&atvm=1',
        },
        {
          iconSvg: '<svg></svg>',
          name: 'Start new AI Mode search',
          clientAction: LhsClientAction.FOCUS_REALBOX_AIM,
        },
        {
          iconSvg: '<svg></svg>',
          name: 'Scheduled Tasks',
          clientAction: LhsClientAction.NAVIGATE,
          targetUrl: tasksUrl,
        },
        {
          iconSvg: '<svg></svg>',
          name: 'Future Action',
          clientAction: LhsClientAction.UNSPECIFIED,
          targetUrl: fallbackUrl,
        },
      ],
    });

    let hostClicked = false;
    threadsRailElement.addEventListener('click', () => {
      hostClicked = true;
    });

    // 1. EXPAND_RAIL dispatches expand-rail without navigating.
    const expandButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>('#railItem-0');
    assertTrue(!!expandButton);
    const whenExpandRail = eventToPromise('expand-rail', threadsRailElement);
    expandButton.click();
    await whenExpandRail;
    assertEquals(0, windowProxy.getCallCount('navigate'));
    assertFalse(hostClicked);

    // 2. FOCUS_REALBOX_AIM dispatches open-composebox without navigating.
    const newSearchButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>('#railItem-1');
    assertTrue(!!newSearchButton);
    const whenOpenComposebox =
        eventToPromise('open-composebox', threadsRailElement);
    newSearchButton.click();
    await whenOpenComposebox;
    assertEquals(0, windowProxy.getCallCount('navigate'));
    assertFalse(hostClicked);

    // 3. NAVIGATE navigates to targetUrl.
    const tasksButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>('#railItem-2');
    assertTrue(!!tasksButton);
    tasksButton.click();
    assertEquals(1, windowProxy.getCallCount('navigate'));
    assertEquals(tasksUrl, windowProxy.getArgs('navigate')[0]);
    assertFalse(hostClicked);

    // 4. UNSPECIFIED falls back to targetUrl when present.
    const fallbackButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>('#railItem-3');
    assertTrue(!!fallbackButton);
    fallbackButton.click();
    assertEquals(2, windowProxy.getCallCount('navigate'));
    assertEquals(fallbackUrl, windowProxy.getArgs('navigate')[1]);
    assertFalse(hostClicked);
  });

  test('does not navigate when targetUrl is missing', async () => {
    await createThreadsRail({
      items: [
        {
          iconSvg: '<svg></svg>',
          name: 'Missing URL',
          clientAction: LhsClientAction.NAVIGATE,
        },
        {
          iconSvg: '<svg></svg>',
          name: 'Unspecified Missing URL',
          clientAction: LhsClientAction.UNSPECIFIED,
        },
      ],
    });

    const buttons = threadsRailElement.shadowRoot.querySelectorAll<HTMLElement>(
        '#iconsContainer cr-icon-button');
    assertEquals(2, buttons.length);
    for (const button of buttons) {
      button.click();
    }
    assertEquals(0, windowProxy.getCallCount('navigate'));
  });

  test('only propagates clicks directly on #rail', async () => {
    await createThreadsRail({
      items: [
        {
          iconSvg: '<svg></svg>',
          name: 'AI Mode history',
          clientAction: LhsClientAction.EXPAND_RAIL,
        },
      ],
    });
    let hostClickCount = 0;
    threadsRailElement.addEventListener('click', () => {
      hostClickCount++;
    });

    const childSelectors = [
      'cr-tooltip[for="logoButton"]',
      'cr-tooltip[for="railItem-0"]',
      '#iconsContainer',
    ];
    for (const selector of childSelectors) {
      const child =
          threadsRailElement.shadowRoot.querySelector<HTMLElement>(selector);
      assertTrue(!!child);
      child.click();
      assertEquals(0, hostClickCount);
    }

    const rail =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>('#rail');
    assertTrue(!!rail);
    rail.click();
    assertEquals(1, hostClickCount);
  });

  test(
      'resets tooltips across inert toggle and suppresses stale hover',
      async () => {
        await createThreadsRail({
          items: [
            {
              iconSvg: '<svg></svg>',
              name: 'Open sidebar',
              clientAction: LhsClientAction.EXPAND_RAIL,
            },
          ],
        });

        const rail =
            threadsRailElement.shadowRoot.querySelector<HTMLElement>('#rail');
        assertTrue(!!rail);
        const expandButton =
            threadsRailElement.shadowRoot.querySelector<HTMLElement>(
                '#railItem-0');
        assertTrue(!!expandButton);
        const initialTooltip =
            threadsRailElement.shadowRoot.querySelector<CrTooltipElement>(
                'cr-tooltip[for="railItem-0"]');
        assertTrue(!!initialTooltip);

        // Initial render does not suppress pointer hover.
        assertFalse(threadsRailElement.hasAttribute('suppress-pointer-hover_'));
        assertEquals('auto', getComputedStyle(expandButton).pointerEvents);

        // Hover until fade-in finishes so showing_ is true and
        // animationPlaying_ is false.
        expandButton.dispatchEvent(new PointerEvent('pointerenter'));
        await eventToPromise('animationend', initialTooltip.$.tooltip);
        assertFalse(initialTooltip.$.tooltip.hidden);

        // Marking the rail inert unmounts all tooltips and reflects [inert].
        threadsRailElement.inert = true;
        await microtasksFinished();
        assertTrue(threadsRailElement.hasAttribute('inert'));
        assertFalse(threadsRailElement.hasAttribute('suppress-pointer-hover_'));
        assertEquals(
            0,
            threadsRailElement.shadowRoot.querySelectorAll('cr-tooltip')
                .length);

        // Clearing inert mounts fresh hidden tooltips while reflecting
        // [suppress-pointer-hover_] so Blink's synthetic post-layout hover
        // recomputation at a stale pre-OOPIF mouse position cannot re-trigger
        // :hover or pointerenter.
        threadsRailElement.inert = false;
        await microtasksFinished();
        assertFalse(threadsRailElement.hasAttribute('inert'));
        assertTrue(threadsRailElement.hasAttribute('suppress-pointer-hover_'));
        assertEquals('none', getComputedStyle(rail).pointerEvents);
        assertEquals('none', getComputedStyle(expandButton).pointerEvents);
        const remountedTooltip =
            threadsRailElement.shadowRoot.querySelector<CrTooltipElement>(
                'cr-tooltip[for="railItem-0"]');
        assertTrue(!!remountedTooltip);
        assertTrue(remountedTooltip.$.tooltip.hidden);
        assertFalse(
            remountedTooltip.$.tooltip.classList.contains('fade-in-animation'));
        assertFalse(remountedTooltip.$.tooltip.classList.contains(
            'fade-out-animation'));

        // A real pointermove unsuppresses hover without showing tooltips until
        // a button is hovered again.
        window.dispatchEvent(new PointerEvent('pointermove'));
        await microtasksFinished();
        assertFalse(threadsRailElement.hasAttribute('suppress-pointer-hover_'));
        assertEquals('auto', getComputedStyle(expandButton).pointerEvents);
        assertTrue(remountedTooltip.$.tooltip.hidden);

        expandButton.dispatchEvent(new PointerEvent('pointerenter'));
        await eventToPromise('animationend', remountedTooltip.$.tooltip);
        assertFalse(remountedTooltip.$.tooltip.hidden);
      });

  test(
      'shows tooltip on keyboard focus immediately after leaving inert',
      async () => {
        await createThreadsRail({
          items: [
            {
              iconSvg: '<svg></svg>',
              name: 'Open sidebar',
              clientAction: LhsClientAction.EXPAND_RAIL,
            },
          ],
        });

        const expandButton =
            threadsRailElement.shadowRoot.querySelector<HTMLElement>(
                '#railItem-0')!;

        threadsRailElement.inert = true;
        await microtasksFinished();
        threadsRailElement.inert = false;
        await microtasksFinished();
        assertTrue(threadsRailElement.hasAttribute('suppress-pointer-hover_'));
        const focusedTooltip =
            threadsRailElement.shadowRoot.querySelector<CrTooltipElement>(
                'cr-tooltip[for="railItem-0"]');
        assertTrue(!!focusedTooltip);
        assertTrue(focusedTooltip.$.tooltip.hidden);

        expandButton.focus();
        await microtasksFinished();
        assertFalse(focusedTooltip.$.tooltip.hidden);
        assertTrue(threadsRailElement.hasAttribute('suppress-pointer-hover_'));
      });
});
