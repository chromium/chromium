// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LhsRailConfig, ThreadsRailElement} from 'chrome://new-tab-page/lazy_load.js';
import {ComposeboxWindowProxy, LhsClientAction} from 'chrome://new-tab-page/lazy_load.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
});
