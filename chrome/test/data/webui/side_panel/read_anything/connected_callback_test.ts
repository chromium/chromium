// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertStringContains} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createApp} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('ConnectedCallback', () => {
  let app: AppElement;
  const speechController = new class extends SpeechController {
    scrollCount: number = 0;

    override onScroll() {
      this.scrollCount++;
    }
  }
  ();


  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    SpeechController.setInstance(speechController);

    app = await createApp();
    app.connectedCallback();
  });

  test('shows loading page', () => {
    assertEquals(
        app.shadowRoot.querySelector<HTMLElement>(
                          '#empty-state-container')!.hidden,
        false);
    const emptyState = app.shadowRoot.querySelector('sp-empty-state')!;
    assertEquals('Getting ready', emptyState.heading);
    assertEquals('', emptyState.body);
    assertStringContains(emptyState.imagePath, 'throbber');
    assertStringContains(emptyState.darkImagePath, 'throbber');
  });

  test('scroll listener added', () => {
    assertEquals(0, speechController.scrollCount);

    // Scroll events are received by containerScroller.
    app.$.containerScroller.dispatchEvent(new Event('scroll'));
    assertEquals(1, speechController.scrollCount);

    app.$.containerScroller.dispatchEvent(new Event('scroll'));
    assertEquals(2, speechController.scrollCount);
  });
});
