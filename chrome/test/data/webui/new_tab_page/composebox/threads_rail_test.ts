// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AI_MODE_HISTORY_URL} from 'chrome://resources/cr_components/composebox/threads_rail.js';
import type {ThreadsRailElement} from 'chrome://resources/cr_components/composebox/threads_rail.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';

import {installMock} from '../test_support.js';

const AIM_THREADS_HISTORY_LABEL = 'AI Mode history';

suite('NewTabPageThreadsRailTest', () => {
  let threadsRailElement: ThreadsRailElement;
  let windowProxy: TestMock<WindowProxy>;

  setup(() => {
    loadTimeData.resetForTesting(
        {aimThreadsHistoryLabel: AIM_THREADS_HISTORY_LABEL});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    windowProxy = installMock(WindowProxy);
    threadsRailElement = document.createElement('cr-threads-rail');
    document.body.appendChild(threadsRailElement);
    return threadsRailElement.updateComplete;
  });

  teardown(() => {
    document.body.removeChild(threadsRailElement);
  });

  test('history button has correct tooltip', () => {
    const historyButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '#showHistoryButton');
    assertTrue(!!historyButton);
    assertEquals(
        loadTimeData.getString('aimThreadsHistoryLabel'), historyButton.title);
  });

  test('open AI Mode history on show history button click', () => {
    const historyButton =
        threadsRailElement.shadowRoot.querySelector<HTMLElement>(
            '#showHistoryButton');
    assertTrue(!!historyButton);

    historyButton.click();
    const args = windowProxy.getArgs('navigate');
    assertEquals(1, args.length);
    assertEquals(AI_MODE_HISTORY_URL, args[0]);
  });
});
