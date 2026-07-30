// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {SpeechBrowserProxyImpl, SpeechController, VoiceLanguageController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createApp, setupBasicSpeech} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('PlayOnOpen', () => {
  let app: AppElement;
  let readingMode: FakeReadingMode;
  let speech: TestSpeechBrowserProxy;
  let speechController: SpeechController;

  const axTree = {
    rootId: 1,
    nodes: [
      {id: 1, role: 'rootWebArea', htmlTag: '#document', childIds: [2]},
      {id: 2, role: 'paragraph', htmlTag: 'p', childIds: [3]},
      {id: 3, role: 'staticText', name: 'Hello world'},
    ],
  };

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    readingMode = new FakeReadingMode();
    readingMode.isReadAnythingImprovedUiEnabled = true;
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    speechController = SpeechController.getInstance();

    VoiceLanguageController.setInstance(new VoiceLanguageController());
    app = await createApp();
    setupBasicSpeech(speech);
  });

  test(
      'setPlayOnOpen triggers speech when playable and resets property',
      async () => {
        let playPauseToggled = false;
        speechController.onPlayPauseToggle = () => {
          playPauseToggled = true;
        };

        readingMode.requiresDistillation = false;

        // Populate content and call updateContent() so
        // computeIsReadAloudPlayable() becomes true
        chrome.readingMode.setContentForTesting(axTree, [3]);
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

        readingMode.requiresDistillation = false;

        chrome.readingMode.setContentForTesting(axTree, [3]);
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

        readingMode.requiresDistillation = false;
        readingMode.getChildren = (_nodeId: number) => {
          return [];
        };
        readingMode.getTextContent = (_nodeId: number) => {
          return '';
        };

        // Pass an empty tree with no text nodes so computeIsReadAloudPlayable()
        // is false
        const emptyAxTree = {
          rootId: 1,
          nodes: [
            {id: 1, role: 'rootWebArea', htmlTag: '#document', childIds: []},
          ],
        };
        chrome.readingMode.setContentForTesting(emptyAxTree, []);
        app.updateContent();
        app.setPlayOnOpen(true);
        await microtasksFinished();

        assertFalse(
            playPauseToggled,
            'Should not toggle playback before content is ready');

        // Now provide playable content and verify speech triggers when ready
        readingMode.getChildren = (nodeId: number) => {
          return nodeId === 1 ? [2, 3] : [];
        };
        readingMode.getTextContent = (_nodeId: number) => {
          return 'Hello world';
        };
        chrome.readingMode.setContentForTesting(axTree, [3]);
        app.updateContent();
        await microtasksFinished();

        assertTrue(
            playPauseToggled,
            'onPlayPauseToggle should be called once content is ready');
      });
});
