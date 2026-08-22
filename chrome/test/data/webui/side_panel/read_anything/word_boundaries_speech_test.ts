// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement, WordBoundaryState} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {AudioBrowserProxyImpl, ContentBrowserProxyImpl, ContentController, ContentType, setInstance, SpeechBrowserProxyImpl, SpeechController, ToolbarEvent, VisualBrowserProxyImpl, VoiceLanguageController, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createApp, createSpeechSynthesisVoice, emitEvent, setContent, setupBasicSpeech} from './common.js';
import {TestAudioBrowserProxy} from './test_audio_browser_proxy.js';
import {TestContentBrowserProxy} from './test_content_browser_proxy.js';
import {TestReadAloudModelBrowserProxy} from './test_read_aloud_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';
import {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('WordBoundariesUsedForSpeech', () => {
  let app: AppElement;
  let wordBoundaries: WordBoundaries;
  let speechController: SpeechController;
  let voiceLanguageController: VoiceLanguageController;
  let readAloudModel: TestReadAloudModelBrowserProxy;

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    ContentBrowserProxyImpl.setInstance(new TestContentBrowserProxy());
    AudioBrowserProxyImpl.setInstance(new TestAudioBrowserProxy());
    VisualBrowserProxyImpl.setInstance(new TestVisualBrowserProxy());

    readAloudModel = new TestReadAloudModelBrowserProxy();
    setInstance(readAloudModel);
    readAloudModel.setInitialized(true);

    const speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    voiceLanguageController = new VoiceLanguageController();
    VoiceLanguageController.setInstance(voiceLanguageController);
    wordBoundaries = new WordBoundaries();
    WordBoundaries.setInstance(wordBoundaries);
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
    const contentController = new ContentController();
    ContentController.setInstance(contentController);

    app = await createApp();
    setupBasicSpeech(speech);

    const node =
        setContent('This is a link. This is another link.', readAloudModel);
    app.$.container.appendChild(node);
    contentController.setState(ContentType.HAS_CONTENT);
  });

  test(
      'during speech with no boundaries wordBoundaryState in default state',
      () => {
        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
        const state: WordBoundaryState = wordBoundaries.state;
        assertTrue(wordBoundaries.notSupported());
        assertEquals(0, state.previouslySpokenIndex);
        assertEquals(0, state.speechUtteranceStartIndex);
        assertEquals(0, state.speechUtteranceLength);
      });

  suite('during speech with one initial word boundary ', () => {
    setup(() => {
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      wordBoundaries.updateBoundary(10, 5);
    });

    test('wordBoundaryState uses most recent boundary', () => {
      const state: WordBoundaryState = wordBoundaries.state;
      assertTrue(wordBoundaries.hasBoundaries());
      assertEquals(10, state.previouslySpokenIndex);
      assertEquals(0, state.speechUtteranceStartIndex);
      assertEquals(5, state.speechUtteranceLength);
    });

    test('pause / play toggle maintains word boundary state', () => {
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);

      const state: WordBoundaryState = wordBoundaries.state;
      assertTrue(wordBoundaries.hasBoundaries());
      assertTrue(!!voiceLanguageController.getCurrentVoice());
      assertEquals(10, state.speechUtteranceStartIndex);
      assertEquals(0, state.previouslySpokenIndex);
      assertEquals(5, state.speechUtteranceLength);
    });

    test('word boundaries update after pause / play toggle', () => {
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      wordBoundaries.updateBoundary(3, 9);

      const state: WordBoundaryState = wordBoundaries.state;
      assertTrue(wordBoundaries.hasBoundaries());
      assertEquals(3, state.previouslySpokenIndex);
      assertEquals(10, state.speechUtteranceStartIndex);
      assertEquals(9, state.speechUtteranceLength);
    });

    test(
        'word boundaries correct after multiple pause / play toggles',
        async () => {
          emitEvent(app, ToolbarEvent.PLAY_PAUSE);
          await microtasksFinished();
          emitEvent(app, ToolbarEvent.PLAY_PAUSE);
          await microtasksFinished();
          wordBoundaries.updateBoundary(3, 9);
          emitEvent(app, ToolbarEvent.PLAY_PAUSE);
          await microtasksFinished();
          emitEvent(app, ToolbarEvent.PLAY_PAUSE);
          await microtasksFinished();
          wordBoundaries.updateBoundary(7);
          emitEvent(app, ToolbarEvent.PLAY_PAUSE);
          await microtasksFinished();
          emitEvent(app, ToolbarEvent.PLAY_PAUSE);
          await microtasksFinished();
          wordBoundaries.updateBoundary(1, 15);

          const state: WordBoundaryState = wordBoundaries.state;
          assertTrue(wordBoundaries.hasBoundaries());
          assertEquals(1, state.previouslySpokenIndex);
          assertEquals(20, state.speechUtteranceStartIndex);
          assertEquals(15, state.speechUtteranceLength);
        });
  });

  test(
      'with multiple word boundaries wordBoundaryState in default state during speech',
      () => {
        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
        wordBoundaries.updateBoundary(10);
        wordBoundaries.updateBoundary(15, 5);
        wordBoundaries.updateBoundary(25, 10);
        wordBoundaries.updateBoundary(40);

        const state: WordBoundaryState = wordBoundaries.state;
        assertTrue(wordBoundaries.hasBoundaries());
        assertEquals(40, state.previouslySpokenIndex);
        assertEquals(0, state.speechUtteranceLength);
        assertEquals(0, state.speechUtteranceStartIndex);
      });

  test(
      'after voice change resets to unsupported boundary mode but keeps boundary indices',
      () => {
        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
        wordBoundaries.updateBoundary(10);
        assertTrue(wordBoundaries.hasBoundaries());

        const selectedVoice =
            createSpeechSynthesisVoice({lang: 'es', name: 'Lauren'});
        emitEvent(app, ToolbarEvent.VOICE, {detail: {selectedVoice}});

        // When the voice changes, we don't know yet whether word boundaries
        // are supported, but we send to the engine the sentence starting from
        // the word we were reading, so previously spoken index should reset,
        // and we should be starting in the middle of the utterance.
        const state: WordBoundaryState = wordBoundaries.state;
        assertTrue(wordBoundaries.notSupported());
        assertEquals(0, state.previouslySpokenIndex);
        assertEquals(0, state.speechUtteranceLength);
        assertEquals(10, state.speechUtteranceStartIndex);

        // After another boundary event, the boundary mode is set to
        // BOUNDARY_DETECTED again, and we know the new word boundaries relative
        // to the substring we had sent to the engine.
        wordBoundaries.updateBoundary(0, 4);
        assertTrue(wordBoundaries.hasBoundaries());
        assertEquals(0, wordBoundaries.state.previouslySpokenIndex);
        assertEquals(4, wordBoundaries.state.speechUtteranceLength);
        assertEquals(10, wordBoundaries.state.speechUtteranceStartIndex);
        wordBoundaries.updateBoundary(5, 12);
        assertTrue(wordBoundaries.hasBoundaries());
        assertEquals(5, wordBoundaries.state.previouslySpokenIndex);
        assertEquals(12, wordBoundaries.state.speechUtteranceLength);
        assertEquals(10, wordBoundaries.state.speechUtteranceStartIndex);
      });

  test(
      'after voice change to same language does not reset word boundary mode',
      () => {
        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
        wordBoundaries.updateBoundary(10);
        assertTrue(wordBoundaries.hasBoundaries());

        const selectedVoice =
            createSpeechSynthesisVoice({lang: 'en', name: 'Lauren'});
        emitEvent(app, ToolbarEvent.VOICE, {detail: {selectedVoice}});

        // After a voice change to the same language, the word boundary state
        // has stayed the same.
        const state: WordBoundaryState = wordBoundaries.state;
        assertTrue(wordBoundaries.hasBoundaries());
        assertEquals(0, state.previouslySpokenIndex);
        assertEquals(10, state.speechUtteranceStartIndex);
      });
});
