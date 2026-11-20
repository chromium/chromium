// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LhsRailConfig, ThreadsRailElement} from 'chrome://new-tab-page/lazy_load.js';
import {ComposeboxWindowProxy, LhsClientAction, ThreadsRailAction} from 'chrome://new-tab-page/lazy_load.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../test_support.js';

const AIM_THREADS_GOOGLE_SEARCH_LABEL = 'Google Search';
const GOOGLE_BASE_URL = 'https://www.google.com/';

suite('NewTabPageThreadsRailTest', () => {
  let threadsRailElement: ThreadsRailElement;
  let windowProxy: TestMock<ComposeboxWindowProxy>;
  let metrics: MetricsTracker;

  async function createThreadsRail(config: LhsRailConfig = {items: []}) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({lhsRailConfig: config});
    threadsRailElement = document.createElement('cr-threads-rail');
    document.body.appendChild(threadsRailElement);
    await microtasksFinished();
  }

  setup(async () => {
    metrics = fakeMetricsPrivate();
    windowProxy = installMock(ComposeboxWindowProxy);
    loadTimeData.overrideValues({
      aimThreadsGoogleSearchLabel: AIM_THREADS_GOOGLE_SEARCH_LABEL,
      googleBaseUrl: GOOGLE_BASE_URL,
    });
    await createThreadsRail();
  });

  test('records shown metric when attached to DOM', () => {
    assertEquals(1, metrics.count('NewTabPage.ThreadsRail.Shown', true));
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
            AIM_THREADS_GOOGLE_SEARCH_LABEL, logoButton.getAttribute('title'));
        assertTrue(logoButton.hasAttribute('suppress-rtl-flip'));

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
    assertEquals(1, metrics.count('NewTabPage.ThreadsRail.Action'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.Action', ThreadsRailAction.LOGO_CLICKED));
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
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '[data-index="0"]');
    assertTrue(!!historyButton);
    assertEquals(
        `--cr-icon-image: url("data:image/svg+xml;charset=utf-8,${
            encodeURIComponent(historySvg)}");`,
        historyButton.getAttribute('style'));
    assertEquals('AI Mode history', historyButton.getAttribute('title'));

    const newSearchButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '[data-index="1"]');
    assertTrue(!!newSearchButton);
    assertTrue(newSearchButton.getAttribute('style')!.includes(
        encodeURIComponent(newThreadSvg)));
    assertEquals(
        'Start new AI Mode search', newSearchButton.getAttribute('title'));

    const tasksButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '[data-index="2"]');
    assertTrue(!!tasksButton);
    assertTrue(tasksButton.getAttribute('style')!.includes(
        encodeURIComponent(tasksSvg)));
    assertEquals('Scheduled Tasks', tasksButton.getAttribute('title'));

    const emptyItemButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '[data-index="3"]');
    assertTrue(!!emptyItemButton);
    assertFalse(emptyItemButton.hasAttribute('style'));
    assertFalse(emptyItemButton.hasAttribute('title'));
  });

  test('handles click actions for configured rail items', async () => {
    const tasksUrl = 'https://www.google.com/search?udm=50&tasks=1';
    const unspecifiedUrl = 'https://www.google.com/search?udm=50&creations=1';
    const unknownActionUrl = 'https://www.google.com/search?udm=50&future=1';
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
          name: 'Unspecified Action',
          clientAction: LhsClientAction.UNSPECIFIED,
          targetUrl: unspecifiedUrl,
        },
        {
          iconSvg: '<svg></svg>',
          name: 'Future Action',
          clientAction: 99 as LhsClientAction,
          targetUrl: unknownActionUrl,
        },
      ],
    });

    let hostClicked = false;
    threadsRailElement.addEventListener('click', () => {
      hostClicked = true;
    });

    // 1. EXPAND_RAIL dispatches expand-rail without navigating.
    const expandButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '[data-index="0"]');
    assertTrue(!!expandButton);
    const whenExpandRail = eventToPromise('expand-rail', threadsRailElement);
    expandButton.click();
    await whenExpandRail;
    assertEquals(0, windowProxy.getCallCount('navigate'));
    assertFalse(hostClicked);
    assertEquals(1, metrics.count('NewTabPage.ThreadsRail.Action'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.Action',
            ThreadsRailAction.EXPAND_RAIL_ITEM_CLICKED));

    // 2. FOCUS_REALBOX_AIM dispatches open-composebox without navigating.
    const newSearchButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '[data-index="1"]');
    assertTrue(!!newSearchButton);
    const whenOpenComposebox =
        eventToPromise('open-composebox', threadsRailElement);
    newSearchButton.click();
    await whenOpenComposebox;
    assertEquals(0, windowProxy.getCallCount('navigate'));
    assertFalse(hostClicked);
    assertEquals(2, metrics.count('NewTabPage.ThreadsRail.Action'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.Action',
            ThreadsRailAction.FOCUS_REALBOX_AIM_ITEM_CLICKED));

    // 3. NAVIGATE navigates to targetUrl.
    const tasksButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '[data-index="2"]');
    assertTrue(!!tasksButton);
    tasksButton.click();
    assertEquals(1, windowProxy.getCallCount('navigate'));
    assertEquals(tasksUrl, windowProxy.getArgs('navigate')[0]);
    assertFalse(hostClicked);
    assertEquals(3, metrics.count('NewTabPage.ThreadsRail.Action'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.Action',
            ThreadsRailAction.NAVIGATE_ITEM_CLICKED));

    // 4. UNSPECIFIED navigates to targetUrl and records
    // UNSPECIFIED_NAVIGATE_ITEM_CLICKED.
    const unspecifiedButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '[data-index="3"]');
    assertTrue(!!unspecifiedButton);
    unspecifiedButton.click();
    assertEquals(2, windowProxy.getCallCount('navigate'));
    assertEquals(unspecifiedUrl, windowProxy.getArgs('navigate')[1]);
    assertFalse(hostClicked);
    assertEquals(4, metrics.count('NewTabPage.ThreadsRail.Action'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.Action',
            ThreadsRailAction.UNSPECIFIED_NAVIGATE_ITEM_CLICKED));

    // 5. Unrecognized future ClientAction values fall back to targetUrl and
    // record FALLBACK_NAVIGATE_ITEM_CLICKED.
    const unknownActionButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '[data-index="4"]');
    assertTrue(!!unknownActionButton);
    unknownActionButton.click();
    assertEquals(3, windowProxy.getCallCount('navigate'));
    assertEquals(unknownActionUrl, windowProxy.getArgs('navigate')[2]);
    assertFalse(hostClicked);
    assertEquals(5, metrics.count('NewTabPage.ThreadsRail.Action'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.Action',
            ThreadsRailAction.FALLBACK_NAVIGATE_ITEM_CLICKED));
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
        {
          iconSvg: '<svg></svg>',
          name: 'Unknown Action Missing URL',
          clientAction: 99 as LhsClientAction,
        },
      ],
    });

    const buttons = threadsRailElement.shadowRoot.querySelectorAll<HTMLElement>(
        '#iconsContainer cr-icon-button');
    assertEquals(3, buttons.length);
    for (const button of buttons) {
      button.click();
    }
    assertEquals(0, windowProxy.getCallCount('navigate'));
    assertEquals(0, metrics.count('NewTabPage.ThreadsRail.Action'));
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

    const iconsContainer =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '#iconsContainer');
    assertTrue(!!iconsContainer);
    iconsContainer.click();
    assertEquals(0, hostClickCount);
    assertEquals(0, metrics.count('NewTabPage.ThreadsRail.Action'));

    const rail =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>('#rail');
    assertTrue(!!rail);
    rail.click();
    assertEquals(1, hostClickCount);
    assertEquals(1, metrics.count('NewTabPage.ThreadsRail.Action'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.Action',
            ThreadsRailAction.RAIL_BACKGROUND_CLICKED));
  });

  test('suppresses stale pointer hover after leaving inert', async () => {
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
            '[data-index="0"]');
    assertTrue(!!expandButton);

    // Initial render does not suppress pointer hover.
    assertFalse(threadsRailElement.hasAttribute('suppress-pointer-hover_'));
    assertEquals('auto', getComputedStyle(expandButton).pointerEvents);

    // Becoming inert does not suppress pointer hover.
    threadsRailElement.inert = true;
    await microtasksFinished();
    assertTrue(threadsRailElement.hasAttribute('inert'));
    assertFalse(threadsRailElement.hasAttribute('suppress-pointer-hover_'));

    // Leaving inert suppresses pointer hit-testing so Blink's hover
    // recomputation at a stale mouse position cannot re-trigger :hover.
    threadsRailElement.inert = false;
    await microtasksFinished();
    assertFalse(threadsRailElement.hasAttribute('inert'));
    assertTrue(threadsRailElement.hasAttribute('suppress-pointer-hover_'));
    assertEquals('none', getComputedStyle(rail).pointerEvents);
    assertEquals('none', getComputedStyle(expandButton).pointerEvents);

    // A real pointermove restores pointer hit-testing.
    window.dispatchEvent(new PointerEvent('pointermove'));
    await microtasksFinished();
    assertFalse(threadsRailElement.hasAttribute('suppress-pointer-hover_'));
    assertEquals('auto', getComputedStyle(expandButton).pointerEvents);
  });

  test(
      'keeps rail buttons focusable while pointer hover is suppressed',
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
                '[data-index="0"]')!;

        threadsRailElement.inert = true;
        await microtasksFinished();
        threadsRailElement.inert = false;
        await microtasksFinished();
        assertTrue(threadsRailElement.hasAttribute('suppress-pointer-hover_'));

        expandButton.focus();
        await microtasksFinished();
        assertEquals(expandButton, threadsRailElement.shadowRoot.activeElement);
        assertTrue(threadsRailElement.hasAttribute('suppress-pointer-hover_'));
      });

  test('focuses the item with the requested action', async () => {
    await createThreadsRail({
      items: [
        {
          iconSvg: '<svg></svg>',
          name: 'New thread',
          clientAction: LhsClientAction.FOCUS_REALBOX_AIM,
        },
        {
          iconSvg: '<svg></svg>',
          name: 'Open sidebar',
          clientAction: LhsClientAction.EXPAND_RAIL,
        },
      ],
    });

    const newThreadButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '[data-index="0"]')!;
    const expandButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '[data-index="1"]')!;

    threadsRailElement.focusItemWithAction(LhsClientAction.FOCUS_REALBOX_AIM);
    assertEquals(newThreadButton, threadsRailElement.shadowRoot.activeElement);

    threadsRailElement.focusItemWithAction(LhsClientAction.EXPAND_RAIL);
    await microtasksFinished();

    assertEquals(expandButton, threadsRailElement.shadowRoot.activeElement);
  });
});
