// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {LineFocusMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {assertCheckMarksForDropdown, mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

suite('LineFocusMenuElement', () => {
  let lineFocusMenu: LineFocusMenuElement;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    mockMetrics();

    lineFocusMenu = document.createElement('line-focus-menu');
    document.body.appendChild(lineFocusMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(lineFocusMenu);
  });
});
