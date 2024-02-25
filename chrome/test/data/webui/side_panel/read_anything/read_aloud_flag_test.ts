// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import type {DomIf} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

// TODO: crbug.com/1474951 - Remove this test once Read Aloud flag is removed.
suite('ReadAloudFlagTest', () => {
  let toolbar: ReadAnythingToolbarElement;

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
  });

  /**
   * Suppresses harmless ResizeObserver errors due to a browser bug.
   * yaqs/2300708289911980032
   */
  function suppressInnocuousErrors() {
    const onerror = window.onerror;
    window.onerror = (message, url, lineNumber, column, error) => {
      if ([
            'ResizeObserver loop limit exceeded',
            'ResizeObserver loop completed with undelivered notifications.',
          ].includes(message.toString())) {
        return;
      }
      if (onerror) {
        onerror.apply(window, [message, url, lineNumber, column, error]);
      }
    };
  }

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
