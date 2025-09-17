// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy, LOG_EMPTY_DELAY_MS} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement, SpEmptyStateElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createApp, mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('EmptyState', () => {
  let app: AppElement;
  let metrics: TestMetricsBrowserProxy;

  function getEmptyState() {
    const empty =
        app.shadowRoot.querySelector<SpEmptyStateElement>('sp-empty-state');
    assertTrue(!!empty);
    return empty;
  }

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.rootId = 1;
    metrics = mockMetrics();

    app = await createApp();
  });

  test('showEmpty shows empty', async () => {
    const emptyPath = 'empty_state.svg';

    app.showEmpty();
    await microtasksFinished();

    const empty = getEmptyState();
    assertTrue(app.isEmptyState());
    assertStringContains(empty.darkImagePath, emptyPath);
    assertStringContains(empty.imagePath, emptyPath);
  });

  test('showEmpty logs if still empty after delay', () => {
    const mockTimer = new MockTimer();
    mockTimer.install();

    app.showEmpty();
    assertTrue(app.isEmptyState());
    assertEquals(0, metrics.getCallCount('recordEmptyState'));

    mockTimer.tick(LOG_EMPTY_DELAY_MS);
    assertTrue(app.isEmptyState());
    assertEquals(1, metrics.getCallCount('recordEmptyState'));

    mockTimer.uninstall();
  });

  test('showEmpty does not log if not empty after delay', () => {
    const mockTimer = new MockTimer();
    mockTimer.install();

    app.showEmpty();
    assertEquals(0, metrics.getCallCount('recordEmptyState'));

    app.updateContent();
    mockTimer.tick(LOG_EMPTY_DELAY_MS);
    assertEquals(0, metrics.getCallCount('recordEmptyState'));

    mockTimer.uninstall();
  });

  test('showEmpty logs empty state once if still empty', () => {
    const mockTimer = new MockTimer();
    mockTimer.install();

    app.showEmpty();
    mockTimer.tick(LOG_EMPTY_DELAY_MS);
    assertEquals(1, metrics.getCallCount('recordEmptyState'));
    assertTrue(app.isEmptyState());

    app.showEmpty();
    mockTimer.tick(LOG_EMPTY_DELAY_MS);
    assertEquals(1, metrics.getCallCount('recordEmptyState'));
    assertTrue(app.isEmptyState());

    mockTimer.uninstall();
  });

  test('updateContent after loading with no content shows empty', async () => {
    chrome.readingMode.getTextContent = () => '';
    app.showLoading();
    await microtasksFinished();
    const mockTimer = new MockTimer();
    mockTimer.install();

    app.updateContent();
    mockTimer.tick(LOG_EMPTY_DELAY_MS);

    assertEquals(1, metrics.getCallCount('recordEmptyState'));
    assertTrue(app.isEmptyState());
    mockTimer.uninstall();
  });
});
