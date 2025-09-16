// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy, SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement, SpEmptyStateElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createApp, mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('EmptyState', () => {
  let app: AppElement;
  let speechController: SpeechController;
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
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
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

  test('showEmpty logs empty state once', async () => {
    app.showEmpty();
    await microtasksFinished();
    assertEquals(1, metrics.getCallCount('recordEmptyState'));
    assertTrue(app.isEmptyState());

    app.showEmpty();
    await microtasksFinished();
    assertEquals(1, metrics.getCallCount('recordEmptyState'));
    assertTrue(app.isEmptyState());
  });

  test('updateContent after loading with no content shows empty', async () => {
    chrome.readingMode.rootId = 1;
    chrome.readingMode.getTextContent = () => '';
    app.showLoading();
    await microtasksFinished();

    app.updateContent();
    await microtasksFinished();

    assertEquals(1, metrics.getCallCount('recordEmptyState'));
    assertTrue(app.isEmptyState());
  });
});
