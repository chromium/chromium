// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {HighlightMenu} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {emitEventWithTarget, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';


suite('HighlightMenu', () => {
  let highlightMenu: HighlightMenu;

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isPhraseHighlightingEnabled = true;

    highlightMenu = document.createElement('highlight-menu');
    document.body.appendChild(highlightMenu);
    flush();
  });

  test('highlight granularity change', async () => {
    const highlights = [
      chrome.readingMode.autoHighlighting,
      chrome.readingMode.wordHighlighting,
      chrome.readingMode.phraseHighlighting,
      chrome.readingMode.sentenceHighlighting,
      chrome.readingMode.noHighlighting,
    ];

    highlights.forEach(highlight => {
      emitEventWithTarget(
          highlightMenu.$.menu, ToolbarEvent.HIGHLIGHT_CHANGE,
          {detail: {data: highlight}});
      assertEquals(highlight, chrome.readingMode.highlightGranularity);
    });
  });
});
