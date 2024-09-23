// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MetricsBrowserProxyImpl, ReadAnythingLogger, ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LetterSpacingMenu} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {emitEventWithTarget, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('LetterSpacing', () => {
  let letterSpacingMenu: LetterSpacingMenu;
  let metrics: TestMetricsBrowserProxy;

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    metrics = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metrics);
    ReadAnythingLogger.setInstance(new ReadAnythingLogger());

    letterSpacingMenu = document.createElement('letter-spacing-menu');
    document.body.appendChild(letterSpacingMenu);
    flush();
  });

  test('spacing change', async () => {
    const veryWide = chrome.readingMode.veryWideLetterSpacing;
    emitEventWithTarget(
        letterSpacingMenu.$.menu, ToolbarEvent.LETTER_SPACING,
        {detail: {data: veryWide}});
    assertEquals(veryWide, chrome.readingMode.letterSpacing);

    const wide = chrome.readingMode.wideLetterSpacing;
    emitEventWithTarget(
        letterSpacingMenu.$.menu, ToolbarEvent.LETTER_SPACING,
        {detail: {data: wide}});
    assertEquals(wide, chrome.readingMode.letterSpacing);

    const standard = chrome.readingMode.standardLetterSpacing;
    emitEventWithTarget(
        letterSpacingMenu.$.menu, ToolbarEvent.LETTER_SPACING,
        {detail: {data: standard}});
    assertEquals(standard, chrome.readingMode.letterSpacing);

    assertEquals(
        ReadAnythingSettingsChange.LETTER_SPACING_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(3, metrics.getCallCount('recordTextSettingsChange'));
  });
});
