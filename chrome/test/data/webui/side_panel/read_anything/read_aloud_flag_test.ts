// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {DomIf} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

// TODO: crbug.com/1474951 - Remove this test once Read Aloud flag is removed.
suite('ReadAloudFlag', () => {
  let toolbar: ReadAnythingToolbarElement;

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
  });

  function createToolbar(): void {
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
  }

  test('read aloud container is visible if enabled', () => {
    chrome.readingMode.isReadAloudEnabled = true;
    createToolbar();

    const container =
        toolbar.shadowRoot!.querySelector<DomIf>('#read-aloud-container');

    assertTrue(!!container);
    assertTrue(container.if !);
  });

  test('read aloud container is invisible if disabled', () => {
    chrome.readingMode.isReadAloudEnabled = false;
    createToolbar();

    const container =
        toolbar.shadowRoot!.querySelector<DomIf>('#read-aloud-container');

    assertTrue(!!container);
    assertFalse(container.if !);
  });
});
