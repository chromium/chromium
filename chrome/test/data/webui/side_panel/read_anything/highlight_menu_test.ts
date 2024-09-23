// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {AppElement, ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {stubAnimationFrame, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('HighlightMenu', () => {
  let app: AppElement;
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
    chrome.readingMode.isPhraseHighlightingEnabled = true;
    chrome.readingMode.isAutomaticWordHighlightingEnabled = true;

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    flush();

    toolbar = app.$.toolbar;
    highlightButton =
        toolbar.$.toolbarContainer.querySelector<CrIconButtonElement>(
            '#highlight')!;
  });

  test('highlighting is on by default', () => {
    assertEquals('read-anything:highlight-on', highlightButton.ironIcon);
    assertStringContains(highlightButton.title, 'highlight');
    assertEquals(0, chrome.readingMode.highlightGranularity);
    assertTrue(chrome.readingMode.isHighlightOn());
  });

  test('click opens menu', () => {
    stubAnimationFrame();
    highlightButton.click();
    flush();

    const menu = toolbar.$.highlightMenu.$.menu.$.lazyMenu.get();
    assertTrue(menu.open);
  });

  suite('dropdown menu', () => {
    let options: HTMLButtonElement[];

    setup(() => {
      stubAnimationFrame();
      highlightButton.click();
      flush();
      const menu = toolbar.$.highlightMenu.$.menu.$.lazyMenu.get();
      assertTrue(menu.open);
      options = Array.from(
          menu.querySelectorAll<HTMLButtonElement>('.dropdown-item'));
    });

    test('has 5 items', () => {
      assertEquals(options.length, 5);
    });

    test('selects highlight granularity', () => {
      let index = 0;
      options.forEach(option => {
        option.click();
        flush();
        assertEquals(chrome.readingMode.highlightGranularity, index);
        index++;
      });
    });

    test('highlight off changes icon', () => {
      options[4]!.click();
      flush();
      assertEquals('read-anything:highlight-off', highlightButton.ironIcon);
    });
  });
});
