// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LineSpacingMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('LineSpacing', () => {
  let lineSpacingMenu: LineSpacingMenuElement;
  let metrics: TestMetricsBrowserProxy;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    metrics = mockMetrics();

    lineSpacingMenu = document.createElement('line-spacing-menu');
    document.body.appendChild(lineSpacingMenu);
  });

  test('spacing change', async () => {
    const veryLoose = chrome.readingMode.veryLooseLineSpacing;
    lineSpacingMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_SPACING, {detail: {data: veryLoose}}));
    assertEquals(veryLoose, chrome.readingMode.lineSpacing);

    const loose = chrome.readingMode.looseLineSpacing;
    lineSpacingMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.LINE_SPACING, {detail: {data: loose}}));
    assertEquals(loose, chrome.readingMode.lineSpacing);

    const standard = chrome.readingMode.standardLineSpacing;
    lineSpacingMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.LINE_SPACING, {detail: {data: standard}}));
    assertEquals(standard, chrome.readingMode.lineSpacing);

    assertEquals(
        ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(3, metrics.getCallCount('recordTextSettingsChange'));
  });
});
