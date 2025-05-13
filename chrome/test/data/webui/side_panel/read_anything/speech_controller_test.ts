// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {BrowserProxy, MAX_SPEECH_LENGTH, NodeStore, PauseActionSource, ReadAloudHighlighter, SpeechBrowserProxyImpl, SpeechController, SpeechEngineState, VoicePackController, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechErrorEvent, createSpeechSynthesisVoice, mockMetrics, setSimpleNodeStoreWithText} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('SpeechController', () => {
  let speech: TestSpeechBrowserProxy;
  let speechController: SpeechController;
  let isSpeechActiveChanged: boolean;
  let isAudioCurrentlyPlayingChanged: boolean;
  let onPreviewVoicePlaying: boolean;
  let onEngineStateChange: boolean;
  let metrics: TestMetricsBrowserProxy;
  let wordBoundaries: WordBoundaries;
  let nodeStore: NodeStore;
  let highlighter: ReadAloudHighlighter;
  let voicePackController: VoicePackController;

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
    onPreviewVoicePlaying = false;
    onEngineStateChange = false;
    const speechListener = {
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
      onSpeechRateChange() {},
    };

    voicePackController = new VoicePackController();
    voicePackController.setCurrentVoice(
        createSpeechSynthesisVoice({lang: 'en', name: 'Google Alpaca'}));
    VoicePackController.setInstance(voicePackController);
    nodeStore = new NodeStore();
    NodeStore.setInstance(nodeStore);
    wordBoundaries = new WordBoundaries();
    WordBoundaries.setInstance(wordBoundaries);
    highlighter = new ReadAloudHighlighter();
    ReadAloudHighlighter.setInstance(highlighter);
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
      speechController.initializeSpeechTree();

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

  test('onSpeechSettingsChange cancels speech', () => {
    speechController.initializeSpeechTree(1);
    speechController.setIsSpeechActive(true);
    speechController.setHasSpeechBeenTriggered(true);
    speechController.setIsAudioCurrentlyPlaying(true);

    speechController.onSpeechSettingsChange();

    assertTrue(isSpeechActiveChanged);
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertEquals(
        PauseActionSource.VOICE_SETTINGS_CHANGE,
        speechController.getPauseSource());
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(1, speech.getCallCount('cancel'));
    assertEquals(0, metrics.getCallCount('recordSpeechPlaybackLength'));
  });

  test('onPlayPauseToggle updates state', () => {
    speechController.onPlayPauseToggle(
        null, 'Listen up, let me tell you a story');

    assertTrue(isSpeechActiveChanged);
    assertTrue(speechController.isSpeechActive());
    assertTrue(speechController.hasSpeechBeenTriggered());
    assertFalse(speechController.isSpeechBeingRepositioned());
    assertEquals(1, metrics.getCallCount('recordNewPageWithSpeech'));
  });

  test('onPlayPauseToggle waits for engine load', async () => {
    const text = 'Sorry not sorry bout what I said';
    speechController.initializeSpeechTree(1);
    setSimpleNodeStoreWithText(text);

    speechController.onPlayPauseToggle(null, text);
    const spoken = await speech.whenCalled('speak');
    assertFalse(speechController.isEngineLoaded());
    assertFalse(speechController.isAudioCurrentlyPlaying());

    assertTrue(!!spoken.onstart, 'onstart');
    spoken.onstart(new SpeechSynthesisEvent('type', {utterance: spoken}));
    assertTrue(speechController.isEngineLoaded(), 'engine loaded');
    assertTrue(speechController.isAudioCurrentlyPlaying(), 'audio playing');
  });

  test('onPlayPauseToggle uses current language and speech rate', async () => {
    const rate = 1.5;
    const lang = 'hi';
    const text = 'I\'m just tryna have some fun';
    chrome.readingMode.speechRate = rate;
    chrome.readingMode.baseLanguageForSpeech = lang;
    speechController.initializeSpeechTree(1);
    setSimpleNodeStoreWithText(text);

    speechController.onPlayPauseToggle(null, text);

    const spoken = await speech.whenCalled('speak');
    assertEquals(rate, spoken.rate);
    assertEquals(lang, spoken.lang);
  });

  test('onPlayPauseToggle pauses with button click', () => {
    const source = PauseActionSource.BUTTON_CLICK;

    speechController.onPlayPauseToggle(null, 'A story that you think');
    speechController.onPlayPauseToggle(null, 'A story that you think');

    assertTrue(isSpeechActiveChanged);
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertEquals(source, speechController.getPauseSource());
    assertEquals(1, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('cancel'));
  });

  test('onPlayPauseToggle logs play session on pause', () => {
    speechController.onPlayPauseToggle(null, 'You\'ve heard before.');
    speechController.onPlayPauseToggle(null, 'You\'ve heard before.');

    assertEquals(1, metrics.getCallCount('recordSpeechPlaybackLength'));
  });

  test(
      'onPlayPauseToggle resume with no word boundaries resumes speech', () => {
        speechController.onPlayPauseToggle(null, 'We know you know our names');
        speechController.onPlayPauseToggle(null, 'We know you know our names');
        speech.reset();

        speechController.onPlayPauseToggle(null, 'We know you know our names');

        assertEquals(1, speech.getCallCount('resume'));
        assertEquals(0, speech.getCallCount('cancel'));
      });

  test(
      'onPlayPauseToggle resume with word boundaries cancels and re-speaks',
      () => {
        const textContent = 'And our fame and our faces';
        setSimpleNodeStoreWithText(textContent);
        speechController.onPlayPauseToggle(null, textContent);
        speechController.onPlayPauseToggle(null, textContent);
        wordBoundaries.updateBoundary(10);
        speech.reset();

        speechController.onPlayPauseToggle(null, textContent);

        assertEquals(1, speech.getCallCount('speak'));
        assertEquals(1, speech.getCallCount('cancel'));
      });

  suite('very long text', () => {
    function getSpokenText(): string {
      assertEquals(1, speech.getCallCount('speak'));
      return speech.getArgs('speak')[0].text.trim();
    }

    const longSentences =
        'A kingdom of isolation, and it looks like I am the queen and the ' +
        'wind is howling like this swirling storm inside, Couldn\'t keep it ' +
        'in, heaven knows I tried, but don\'t let them in, don\'t let them ' +
        'see, be the good girl you always have to be, and conceal, don\'t ' +
        'feel, don\'t let them know.' +
        'Well, now they know, let it go, let it go, can\'t hold it back ' +
        'anymore, let it go, let it go, turn away and slam the ' +
        'door- I don\'t care what they\'re going to say, let the storm rage ' +
        'on- the cold never bothered me anyway- it\'s funny how some ' +
        'distance makes everything seem small and the fears that once ' +
        'controlled me can\'t get to me at all- it\'s time to see what I can ' +
        'do to test the limits and break through- no right no wrong no rules ' +
        'for me- I\'m free- let it go let it go I am one with the wind and ' +
        'sky let it go let it go you\'ll never see me cry- here I stand and ' +
        'here I stay- let the storm rage on';

    setup(() => {
      speechController.initializeSpeechTree(1);
      setSimpleNodeStoreWithText(longSentences);
    });

    test('uses max speech length', () => {
      const expectedNumSegments =
          Math.ceil(longSentences.length / MAX_SPEECH_LENGTH);

      speechController.onPlayPauseToggle(null, longSentences);

      assertGT(expectedNumSegments, 0);
      for (let i = 0; i < expectedNumSegments; i++) {
        assertEquals(i + 1, speech.getCallCount('speak'));
        assertGT(
            MAX_SPEECH_LENGTH, speech.getArgs('speak')[i].text.trim().length);
        speech.getArgs('speak')[i].onend();
      }
    });

    test('on text-too-long error smaller text segment plays', () => {
      voicePackController.setCurrentVoice(createSpeechSynthesisVoice(
          {lang: 'en', name: 'Google Dinosaur', localService: true}));
      const accessibleTextLength =
          speechController.getUtteranceEndBoundary(longSentences, true);
      speechController.onPlayPauseToggle(null, longSentences);
      assertEquals(longSentences, getSpokenText());
      const utterance = speech.getArgs('speak')[0];
      speech.reset();

      utterance.onerror(createSpeechErrorEvent(utterance, 'text-too-long'));

      assertEquals(1, metrics.getCallCount('recordSpeechError'));
      const spoken1 = speech.getArgs('speak')[0];
      assertEquals(
          longSentences.substring(0, accessibleTextLength), getSpokenText());
      // When this segment is finished, we should speak the remaining text.
      speech.reset();
      spoken1.onend();
      assertEquals(
          longSentences.substring(accessibleTextLength), getSpokenText());
    });
  });

  test('stops speech on language-unavailable', async () => {
    const textContent = 'I\'m done cuz all this time';
    const pageLanguage = 'es';
    setSimpleNodeStoreWithText(textContent);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    voicePackController.setCurrentLanguage(pageLanguage);
    speechController.initializeSpeechTree(1);

    speechController.onPlayPauseToggle(null, textContent);
    assertEquals(1, speech.getCallCount('speak'));
    const utterance = speech.getArgs('speak')[0];
    speech.reset();

    utterance.onerror(
        createSpeechErrorEvent(utterance, 'language-unavailable'));

    assertEquals(1, metrics.getCallCount('recordSpeechError'));
    assertEquals(1, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('speak'));
    assertEquals(
        chrome.readingMode.engineErrorStopSource,
        await metrics.whenCalled('recordSpeechStopSource'));
  });

  test('stops speech on voice-unavailable', async () => {
    const textContent = 'I\'ve been just one word';
    const pageLanguage = 'es';
    setSimpleNodeStoreWithText(textContent);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    voicePackController.setCurrentLanguage(pageLanguage);
    speechController.initializeSpeechTree(1);

    speechController.onPlayPauseToggle(null, textContent);
    assertEquals(1, speech.getCallCount('speak'));
    const utterance = speech.getArgs('speak')[0];
    speech.reset();

    utterance.onerror(createSpeechErrorEvent(utterance, 'voice-unavailable'));

    assertEquals(1, metrics.getCallCount('recordSpeechError'));
    assertEquals(1, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('speak'));
    assertEquals(
        chrome.readingMode.engineErrorStopSource,
        await metrics.whenCalled('recordSpeechStopSource'));
  });

  test('invalid argument updates speech rate', () => {
    const textContent = 'In a stupid rhyme';
    const pageLanguage = 'es';
    setSimpleNodeStoreWithText(textContent);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    chrome.readingMode.speechRate = 4;
    voicePackController.setCurrentLanguage(pageLanguage);
    speechController.initializeSpeechTree(1);

    speechController.onPlayPauseToggle(null, textContent);
    assertEquals(1, speech.getCallCount('speak'));
    const utterance = speech.getArgs('speak')[0];
    speech.reset();

    utterance.onerror(createSpeechErrorEvent(utterance, 'invalid-argument'));

    assertEquals(1, chrome.readingMode.speechRate);
    assertEquals(0, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('speak'));
    assertEquals(1, metrics.getCallCount('recordSpeechError'));
    assertEquals(0, metrics.getCallCount('recordSpeechStopSource'));
  });

  test('speech interrupt while repositioning keeps playing speech', () => {
    const textContent = 'So I picked up a pen and a microphone';
    const pageLanguage = 'es';
    setSimpleNodeStoreWithText(textContent);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    chrome.readingMode.speechRate = 4;
    voicePackController.setCurrentLanguage(pageLanguage);
    speechController.initializeSpeechTree(1);

    speechController.onPlayPauseToggle(null, textContent);
    assertEquals(1, speech.getCallCount('speak'));
    const utterance = speech.getArgs('speak')[0];
    speech.reset();
    speechController.setIsSpeechBeingRepositioned(true);
    speechController.setIsSpeechActive(true);
    speechController.setIsAudioCurrentlyPlaying(true);

    utterance.onerror(createSpeechErrorEvent(utterance, 'interrupted'));

    assertTrue(speechController.isAudioCurrentlyPlaying());
    assertTrue(speechController.isSpeechActive());
    assertTrue(speechController.isSpeechBeingRepositioned());
    assertEquals(0, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(0, metrics.getCallCount('recordSpeechError'));
  });

  test('speech interrupt stops speech', async () => {
    const textContent = 'History\'s about to get overthrown';
    const pageLanguage = 'es';
    setSimpleNodeStoreWithText(textContent);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    chrome.readingMode.speechRate = 4;
    voicePackController.setCurrentLanguage(pageLanguage);
    speechController.initializeSpeechTree(1);

    speechController.onPlayPauseToggle(null, textContent);
    assertEquals(1, speech.getCallCount('speak'));
    const utterance = speech.getArgs('speak')[0];
    speech.reset();
    speechController.setIsAudioCurrentlyPlaying(true);

    utterance.onerror(createSpeechErrorEvent(utterance, 'interrupted'));

    assertEquals(
        PauseActionSource.ENGINE_INTERRUPT, speechController.getPauseSource());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isSpeechBeingRepositioned());
    assertEquals(1, speech.getCallCount('cancel'));
    assertEquals(0, metrics.getCallCount('recordSpeechError'));
    assertEquals(
        chrome.readingMode.engineInterruptStopSource,
        await metrics.whenCalled('recordSpeechStopSource'));
  });

  test('onSpeechFinished', async () => {
    speechController.onPlayPauseToggle(null, 'New phone who dis?');

    speechController.onSpeechFinished();

    assertTrue(isSpeechActiveChanged);
    assertFalse(speechController.isSpeechActive());
    assertEquals(1, metrics.getCallCount('recordSpeechPlaybackLength'));
    assertEquals(
        chrome.readingMode.contentFinishedStopSource,
        await metrics.whenCalled('recordSpeechStopSource'));
  });

  test('playNextGranularity propagates change', () => {
    let movedToNext = false;
    chrome.readingMode.getCurrentText = () => [];
    chrome.readingMode.movePositionToNextGranularity = () => {
      movedToNext = true;
    };

    speechController.playNextGranularity();

    assertTrue(movedToNext);
  });

  test('playPreviousGranularity propagates change', () => {
    let movedToPrevious: boolean = false;
    chrome.readingMode.getCurrentText = () => [];
    chrome.readingMode.movePositionToPreviousGranularity = () => {
      movedToPrevious = true;
    };

    speechController.playPreviousGranularity();

    assertTrue(movedToPrevious);
  });

  test('playNextGranularity updates state', () => {
    setSimpleNodeStoreWithText('Know all about the glories');
    speechController.setIsSpeechBeingRepositioned(false);
    wordBoundaries.updateBoundary(5);

    speechController.playNextGranularity();

    assertTrue(speechController.isSpeechBeingRepositioned());
    assertFalse(wordBoundaries.hasBoundaries());
    assertEquals(1, speech.getCallCount('cancel'));
  });

  test('playPreviousGranularity updates state', () => {
    setSimpleNodeStoreWithText('And the disgraces');
    speechController.setIsSpeechBeingRepositioned(false);
    wordBoundaries.updateBoundary(5);

    speechController.playPreviousGranularity();

    assertTrue(speechController.isSpeechBeingRepositioned());
    assertFalse(wordBoundaries.hasBoundaries());
    assertEquals(1, speech.getCallCount('cancel'));
  });

  test('onHighlightGranularityChange', async () => {
    const granularity1 = chrome.readingMode.noHighlighting;
    const granularity2 = chrome.readingMode.wordHighlighting;

    speechController.onHighlightGranularityChange(granularity1);
    assertEquals(granularity1, chrome.readingMode.highlightGranularity);
    assertEquals(
        granularity1, await metrics.whenCalled('recordHighlightGranularity'));

    metrics.reset();
    speechController.onHighlightGranularityChange(granularity2);
    assertEquals(granularity2, chrome.readingMode.highlightGranularity);
    assertEquals(
        granularity2, await metrics.whenCalled('recordHighlightGranularity'));
  });

  test('onLockScreen while paused does nothing', () => {
    speechController.onLockScreen();

    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('speak'));
  });

  test('onLockScreen while playing cancels speech', () => {
    speechController.setIsSpeechActive(true);

    speechController.onLockScreen();

    assertEquals(1, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('speak'));
  });
});
