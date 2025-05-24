// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ColorMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('ColorMenuElement', () => {
  let colorMenu: ColorMenuElement;
  let metrics: TestMetricsBrowserProxy;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    metrics = mockMetrics();

    colorMenu = document.createElement('color-menu');
    document.body.appendChild(colorMenu);
  });

  test('theme change', async () => {
    const theme1 = chrome.readingMode.blueTheme;
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme1}}));
    assertEquals(theme1, chrome.readingMode.colorTheme);

    const theme2 = chrome.readingMode.defaultTheme;
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme2}}));
    assertEquals(theme2, chrome.readingMode.colorTheme);

    const theme3 = chrome.readingMode.darkTheme;
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme3}}));
    assertEquals(theme3, chrome.readingMode.colorTheme);

    assertEquals(
        ReadAnythingSettingsChange.THEME_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(3, metrics.getCallCount('recordTextSettingsChange'));
  });
});
