// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import {assertEquals, assertStringContains} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('HighlightToggle', () => {
  let toolbar: ReadAnythingToolbarElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let highlightButton: CrIconButtonElement;

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isReadAloudEnabled = true;

    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    highlightButton =
        toolbar.shadowRoot!.querySelector<CrIconButtonElement>('#highlight')!;
  });

  suite('by default', () => {
    test('highlighting is on', () => {
      assertEquals(highlightButton.ironIcon, 'read-anything:highlight-on');
      assertStringContains(highlightButton.title, 'off');
      assertEquals(chrome.readingMode.highlightGranularity, 1);
    });
  });

  suite('on first click', () => {
    setup(() => {
      highlightButton.click();
    });

    test('highlighting is turned off', () => {
      assertEquals(highlightButton.ironIcon, 'read-anything:highlight-off');
      assertStringContains(highlightButton.title, 'on');
      assertEquals(chrome.readingMode.highlightGranularity, 0);
    });

    suite('on next click', () => {
      setup(() => {
        highlightButton.click();
      });

      test('highlighting is turned back on', () => {
        assertEquals(highlightButton.ironIcon, 'read-anything:highlight-on');
        assertStringContains(highlightButton.title, 'off');
        assertEquals(chrome.readingMode.highlightGranularity, 1);
      });
    });
  });
});
