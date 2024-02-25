// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/app.js';
import {defaultFontName} from 'chrome-untrusted://read-anything-side-panel.top-chrome/common.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('AppFontChange', () => {
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let app: ReadAnythingElement;

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
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
        console.info('Suppressed ResizeObserver error: ', message);
        return;
      }
      if (onerror) {
        onerror.apply(window, [message, url, lineNumber, column, error]);
      }
    };
  }

  function containerFont(): string {
    return window.getComputedStyle(app.$.container)
        .getPropertyValue('font-family');
  }

  function assertFontsEqual(actual: string, expected: string): void {
    assertEquals(
        actual.trim().toLowerCase().replaceAll('"', ''),
        expected.trim().toLowerCase().replaceAll('"', ''));
  }

  test('valid font updates container font', () => {
    const font1 = 'Andika';
    app.updateFont(font1);
    assertFontsEqual(containerFont(), font1);

    const font2 = 'Comic Neue';
    app.updateFont(font2);
    assertFontsEqual(containerFont(), font2);
  });

  test('invalid font uses default', () => {
    const font1 = 'not a real font';
    app.updateFont(font1);
    assertFontsEqual(containerFont(), defaultFontName);

    const font2 = 'FakeFont';
    app.updateFont(font2);
    assertFontsEqual(containerFont(), defaultFontName);
  });

  test('unsupported font uses default', () => {
    const font1 = 'Comic Sans';
    app.updateFont(font1);
    assertFontsEqual(containerFont(), defaultFontName);

    const font2 = 'Times New Roman';
    app.updateFont(font2);
    assertFontsEqual(containerFont(), defaultFontName);
  });
});
