// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, PauseActionSource, SpeechModel} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

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

  test('reset', () => {
    speechModel.setState({
      isSpeechActive: true,
      isSpeechTreeInitialized: true,
      pauseSource: PauseActionSource.BUTTON_CLICK,
      isAudioCurrentlyPlaying: true,
      hasSpeechBeenTriggered: true,
      isSpeechBeingRepositioned: true,
    });

    speechModel.reset();

    assertFalse(speechModel.isSpeechActive());
    assertFalse(speechModel.isSpeechTreeInitialized());
    assertEquals(PauseActionSource.DEFAULT, speechModel.getPauseSource());
    assertFalse(speechModel.isAudioCurrentlyPlaying());
    assertFalse(speechModel.hasSpeechBeenTriggered());
    assertFalse(speechModel.isSpeechBeingRepositioned());
  });

  test('setState makes a copy', () => {
    const pauseSource = PauseActionSource.ENGINE_INTERRUPT;
    const state = {
      isSpeechActive: true,
      isSpeechTreeInitialized: true,
      pauseSource: pauseSource,
      isAudioCurrentlyPlaying: true,
      hasSpeechBeenTriggered: true,
      isSpeechBeingRepositioned: true,
    };

    speechModel.setState(state);

    assertNotEquals(state, speechModel.getState());
    assertTrue(speechModel.isSpeechActive());
    assertTrue(speechModel.isSpeechTreeInitialized());
    assertEquals(pauseSource, speechModel.getPauseSource());
    assertTrue(speechModel.isAudioCurrentlyPlaying());
    assertTrue(speechModel.hasSpeechBeenTriggered());
    assertTrue(speechModel.isSpeechBeingRepositioned());
  });

  test('setIsSpeechTreeInitialized', () => {
    speechModel.setIsSpeechTreeInitialized(true);
    assertTrue(speechModel.isSpeechTreeInitialized());
    speechModel.setIsSpeechTreeInitialized(false);
    assertFalse(speechModel.isSpeechTreeInitialized());
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
});
