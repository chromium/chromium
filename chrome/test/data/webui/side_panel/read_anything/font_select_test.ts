// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ReadAnythingSettingsChange} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {FontSelectElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('FontSelect', () => {
  let fontSelect: FontSelectElement;
  let metrics: TestMetricsBrowserProxy;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.supportedFonts = [];
    metrics = mockMetrics();
    fontSelect = document.createElement('font-select');
    document.body.appendChild(fontSelect);
  });

  async function updateFonts(supportedFonts: string[]): Promise<void> {
    chrome.readingMode.supportedFonts = supportedFonts;
    fontSelect.pageLanguage = 'it-it' + supportedFonts.length;
    return microtasksFinished();
  }

  test('updates fonts on page language change', async () => {
    chrome.readingMode.supportedFonts =
        ['font 1', 'font 2', 'font 3', 'font 4'];
    fontSelect.pageLanguage = 'hi';
    await microtasksFinished();
    assertEquals(4, fontSelect.options.length);

    chrome.readingMode.supportedFonts = ['font 1', 'font 2'];
    fontSelect.pageLanguage = 'jp';
    await microtasksFinished();
    assertEquals(2, fontSelect.options.length);
  });

  test('updates font titles on fonts loaded', async () => {
    chrome.readingMode.supportedFonts = ['font 1', 'font 2', 'font 3'];
    const options =
        Array.from(fontSelect.querySelectorAll<HTMLOptionElement>('option'));
    assertTrue(options.every(option => option.innerText.includes('(loading)')));

    fontSelect.areFontsLoaded = true;
    await microtasksFinished();

    assertFalse(options.some(option => option.innerText.includes('(loading)')));
  });

  test('updates fonts when settings are restored', async () => {
    chrome.readingMode.supportedFonts = ['font 1', 'font 2', 'font 3'];
    chrome.readingMode.fontName = 'font 1';
    fontSelect.areFontsLoaded = true;
    await microtasksFinished();
    assertEquals(0, fontSelect.$.select.selectedIndex);

    chrome.readingMode.fontName = 'font 2';
    fontSelect.settingsPrefs = {
      letterSpacing: 0,
      lineSpacing: 0,
      theme: 0,
      speechRate: 0,
      font: chrome.readingMode.fontName,
      highlightGranularity: 0,
    };
    await microtasksFinished();

    assertEquals(1, fontSelect.$.select.selectedIndex);
  });

  test('uses the first font if font not available', async () => {
    // Set the current font to one that will be removed
    const defaultFont = 'EB Garamond';
    const fonts = ['Andika', 'Poppins', 'STIX Two Text'];
    chrome.readingMode.fontName = defaultFont;
    await updateFonts(fonts.concat(chrome.readingMode.fontName));

    // Update the fonts to exclude the previously chosen font
    await updateFonts(fonts);

    assertEquals(0, fontSelect.$.select.selectedIndex);
    // Avoid overriding the user default font
    assertEquals(defaultFont, chrome.readingMode.fontName);
  });

  test('propagates font', async () => {
    const fonts = ['Andika', 'Poppins', 'STIX Two Text'];
    await updateFonts(fonts.concat(chrome.readingMode.fontName));

    fontSelect.$.select.selectedIndex = 0;
    fontSelect.$.select.dispatchEvent(new Event('change', {bubbles: true}));
    assertEquals(fonts[0]!, chrome.readingMode.fontName);

    fontSelect.$.select.selectedIndex = 1;
    fontSelect.$.select.dispatchEvent(new Event('change', {bubbles: true}));
    assertEquals(fonts[1]!, chrome.readingMode.fontName);

    fontSelect.$.select.selectedIndex = 2;
    fontSelect.$.select.dispatchEvent(new Event('change', {bubbles: true}));
    assertEquals(fonts[2]!, chrome.readingMode.fontName);

    assertEquals(
        ReadAnythingSettingsChange.FONT_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(3, metrics.getCallCount('recordTextSettingsChange'));
  });
});
