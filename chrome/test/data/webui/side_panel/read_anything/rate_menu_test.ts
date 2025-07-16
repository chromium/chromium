// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ReadAloudSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {RateMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('RateMenuElement', () => {
  let rateMenu: RateMenuElement;
  let metrics: TestMetricsBrowserProxy;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    metrics = mockMetrics();

    rateMenu = document.createElement('rate-menu') as RateMenuElement;
    document.body.appendChild(rateMenu);
  });

  test('rate change', async () => {
    const rate1 = 1;
    rateMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.RATE, {detail: {data: rate1}}));
    assertEquals(rate1, chrome.readingMode.speechRate);

    const rate2 = 0.5;
    rateMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.RATE, {detail: {data: rate2}}));
    assertEquals(rate2, chrome.readingMode.speechRate);

    const rate3 = 4;
    rateMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.RATE, {detail: {data: rate3}}));
    assertEquals(rate3, chrome.readingMode.speechRate);

    assertEquals(
        ReadAloudSettingsChange.VOICE_SPEED_CHANGE,
        await metrics.whenCalled('recordSpeechSettingsChange'));
    assertEquals(3, metrics.getCallCount('recordSpeechSettingsChange'));
  });
});
