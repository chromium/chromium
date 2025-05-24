// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {FakeReadingMode} from './fake_reading_mode.js';

// TODO: crbug.com/1474951 - Remove this test once Read Aloud flag is removed.
suite('ReadAloudFlag', () => {
  let toolbar: ReadAnythingToolbarElement;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
  });

  function createToolbar(): Promise<void> {
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    return microtasksFinished();
  }

  test('read aloud is visible if enabled', async () => {
    chrome.readingMode.isReadAloudEnabled = true;
    await createToolbar();

    const audioControls = toolbar.shadowRoot.querySelector('#audio-controls');

    assertTrue(!!audioControls);
  });

  test('read aloud is invisible if disabled', async () => {
    chrome.readingMode.isReadAloudEnabled = false;
    await createToolbar();

    const audioControls = toolbar.shadowRoot.querySelector('#audio-controls');

    assertFalse(!!audioControls);
  });
});
