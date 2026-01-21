// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ThreadsRailElement} from 'chrome://new-tab-page/lazy_load.js';
import {ComposeboxWindowProxy} from 'chrome://new-tab-page/lazy_load.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';

import {installMock} from '../test_support.js';

const AIM_THREADS_HISTORY_LABEL = 'AI Mode history';
const AIM_THREADS_URL = 'https://www.google.com/search?udm=50&atvm=1';

suite('NewTabPageThreadsRailTest', () => {
  let threadsRailElement: ThreadsRailElement;
  let windowProxy: TestMock<ComposeboxWindowProxy>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    windowProxy = installMock(ComposeboxWindowProxy);
    loadTimeData.overrideValues({
      aimThreadsHistoryLabel: AIM_THREADS_HISTORY_LABEL,
      threadsUrl: AIM_THREADS_URL,
    });
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
    assertEquals(loadTimeData.getString('threadsUrl'), args[0]);
  });
});
