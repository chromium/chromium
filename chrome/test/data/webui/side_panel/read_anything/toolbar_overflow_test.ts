// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {moreOptionsClass} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

// TODO: b/40275871 - Add more tests.
suite('ToolbarOverflow', () => {
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let toolbar: ReadAnythingToolbarElement;

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
    flush();
  });

  suite('on reset toolbar', () => {
    setup(() => {
      // Open the menu first so we can be sure the event is what closes it.
      toolbar.$.moreOptionsMenu.get().showAt(toolbar);
      assertTrue(toolbar.$.moreOptionsMenu.get().open);

      toolbar.$.toolbarContainer.dispatchEvent(
          new CustomEvent('reset-toolbar'));
      flush();
    });

    test('more options closed', () => {
      assertFalse(toolbar.$.moreOptionsMenu.get().open);
    });

    test('more options empty', () => {
      const moreOptionsButtons =
          toolbar.$.moreOptionsMenu.get().querySelectorAll<HTMLElement>(
              moreOptionsClass);
      assertEquals(0, moreOptionsButtons.length);
    });
  });

  suite('on toolbar overflow', () => {
    function overflow(numOverflowButtons: number) {
      toolbar.$.toolbarContainer.dispatchEvent(
          new CustomEvent('toolbar-overflow', {
            bubbles: true,
            composed: true,
            detail: {numOverflowButtons},
          }));
      toolbar.$.moreOptionsMenu.get();
      flush();
    }

    test('more options contains overflow', () => {
      let numOverflow = 3;
      overflow(numOverflow);
      assertEquals(
          numOverflow,
          toolbar.$.moreOptionsMenu.get()
              .querySelectorAll<HTMLElement>(moreOptionsClass)
              .length);

      numOverflow = 5;
      overflow(numOverflow);
      assertEquals(
          numOverflow,
          toolbar.$.moreOptionsMenu.get()
              .querySelectorAll<HTMLElement>(moreOptionsClass)
              .length);
    });
  });
});
