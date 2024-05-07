// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

// TODO: crbug.com/1474951 - Remove this test once WebUI flag is removed.
suite('WebUiToolbarFlag', () => {
  let app: ReadAnythingElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
  });

  function createApp(): void {
    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
  }

  test('webUI toolbar is visible if enabled', () => {
    chrome.readingMode.isWebUIToolbarVisible = true;
    createApp();

    const container = app.querySelector<HTMLElement>('#toolbar-container');

    assertTrue(!!container);
    assertFalse(container.hidden);
  });

  test('webUI toolbar is invisible if disabled', () => {
    chrome.readingMode.isWebUIToolbarVisible = false;
    createApp();

    const container = app.querySelector<HTMLElement>('#toolbar-container');

    assertTrue(!!container);
    assertTrue(container.hidden);
  });
});
