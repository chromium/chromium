// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {PLAY_PAUSE_EVENT} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('PlayPause', () => {
  let toolbar: ReadAnythingToolbarElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let playPauseButton: CrIconButtonElement;
  let granularityContainer: HTMLElement;
  let clickEmitted: boolean;

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

    playPauseButton =
        toolbar.shadowRoot!.querySelector<CrIconButtonElement>('#play-pause')!;
    granularityContainer = toolbar.shadowRoot!.querySelector<HTMLElement>(
        '#granularity-container')!;
    clickEmitted = false;
    document.addEventListener(PLAY_PAUSE_EVENT, () => clickEmitted = true);
  });

  function toolbarPaused(paused: boolean) {
    // Bypass Typescript compiler to allow us to get a private property
    // @ts-ignore
    toolbar.paused = paused;
  }

  suite('on click', () => {
    test('emits play event', () => {
      playPauseButton.click();
      assertTrue(clickEmitted);

      clickEmitted = false;
      playPauseButton.click();
      assertTrue(clickEmitted);
    });
  });

  suite('when playing', () => {
    setup(() => {
      toolbarPaused(false);
    });

    test('button indicates speech is playing', () => {
      assertEquals(playPauseButton.ironIcon, 'read-anything-20:pause');
      assertStringContains(playPauseButton.title.toLowerCase(), 'pause');
      assertStringContains(playPauseButton.ariaLabel!.toLowerCase(), 'pause');
    });

    test('granularity menu buttons show', () => {
      assertFalse(granularityContainer.hidden);
    });
  });

  suite('when paused', () => {
    setup(() => {
      toolbarPaused(true);
    });

    test('button indicates speech is paused', () => {
      assertEquals(playPauseButton.ironIcon, 'read-anything-20:play');
      assertStringContains(playPauseButton.title.toLowerCase(), 'play');
      assertStringContains(playPauseButton.ariaLabel!.toLowerCase(), 'play');
    });

    test('granularity menu buttons hidden', () => {
      assertTrue(granularityContainer.hidden);
    });
  });
});
