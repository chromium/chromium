// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {BrowserProxy, PauseActionSource, SpeechBrowserProxyImpl, SpeechController, SpeechEngineState} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice, mockMetrics} from './common.js';
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
  let onPreviewVoicePlaying: boolean;
  let onEngineStateChange: boolean;
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
    onPreviewVoicePlaying = false;
    onEngineStateChange = false;
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

      onEngineStateChange() {
        onEngineStateChange = true;
      },

      onPreviewVoicePlaying() {
        onPreviewVoicePlaying = true;
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

  test('setEngineState notifies listeners if value changes', () => {
    speechController.setEngineState(SpeechEngineState.NONE);

    assertFalse(onEngineStateChange);
    assertFalse(speechController.isEngineLoaded());

    speechController.setEngineState(SpeechEngineState.LOADED);

    assertTrue(onEngineStateChange);
    assertTrue(speechController.isEngineLoaded());
  });

  test('setPreviewVoicePlaying notifies listeners if value changes', () => {
    speechController.setPreviewVoicePlaying(null);

    assertFalse(onPreviewVoicePlaying);
    assertFalse(!!speechController.getPreviewVoicePlaying());

    const voice = createSpeechSynthesisVoice({lang: 'it', name: 'June'});
    speechController.setPreviewVoicePlaying(voice);

    assertTrue(onPreviewVoicePlaying);
    assertEquals(voice, speechController.getPreviewVoicePlaying());
  });

  test('speakMessage waits for engine load', async () => {
    const msg =
        new SpeechSynthesisUtterance('Sorry not sorry bout what I said');
    speechController.setOnSpeechSynthesisUtteranceStart(msg);

    speechController.speakMessage(msg);
    assertEquals(msg, await speech.whenCalled('speak'));
    assertFalse(speechController.isEngineLoaded());
    assertFalse(speechController.isAudioCurrentlyPlaying());

    assertTrue(!!msg.onstart);
    msg.onstart(new SpeechSynthesisEvent('type', {utterance: msg}));
    assertTrue(speechController.isEngineLoaded());
    assertTrue(speechController.isAudioCurrentlyPlaying());
  });

  test('speakMessage uses current language and speech rate', async () => {
    const rate = 1.5;
    const lang = 'hi';
    const msg = new SpeechSynthesisUtterance('I\'m just tryna have some fun');
    chrome.readingMode.speechRate = rate;
    chrome.readingMode.baseLanguageForSpeech = lang;
    speechController.setOnSpeechSynthesisUtteranceStart(msg);

    speechController.speakMessage(msg);

    const spoken = await speech.whenCalled('speak');
    assertEquals(msg, spoken);
    assertEquals(rate, msg.rate);
    assertEquals(lang, msg.lang);
  });

  test('previewVoice stops speech', () => {
    speechController.setIsSpeechActive(true);
    speechController.setIsAudioCurrentlyPlaying(true);

    speechController.previewVoice(null);

    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertEquals(
        PauseActionSource.VOICE_PREVIEW, speechController.getPauseSource());
  });

  test('previewVoice plays preview with voice', () => {
    const voice = createSpeechSynthesisVoice({lang: 'yue', name: 'August'});
    speechController.previewVoice(voice);
    assertEquals(1, speech.getCallCount('speak'));
  });

  test('previewVoice sets preview voice playing', async () => {
    const voice = createSpeechSynthesisVoice({lang: 'yue', name: 'November'});

    speechController.previewVoice(voice);

    const spoken = await speech.whenCalled('speak');
    spoken.onstart(new SpeechSynthesisEvent('type', {utterance: spoken}));
    assertEquals(voice, speechController.getPreviewVoicePlaying());

    spoken.onend();
    assertFalse(!!speechController.getPreviewVoicePlaying());
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
