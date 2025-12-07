// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ReadAloudSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {RateMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertNotEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, mockMetrics} from './common.js';
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

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(rateMenu);
  });

  test('rate change is propagated', async () => {
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

  test('rate change logs new rate', async () => {
    const index = 2;
    rateMenu.$.menu.currentSelectedIndex = index;
    rateMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.RATE, {detail: {data: 0.5}}));

    assertEquals(index, await metrics.whenCalled('recordVoiceSpeed'));
  });

  test('restores saved rate option', async () => {
    const rate = 1.2;
    const startingIndex = rateMenu.$.menu.currentSelectedIndex;
    assertNotEquals(rate, startingIndex);

    rateMenu.settingsPrefs = {
      letterSpacing: 0,
      lineSpacing: 0,
      theme: 0,
      speechRate: rate,
      font: '',
      highlightGranularity: 0,
    };
    await microtasksFinished();

    assertNotEquals(startingIndex, rateMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved rate is the same', async () => {
    const startingIndex = rateMenu.$.menu.currentSelectedIndex;

    rateMenu.settingsPrefs = {
      letterSpacing: 100,
      lineSpacing: 101,
      theme: 102,
      speechRate: 0,
      font: 'font',
      highlightGranularity: 103,
    };
    await microtasksFinished();

    assertEquals(startingIndex, rateMenu.$.menu.currentSelectedIndex);
  });

  // <if expr="is_chromeos">
  test('ChromeOS number of rate options correct', () => {
    // Should include 0.5, 0.8, 1, 1.2, 1.5, 2, 3, 4
    const expectedRateOptions = 8;
    const rateOptions =
        rateMenu.$.menu.$.lazyMenu.get().querySelectorAll<HTMLElement>(
            '.dropdown-item');
    assertEquals(expectedRateOptions, rateOptions.length);
  });
  // </if>

  // <if expr="not is_chromeos">
  test('Non-ChromeOS number of rate options correct', () => {
    // Should include 0.5, 0.8, 1, 1.2, 1.5, 2.
    const expectedRateOptions = 6;
    const rateOptions =
        rateMenu.$.menu.$.lazyMenu.get().querySelectorAll<HTMLElement>(
            '.dropdown-item');
    assertEquals(expectedRateOptions, rateOptions.length);
  });
  // </if>
});
