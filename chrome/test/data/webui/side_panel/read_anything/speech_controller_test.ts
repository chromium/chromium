// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {BrowserProxy, PauseActionSource, SpeechBrowserProxyImpl, SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('SpeechController', () => {
  let speech: TestSpeechBrowserProxy;
  let speechController: SpeechController;
  let onPause: boolean;
  let isSpeechActiveChanged: boolean;
  let isAudioCurrentlyPlayingChanged: boolean;
  let metrics: TestMetricsBrowserProxy;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    speech = new TestSpeechBrowserProxy();
    metrics = mockMetrics();
    SpeechBrowserProxyImpl.setInstance(speech);
    isSpeechActiveChanged = false;
    isAudioCurrentlyPlayingChanged = false;
    onPause = false;
    const speechListener = {
      onPause() {
        onPause = true;
      },

      onIsSpeechActiveChange() {
        isSpeechActiveChanged = true;
      },

      onIsAudioCurrentlyPlayingChange() {
        isAudioCurrentlyPlayingChanged = true;
      },
    };

    speechController = new SpeechController();
    speechController.addListener(speechListener);
  });

  test('setState', () => {
    const pauseSource = PauseActionSource.ENGINE_INTERRUPT;
    const state = {
      isSpeechActive: true,
      isSpeechTreeInitialized: true,
      pauseSource: pauseSource,
      isAudioCurrentlyPlaying: true,
      hasSpeechBeenTriggered: true,
      isSpeechBeingRepositioned: true,
    };

    speechController.setState(state);

    assertTrue(isSpeechActiveChanged);
    assertTrue(isAudioCurrentlyPlayingChanged);
    assertFalse(onPause);
    assertNotEquals(state, speechController.getState());
    assertTrue(speechController.isSpeechActive());
    assertTrue(speechController.isSpeechTreeInitialized());
    assertEquals(pauseSource, speechController.getPauseSource());
    assertTrue(speechController.isAudioCurrentlyPlaying());
    assertTrue(speechController.hasSpeechBeenTriggered());
    assertTrue(speechController.isSpeechBeingRepositioned());
  });

  test('reset', () => {
    const pauseSource = PauseActionSource.ENGINE_INTERRUPT;
    const state = {
      isSpeechActive: true,
      isSpeechTreeInitialized: true,
      pauseSource: pauseSource,
      isAudioCurrentlyPlaying: true,
      hasSpeechBeenTriggered: true,
      isSpeechBeingRepositioned: true,
    };
    speechController.setState(state);

    speechController.reset();

    assertTrue(isSpeechActiveChanged);
    assertTrue(isAudioCurrentlyPlayingChanged);
    assertFalse(onPause);
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isSpeechTreeInitialized());
    assertEquals(PauseActionSource.DEFAULT, speechController.getPauseSource());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertFalse(speechController.hasSpeechBeenTriggered());
    assertFalse(speechController.isSpeechBeingRepositioned());
  });

  test('isPausedFromButton', () => {
    const pauseSource1 = PauseActionSource.ENGINE_INTERRUPT;
    const pauseSource2 = PauseActionSource.DEFAULT;
    const pauseSource3 = PauseActionSource.BUTTON_CLICK;

    speechController.stopSpeech(pauseSource1);
    assertFalse(speechController.isPausedFromButton());

    speechController.stopSpeech(pauseSource2);
    assertFalse(speechController.isPausedFromButton());

    speechController.stopSpeech(pauseSource3);
    assertTrue(speechController.isPausedFromButton());
  });

  test('setIsSpeechActive notifies listeners if value changes', () => {
    let sentIsSpeechActive = false;
    chrome.readingMode.onSpeechPlayingStateChanged = () => {
      sentIsSpeechActive = true;
    };

    speechController.setIsSpeechActive(false);

    assertFalse(isSpeechActiveChanged);
    assertFalse(sentIsSpeechActive);
    assertFalse(speechController.isSpeechActive());
    assertFalse(isAudioCurrentlyPlayingChanged);

    speechController.setIsSpeechActive(true);

    assertTrue(isSpeechActiveChanged);
    assertTrue(sentIsSpeechActive);
    assertTrue(speechController.isSpeechActive());
    assertFalse(isAudioCurrentlyPlayingChanged);
  });

  test('setIsAudioCurrentlyPlaying notifies listeners if value changes', () => {
    speechController.setIsAudioCurrentlyPlaying(false);

    assertFalse(isSpeechActiveChanged);
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertFalse(isAudioCurrentlyPlayingChanged);

    speechController.setIsAudioCurrentlyPlaying(true);

    assertFalse(isSpeechActiveChanged);
    assertTrue(speechController.isAudioCurrentlyPlaying());
    assertTrue(isAudioCurrentlyPlayingChanged);
  });

  suite('initializeSpeechTree', () => {
    let initAxPositionWithNode: number;
    let startedPreprocess: boolean = false;

    setup(() => {
      chrome.readingMode.initAxPositionWithNode = (nodeId) => {
        initAxPositionWithNode = nodeId;
      };
      chrome.readingMode.preprocessTextForSpeech = () => {
        startedPreprocess = true;
      };
    });

    test('with no node id does nothing', () => {
      speechController.initializeSpeechTree(null);

      assertFalse(!!initAxPositionWithNode);
      assertFalse(startedPreprocess);
      assertFalse(speechController.isSpeechTreeInitialized());
    });

    test('when already initialized does nothing', () => {
      const id1 = 10;
      const id2 = 12;
      speechController.initializeSpeechTree(id1);
      startedPreprocess = false;

      speechController.initializeSpeechTree(id2);

      assertEquals(id1, initAxPositionWithNode);
      assertFalse(startedPreprocess);
    });

    test('initializes speech tree', () => {
      const id = 14;
      speechController.initializeSpeechTree(id);

      assertEquals(id, initAxPositionWithNode);
      assertTrue(startedPreprocess);
      assertTrue(speechController.isSpeechTreeInitialized());
    });
  });

  test('stopSpeech with button click pauses', () => {
    const source = PauseActionSource.BUTTON_CLICK;
    speechController.setIsSpeechActive(true);
    speechController.setIsAudioCurrentlyPlaying(true);

    speechController.stopSpeech(source);

    assertTrue(onPause);
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertEquals(source, speechController.getPauseSource());
    assertEquals(1, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('cancel'));
  });

  test('stopSpeech without button click cancels', () => {
    const source = PauseActionSource.VOICE_SETTINGS_CHANGE;
    speechController.setIsSpeechActive(true);
    speechController.setIsAudioCurrentlyPlaying(true);

    speechController.stopSpeech(source);

    assertTrue(onPause);
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertEquals(source, speechController.getPauseSource());
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(1, speech.getCallCount('cancel'));
  });

  test('onSpeechInterrupted while repositioning keeps playing speech', () => {
    speechController.setIsSpeechBeingRepositioned(true);
    speechController.setIsSpeechActive(true);
    speechController.setIsAudioCurrentlyPlaying(true);

    speechController.onSpeechInterrupted();

    assertFalse(onPause);
    assertTrue(speechController.isAudioCurrentlyPlaying());
    assertTrue(speechController.isSpeechActive());
    assertTrue(speechController.isSpeechBeingRepositioned());
  });

  test('onSpeechInterrupted stops speech', async () => {
    speechController.initializeSpeechTree(1);
    speechController.setIsAudioCurrentlyPlaying(true);

    speechController.onSpeechInterrupted();

    assertTrue(onPause);
    assertEquals(
        PauseActionSource.ENGINE_INTERRUPT, speechController.getPauseSource());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isSpeechBeingRepositioned());
    assertEquals(
        chrome.readingMode.engineInterruptStopSource,
        await metrics.whenCalled('recordSpeechStopSource'));
  });
});
