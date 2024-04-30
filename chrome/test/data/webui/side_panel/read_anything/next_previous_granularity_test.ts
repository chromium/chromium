// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NEXT_GRANULARITY_EVENT, PREVIOUS_GRANULARITY_EVENT} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('NextPrevious', () => {
  let toolbar: ReadAnythingToolbarElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let nextButton: CrIconButtonElement;
  let previousButton: CrIconButtonElement;
  let nextEmitted: boolean;
  let previousEmitted: boolean;

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

    nextButton = toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
        '#nextGranularity')!;
    previousButton = toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
        '#previousGranularity')!;
    nextEmitted = false;
    previousEmitted = false;
    document.addEventListener(NEXT_GRANULARITY_EVENT, () => nextEmitted = true);
    document.addEventListener(
        PREVIOUS_GRANULARITY_EVENT, () => previousEmitted = true);
  });

  suite('on next click', () => {
    test('emits next event', () => {
      nextButton.click();
      assertTrue(nextEmitted);
      assertFalse(previousEmitted);
    });
  });

  suite('on previous click', () => {
    test('emits previous event', () => {
      previousButton.click();
      assertTrue(previousEmitted);
      assertFalse(nextEmitted);
    });
  });
});
