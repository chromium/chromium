// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/app.js';
import {assertEquals, assertFalse} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('PlayPause', () => {
  let app: ReadAnythingElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let playPauseButton: CrIconButtonElement;

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

  function assertAppPausedState(shouldBePaused: boolean): void {
    // Bypass Typescript compiler to allow us to get a private property
    // @ts-ignore
    assertEquals(app.paused, shouldBePaused);
  }

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
    app.updateContent();
    playPauseButton =
        app.$.toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
            '#play-pause')!;
  });

  suite('by default', () => {
    test('is paused', () => {
      assertAppPausedState(true);
      assertFalse(app.speechStarted);
    });

    test('shows play icon', () => {
      assertEquals(playPauseButton.ironIcon, 'read-anything-20:play');
    });
  });

  suite('on first click', () => {
    setup(() => {
      playPauseButton.click();
    });

    test('starts speech', () => {
      assertAppPausedState(false);
      // TODO: b/323960128 - Since this test browser doesn't have any
      // voices, speechStarted doesn't get set to true. Find a way to add a mock
      // voice to this browser, and test that app.speechStarted is true.
    });

    test('updates icon to pause', () => {
      assertEquals(playPauseButton.ironIcon, 'read-anything-20:pause');
    });
  });

  suite('on second click', () => {
    setup(() => {
      playPauseButton.click();
      playPauseButton.click();
    });

    test('stops speech', () => {
      assertAppPausedState(true);
    });

    test('updates icon to play', () => {
      assertEquals(playPauseButton.ironIcon, 'read-anything-20:play');
    });
  });

  suite('on keyboard k pressed', () => {
    let kPress: KeyboardEvent;

    setup(() => {
      kPress = new KeyboardEvent('keydown', {key: 'k'});
    });

    test('first press plays', () => {
      app.$.flexParent!.dispatchEvent(kPress);
      assertAppPausedState(false);
    });

    test('second press pauses', () => {
      app.$.flexParent!.dispatchEvent(kPress);
      app.$.flexParent!.dispatchEvent(kPress);
      assertAppPausedState(true);
    });
  });
});
