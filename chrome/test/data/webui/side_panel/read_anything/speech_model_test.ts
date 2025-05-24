// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, PauseActionSource, SpeechEngineState, SpeechModel} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('SpeechModel', () => {
  let speechModel: SpeechModel;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    speechModel = new SpeechModel();
  });

  test('setState makes a copy', () => {
    const pauseSource = PauseActionSource.ENGINE_INTERRUPT;
    const state = {
      isSpeechActive: true,
      pauseSource: pauseSource,
      isAudioCurrentlyPlaying: true,
      hasSpeechBeenTriggered: true,
      isSpeechBeingRepositioned: true,
    };

    speechModel.setState(state);

    assertNotEquals(state, speechModel.getState());
    assertTrue(speechModel.isSpeechActive());
    assertEquals(pauseSource, speechModel.getPauseSource());
    assertTrue(speechModel.isAudioCurrentlyPlaying());
    assertTrue(speechModel.hasSpeechBeenTriggered());
    assertTrue(speechModel.isSpeechBeingRepositioned());
  });

  test('setIsSpeechActive', () => {
    speechModel.setIsSpeechActive(true);
    assertTrue(speechModel.isSpeechActive());
    speechModel.setIsSpeechActive(false);
    assertFalse(speechModel.isSpeechActive());
  });

  test('setIsAudioCurrentlyPlaying', () => {
    speechModel.setIsAudioCurrentlyPlaying(true);
    assertTrue(speechModel.isAudioCurrentlyPlaying());
    speechModel.setIsAudioCurrentlyPlaying(false);
    assertFalse(speechModel.isAudioCurrentlyPlaying());
  });

  test('setHasSpeechBeenTriggered', () => {
    speechModel.setHasSpeechBeenTriggered(true);
    assertTrue(speechModel.hasSpeechBeenTriggered());
    speechModel.setHasSpeechBeenTriggered(false);
    assertFalse(speechModel.hasSpeechBeenTriggered());
  });

  test('setIsSpeechBeingRepositioned', () => {
    speechModel.setIsSpeechBeingRepositioned(true);
    assertTrue(speechModel.isSpeechBeingRepositioned());
    speechModel.setIsSpeechBeingRepositioned(false);
    assertFalse(speechModel.isSpeechBeingRepositioned());
  });

  test('setPauseSource', () => {
    const source1 = PauseActionSource.BUTTON_CLICK;
    const source2 = PauseActionSource.VOICE_SETTINGS_CHANGE;
    const source3 = PauseActionSource.DEFAULT;

    speechModel.setPauseSource(source1);
    assertEquals(source1, speechModel.getPauseSource());
    speechModel.setPauseSource(source2);
    assertEquals(source2, speechModel.getPauseSource());
    speechModel.setPauseSource(source3);
    assertEquals(source3, speechModel.getPauseSource());
  });

  test('setPreviewVoicePlaying', () => {
    const voice1 = createSpeechSynthesisVoice({lang: 'en', name: 'April'});
    const voice2 = null;
    const voice3 = createSpeechSynthesisVoice({lang: 'fr', name: 'May'});

    speechModel.setPreviewVoicePlaying(voice1);
    assertEquals(voice1, speechModel.getPreviewVoicePlaying());
    speechModel.setPreviewVoicePlaying(voice2);
    assertEquals(voice2, speechModel.getPreviewVoicePlaying());
    speechModel.setPreviewVoicePlaying(voice3);
    assertEquals(voice3, speechModel.getPreviewVoicePlaying());
  });

  test('setEngineState', () => {
    const state1 = SpeechEngineState.LOADING;
    const state2 = SpeechEngineState.NONE;
    const state3 = SpeechEngineState.LOADED;

    speechModel.setEngineState(state1);
    assertEquals(state1, speechModel.getEngineState());
    speechModel.setEngineState(state2);
    assertEquals(state2, speechModel.getEngineState());
    speechModel.setEngineState(state3);
    assertEquals(state3, speechModel.getEngineState());
  });
});
