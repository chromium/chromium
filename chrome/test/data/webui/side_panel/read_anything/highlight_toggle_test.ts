// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('HighlightToggle', () => {
  let app: AppElement;
  let toolbar: ReadAnythingToolbarElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let highlightButton: CrIconButtonElement;
  let highlightEmitted: boolean;

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isReadAloudEnabled = true;
    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    flush();

    toolbar = app.$.toolbar;
    highlightButton =
        toolbar.$.toolbarContainer.querySelector<CrIconButtonElement>(
            '#highlight')!;

    assertNotEquals(toolbar, null, 'toolbar null');
    assertNotEquals(highlightButton, null, 'highlight button null');

    highlightEmitted = false;
    document.addEventListener(
        ToolbarEvent.HIGHLIGHT_CHANGE, () => highlightEmitted = true);
  });

  suite('by default', () => {
    test('highlighting is on', () => {
      assertEquals('read-anything:highlight-on', highlightButton.ironIcon);
      assertStringContains(highlightButton.title, 'off');
      assertEquals(0, chrome.readingMode.highlightGranularity);
      assertTrue(chrome.readingMode.isHighlightOn());
      assertFalse(highlightEmitted);
    });
  });

  suite('on first click', () => {
    setup(() => {
      highlightButton.click();
    });

    test('highlighting is turned off', () => {
      assertEquals('read-anything:highlight-off', highlightButton.ironIcon);
      assertStringContains(highlightButton.title, 'on');
      assertEquals(
          chrome.readingMode.noHighlighting,
          chrome.readingMode.highlightGranularity);
      assertFalse(chrome.readingMode.isHighlightOn());
      assertTrue(highlightEmitted);
    });

    suite('on next click', () => {
      setup(() => {
        highlightButton.click();
      });

      test('highlighting is turned back on', () => {
        assertEquals('read-anything:highlight-on', highlightButton.ironIcon);
        assertStringContains(highlightButton.title, 'off');
        assertEquals(0, chrome.readingMode.highlightGranularity);
        assertTrue(chrome.readingMode.isHighlightOn());
        assertTrue(highlightEmitted);
      });
    });
  });
});
