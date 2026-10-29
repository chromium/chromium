// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ThreadsRailElement} from 'chrome://new-tab-page/lazy_load.js';
import {ComposeboxWindowProxy} from 'chrome://new-tab-page/lazy_load.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {installMock} from '../test_support.js';

const AIM_THREADS_GOOGLE_SEARCH_LABEL = 'Google Search';
const AIM_THREADS_HISTORY_LABEL = 'AI Mode history';
const AIM_THREADS_NEW_SEARCH_LABEL = 'Start new AI Mode search';
const GOOGLE_BASE_URL = 'https://www.google.com/';
const DESKTOP_CHROME_NTP_THREADS_SOURCE = 'chrome.crn.rb';
const DESKTOP_CHROME_NTP_THREADS_ENTRY_POINT = 129;
const AIM_DISPLAY_MODE = 50;
const AIM_THREADS_VISIBILITY_MODE = 3;
const AIM_THREADS_URL = `https://www.google.com/search?udm=${
    AIM_DISPLAY_MODE}&aep=${DESKTOP_CHROME_NTP_THREADS_ENTRY_POINT}&atvm=${
    AIM_THREADS_VISIBILITY_MODE}&source=${DESKTOP_CHROME_NTP_THREADS_SOURCE}`;

suite('NewTabPageThreadsRailTest', () => {
  let threadsRailElement: ThreadsRailElement;
  let windowProxy: TestMock<ComposeboxWindowProxy>;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    windowProxy = installMock(ComposeboxWindowProxy);
    metrics = fakeMetricsPrivate();
    loadTimeData.overrideValues({
      aimThreadsGoogleSearchLabel: AIM_THREADS_GOOGLE_SEARCH_LABEL,
      aimThreadsHistoryLabel: AIM_THREADS_HISTORY_LABEL,
      aimThreadsNewSearchLabel: AIM_THREADS_NEW_SEARCH_LABEL,
      googleBaseUrl: GOOGLE_BASE_URL,
      threadsUrl: AIM_THREADS_URL,
    });
    threadsRailElement = document.createElement('cr-threads-rail');
    document.body.appendChild(threadsRailElement);
    return threadsRailElement.updateComplete;
  });

  teardown(() => {
    document.body.removeChild(threadsRailElement);
  });

  test('renders logo and icon buttons with correct tooltips', () => {
    const logoButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>('#logoButton');
    assertTrue(!!logoButton);
    assertTrue(isVisible(logoButton));
    assertEquals(
        loadTimeData.getString('aimThreadsGoogleSearchLabel'),
        logoButton.getAttribute('aria-label'));
    assertTrue(logoButton.hasAttribute('suppress-rtl-flip'));
    const logoTooltip =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            'cr-tooltip[for="logoButton"]');
    assertTrue(!!logoTooltip);
    assertEquals(
        loadTimeData.getString('aimThreadsGoogleSearchLabel'),
        logoTooltip.textContent.trim());

    const historyButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '#showHistoryButton');
    assertTrue(!!historyButton);
    assertEquals(
        loadTimeData.getString('aimThreadsHistoryLabel'),
        historyButton.getAttribute('aria-label'));
    const historyTooltip =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            'cr-tooltip[for="showHistoryButton"]');
    assertTrue(!!historyTooltip);
    assertEquals(
        loadTimeData.getString('aimThreadsHistoryLabel'),
        historyTooltip.textContent.trim());

    const newSearchButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '#newSearchButton');
    assertTrue(!!newSearchButton);
    assertEquals(
        loadTimeData.getString('aimThreadsNewSearchLabel'),
        newSearchButton.getAttribute('aria-label'));
    const newSearchTooltip =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            'cr-tooltip[for="newSearchButton"]');
    assertTrue(!!newSearchTooltip);
    assertEquals(
        loadTimeData.getString('aimThreadsNewSearchLabel'),
        newSearchTooltip.textContent.trim());

    const tasksButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '#tasksButton');
    assertTrue(!!tasksButton);
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
    assertEquals(loadTimeData.getString('googleBaseUrl'), args[0]);
    assertFalse(hostClicked);
  });

  test('navigates and records metric on show history button click', () => {
    let hostClicked = false;
    threadsRailElement.addEventListener('click', () => {
      hostClicked = true;
    });
    const historyButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '#showHistoryButton');
    assertTrue(!!historyButton);

    historyButton.click();

    const args = windowProxy.getArgs('navigate');
    assertEquals(1, args.length);
    assertEquals(loadTimeData.getString('threadsUrl'), args[0]);
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ThreadsRail.Action',
            0 /* ThreadsAction.SHOW_HISTORY_CLICKED */));
    assertFalse(hostClicked);
  });
});
