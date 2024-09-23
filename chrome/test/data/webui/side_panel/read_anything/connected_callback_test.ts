// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertStringContains} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('ConnectedCallback', () => {
  let app: AppElement;

  setup(() => {
    suppressInnocuousErrors();
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);

    app.connectedCallback();
  });

  test('shows loading page', () => {
    assertEquals(
        app.querySelector<HTMLElement>('#empty-state-container')!.hidden,
        false);
    const emptyState = app.querySelector('sp-empty-state')!;
    assertEquals('Getting ready', emptyState.heading);
    assertEquals('', emptyState.body);
    assertStringContains(emptyState.imagePath, 'throbber');
    assertStringContains(emptyState.darkImagePath, 'throbber');
  });
});
