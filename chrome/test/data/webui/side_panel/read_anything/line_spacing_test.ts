// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MetricsBrowserProxyImpl, ReadAnythingLogger, ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LineSpacingMenu} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {emitEventWithTarget, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('LineSpacing', () => {
  let lineSpacingMenu: LineSpacingMenu;
  let metrics: TestMetricsBrowserProxy;

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    metrics = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metrics);
    ReadAnythingLogger.setInstance(new ReadAnythingLogger());

    lineSpacingMenu = document.createElement('line-spacing-menu');
    document.body.appendChild(lineSpacingMenu);
    flush();
  });

  test('spacing change', async () => {
    const veryLoose = chrome.readingMode.veryLooseLineSpacing;
    emitEventWithTarget(
        lineSpacingMenu.$.menu, ToolbarEvent.LINE_SPACING,
        {detail: {data: veryLoose}});
    assertEquals(veryLoose, chrome.readingMode.lineSpacing);

    const loose = chrome.readingMode.looseLineSpacing;
    emitEventWithTarget(
        lineSpacingMenu.$.menu, ToolbarEvent.LINE_SPACING,
        {detail: {data: loose}});
    assertEquals(loose, chrome.readingMode.lineSpacing);

    const standard = chrome.readingMode.standardLineSpacing;
    emitEventWithTarget(
        lineSpacingMenu.$.menu, ToolbarEvent.LINE_SPACING,
        {detail: {data: standard}});
    assertEquals(standard, chrome.readingMode.lineSpacing);

    assertEquals(
        ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(3, metrics.getCallCount('recordTextSettingsChange'));
  });
});
