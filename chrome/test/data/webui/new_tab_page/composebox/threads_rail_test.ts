// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ThreadsRailElement} from 'chrome://new-tab-page/lazy_load.js';
import {ComposeboxWindowProxy} from 'chrome://new-tab-page/lazy_load.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';

import {installMock} from '../test_support.js';

const AIM_THREADS_HISTORY_LABEL = 'AI Mode history';
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
      aimThreadsHistoryLabel: AIM_THREADS_HISTORY_LABEL,
      threadsUrl: AIM_THREADS_URL,
      enableThreadsRailLogo: true,
    });
    threadsRailElement = document.createElement('cr-threads-rail');
    document.body.appendChild(threadsRailElement);
    return threadsRailElement.updateComplete;
  });

  teardown(() => {
    document.body.removeChild(threadsRailElement);
  });

  test('Logo shows on rail when enabled', async () => {
    const threadsRail = document.createElement('cr-threads-rail');
    document.body.appendChild(threadsRail);
    await threadsRail.updateComplete;

    const logo = threadsRail.shadowRoot.querySelector<HTMLElement>('#logo');
    assertTrue(!!logo);
  });

  test('Logo does not show on rail when disabled', async () => {
    loadTimeData.overrideValues({enableThreadsRailLogo: false});
    const threadsRail = document.createElement('cr-threads-rail');
    document.body.appendChild(threadsRail);
    await threadsRail.updateComplete;

    const logo = threadsRail.shadowRoot.querySelector<HTMLElement>('#logo');
    assertEquals(null, logo);
  });

  test('history button has correct tooltip', () => {
    const historyButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '#showHistoryButton');
    assertTrue(!!historyButton);
    assertEquals(
        loadTimeData.getString('aimThreadsHistoryLabel'), historyButton.title);
  });

  test('navigates and records metric on show history button click', () => {
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
  });
});
