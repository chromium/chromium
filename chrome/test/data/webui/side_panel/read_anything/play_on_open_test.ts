// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement, SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {setupAppTestEnvironment, setupBasicSpeech} from './common.js';
import type {TestContentBrowserProxy} from './test_content_browser_proxy.js';
import type {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('PlayOnOpen', () => {
  let app: AppElement;
  let speech: TestSpeechBrowserProxy;
  let speechController: SpeechController;
  let contentBrowserProxy: TestContentBrowserProxy;

  function setPlayableContent() {
    contentBrowserProxy.rootId = 1;
    contentBrowserProxy.childrenMap = {1: [2]};
    contentBrowserProxy.textContentMap = {2: 'Hello world'};
    contentBrowserProxy.htmlTagMap = {1: 'p'};
  }

  function setEmptyContent() {
    contentBrowserProxy.rootId = 1;
    contentBrowserProxy.childrenMap = {1: []};
    contentBrowserProxy.textContentMap = {};
    contentBrowserProxy.htmlTagMap = {};
  }

  setup(async () => {
    const result =
        await setupAppTestEnvironment({readAnythingImprovedUiEnabled: true});
    app = result.app;
    contentBrowserProxy = result.contentBrowserProxy;
    speech = result.speech;
    speechController = result.speechController;
    setupBasicSpeech(speech);
  });

  test(
      'setPlayOnOpen triggers speech when playable and resets property',
      async () => {
        let playPauseToggled = false;
        speechController.onPlayPauseToggle = () => {
          playPauseToggled = true;
        };

        contentBrowserProxy.requiresDistillationVal = false;

        // Populate content and call updateContent() so
        // computeIsReadAloudPlayable() becomes true
        setPlayableContent();
        app.updateContent();
        await microtasksFinished();

        app.setPlayOnOpen(true);
        await microtasksFinished();

        assertTrue(
            playPauseToggled, 'onPlayPauseToggle should have been called');

        // Verify playOnOpen was reset to false by updating content again and
        // ensuring onPlayPauseToggle is not called a second time.
        playPauseToggled = false;
        app.updateContent();
        await microtasksFinished();
        assertFalse(
            playPauseToggled,
            'onPlayPauseToggle should not be called again after reset');
      });

  test(
      'not calling setPlayOnOpen does not trigger speech when playable',
      async () => {
        let playPauseToggled = false;
        speechController.onPlayPauseToggle = () => {
          playPauseToggled = true;
        };

        contentBrowserProxy.requiresDistillationVal = false;

        setPlayableContent();
        app.updateContent();
        await microtasksFinished();

        assertFalse(playPauseToggled, 'onPlayPauseToggle should not be called');
      });

  test(
      'setPlayOnOpen does not trigger speech until content is ready',
      async () => {
        let playPauseToggled = false;
        speechController.onPlayPauseToggle = () => {
          playPauseToggled = true;
        };

        contentBrowserProxy.requiresDistillationVal = false;

        // Pass an empty tree with no text nodes so computeIsReadAloudPlayable()
        // is false
        setEmptyContent();
        app.updateContent();
        app.setPlayOnOpen(true);
        await microtasksFinished();

        assertFalse(
            playPauseToggled,
            'Should not toggle playback before content is ready');

        // Now provide playable content and verify speech triggers when ready
        setPlayableContent();
        app.updateContent();
        await microtasksFinished();

        assertTrue(
            playPauseToggled,
            'onPlayPauseToggle should be called once content is ready');
      });
});
