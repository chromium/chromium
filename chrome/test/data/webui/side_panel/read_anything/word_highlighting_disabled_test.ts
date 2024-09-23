// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy, WordBoundaryMode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement, WordBoundaryState} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {setSimpleAxTreeWithText, suppressInnocuousErrors} from './common.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('WordHighlightingDisabled', () => {
  test('with flag disabled sentence highlight used', () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testBrowserProxy: TestColorUpdaterBrowserProxy =
        new TestColorUpdaterBrowserProxy();
    suppressInnocuousErrors();
    BrowserProxy.setInstance(testBrowserProxy);
    // Do not call the real `onConnected()`. As defined in
    // ReadAnythingAppController, onConnected creates mojo pipes to connect to
    // the rest of the Read Anything feature, which we are not testing here.
    chrome.readingMode.onConnected = () => {};

    const app: AppElement = document.createElement('read-anything-app');
    document.body.appendChild(app);
    flush();

    const sentence = 'It\'s time to try defying gravity.';
    setSimpleAxTreeWithText(sentence);

    app.playSpeech();
    app.updateBoundary(10);

    // Since the word highlighting flag is disabled, the sentence should be
    // highlighted not the word.
    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(!!currentHighlight);
    assertEquals(sentence, currentHighlight.textContent);

    // Even though sentence highlighting is used, the word boundaries should
    // still be update for resuming speech on the word boundary.
    const state: WordBoundaryState = app.wordBoundaryState;
    assertEquals(WordBoundaryMode.BOUNDARY_DETECTED, state.mode);
    assertEquals(10, state.previouslySpokenIndex);
    assertEquals(0, state.speechUtteranceStartIndex);
  });
});
