// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {BrowserProxy, MAX_SPEECH_LENGTH, NodeStore, ReadAloudHighlighter, SelectionController, setInstance, SpeechBrowserProxyImpl, SpeechController, VoiceLanguageController, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGE, assertGT, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechErrorEvent, createSpeechSynthesisVoice, createWordBoundaryEvent, mockMetrics, setContent} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestReadAloudModelBrowserProxy} from './test_read_aloud_browser_proxy.js';
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
  let voiceLanguageController: VoiceLanguageController;
  let readingMode: FakeReadingMode;
  let selectionController: SelectionController;
  let readAloudModel: TestReadAloudModelBrowserProxy;

  // TODO: crbug.com/440400392- Move all tests relying on chrome.readingMode
  // for text segmentation to use TestReadAloudModelBrowserProxy instead.
  function onPlayPauseToggle(text: string) {
    setContent(text, readAloudModel);
    const element = document.createElement('p');
    element.textContent = text;
    speechController.onPlayPauseToggle(element);
  }

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    metrics = mockMetrics();
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

      onPlayingFromSelection() {

      },
    };

    readAloudModel = new TestReadAloudModelBrowserProxy();
    setInstance(readAloudModel);
    readAloudModel.setInitialized(true);
    voiceLanguageController = new VoiceLanguageController();
    voiceLanguageController.setUserPreferredVoice(
        createSpeechSynthesisVoice({lang: 'en', name: 'Google Alpaca'}));
    VoiceLanguageController.setInstance(voiceLanguageController);
    nodeStore = new NodeStore();
    NodeStore.setInstance(nodeStore);
    wordBoundaries = new WordBoundaries();
    WordBoundaries.setInstance(wordBoundaries);
    highlighter = new ReadAloudHighlighter();
    ReadAloudHighlighter.setInstance(highlighter);
    selectionController = new SelectionController();
    SelectionController.setInstance(selectionController);
    speechController = new SpeechController();
    speechController.addListener(speechListener);
    speech.reset();
  });

  test('isPausedFromButton', () => {
    assertFalse(speechController.isPausedFromButton());

    onPlayPauseToggle('No matter how many times');
    onPlayPauseToggle('No matter how many times');

    assertTrue(speechController.isPausedFromButton());
  });

  test('pause source is not updated if already paused', () => {
    assertFalse(speechController.isPausedFromButton());

    onPlayPauseToggle('No matter how many times');
    onPlayPauseToggle('No matter how many times');
    assertTrue(speechController.isPausedFromButton());

    speechController.previewVoice(null);
    assertTrue(speechController.isPausedFromButton());
  });

  test('isTemporaryPause', () => {
    assertFalse(speechController.isTemporaryPause());

    onPlayPauseToggle('No matter how many times');
    onPlayPauseToggle('No matter how many times');
    assertFalse(speechController.isTemporaryPause());

    onPlayPauseToggle('No matter how many times');
    speechController.previewVoice(null);
    assertTrue(speechController.isTemporaryPause());
  });

  test('previewVoice stops speech', () => {
    onPlayPauseToggle('Grew up in the French court');

    speechController.previewVoice(null);

    assertFalse(onPreviewVoicePlaying);
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertFalse(speechController.isPausedFromButton());
    assertTrue(speechController.isTemporaryPause());
  });

  test('previewVoice plays preview with voice', () => {
    const voice = createSpeechSynthesisVoice({lang: 'yue', name: 'August'});
    speechController.previewVoice(voice);
    assertEquals(1, speech.getCallCount('speak'));
  });

  test('previewVoice sets preview voice playing', async () => {
    const voice = createSpeechSynthesisVoice({lang: 'yue', name: 'November'});

    speechController.previewVoice(voice);
    assertFalse(onPreviewVoicePlaying);

    const spoken = await speech.whenCalled('speak');
    spoken.onstart(new SpeechSynthesisEvent('type', {utterance: spoken}));
    assertTrue(onPreviewVoicePlaying);
    assertEquals(voice, speechController.getPreviewVoicePlaying());

    onPreviewVoicePlaying = false;
    spoken.onend();
    assertTrue(onPreviewVoicePlaying);
    assertFalse(!!speechController.getPreviewVoicePlaying());
  });

  test('onSpeechSettingsChange cancels and resumes speech if playing', () => {
    const text = 'In all the time I\'ve been by your side';
    setContent(text, readAloudModel);
    onPlayPauseToggle(text);
    speech.reset();

    speechController.onSpeechSettingsChange();

    assertTrue(isSpeechActiveChanged);
    assertFalse(speechController.isPausedFromButton());
    assertTrue(speechController.isTemporaryPause());
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(2, speech.getCallCount('cancel'));
    assertEquals(1, speech.getCallCount('speak'));
    assertEquals(0, metrics.getCallCount('recordSpeechPlaybackLength'));
  });

  test('onSpeechSettingsChange does not resume speech if not playing', () => {
    speechController.setHasSpeechBeenTriggered(true);
    setContent('I\'ve never lost control', readAloudModel);

    speechController.onSpeechSettingsChange();

    assertFalse(isSpeechActiveChanged);
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertFalse(speechController.isPausedFromButton());
    assertFalse(speechController.isTemporaryPause());
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(1, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('speak'));
    assertEquals(0, metrics.getCallCount('recordSpeechPlaybackLength'));
  });

  test('onPlayPauseToggle updates state', () => {
    onPlayPauseToggle('Listen up, let me tell you a story');

    assertTrue(isSpeechActiveChanged);
    assertTrue(speechController.isSpeechActive());
    assertTrue(speechController.hasSpeechBeenTriggered());
    assertFalse(speechController.isSpeechBeingRepositioned());
    assertEquals(1, metrics.getCallCount('recordNewPageWithSpeech'));
  });

  test('onPlayPauseToggle waits for engine load', async () => {
    const text = 'Sorry not sorry bout what I said';
    setContent(text, readAloudModel);

    onPlayPauseToggle(text);
    const spoken = await speech.whenCalled('speak');
    assertTrue(onEngineStateChange);
    assertFalse(isAudioCurrentlyPlayingChanged);
    assertFalse(speechController.isEngineLoaded());
    assertFalse(speechController.isAudioCurrentlyPlaying());

    onEngineStateChange = false;
    assertTrue(!!spoken.onstart, 'onstart');
    spoken.onstart(new SpeechSynthesisEvent('type', {utterance: spoken}));
    assertTrue(onEngineStateChange);
    assertTrue(isAudioCurrentlyPlayingChanged);
    assertTrue(speechController.isEngineLoaded());
    assertTrue(speechController.isAudioCurrentlyPlaying());
  });

  test('onPlayPauseToggle uses current language and speech rate', async () => {
    const rate = 1.5;
    const lang = 'hi';
    const text = 'I\'m just tryna have some fun';
    chrome.readingMode.speechRate = rate;
    chrome.readingMode.baseLanguageForSpeech = lang;
    setContent(text, readAloudModel);

    onPlayPauseToggle(text);

    const spoken = await speech.whenCalled('speak');
    assertEquals(rate, spoken.rate);
    assertEquals(lang, spoken.lang);
  });

  test('onPlayPauseToggle pauses with button click', () => {
    onPlayPauseToggle('A story that you think');
    speech.reset();
    onPlayPauseToggle('A story that you think');

    assertTrue(isSpeechActiveChanged);
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertTrue(speechController.isPausedFromButton());
    assertFalse(speechController.isTemporaryPause());
    assertEquals(1, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('cancel'));
  });

  test('onPlayPauseToggle logs play session on pause', () => {
    onPlayPauseToggle('You\'ve heard before.');
    onPlayPauseToggle('You\'ve heard before.');

    assertEquals(1, metrics.getCallCount('recordSpeechPlaybackLength'));
  });

  test(
      'onPlayPauseToggle resume with no word boundaries resumes speech', () => {
        onPlayPauseToggle('We know you know our names');
        onPlayPauseToggle('We know you know our names');
        speech.reset();

        onPlayPauseToggle('We know you know our names');

        assertEquals(1, speech.getCallCount('resume'));
        assertEquals(0, speech.getCallCount('cancel'));
      });

  test('word boundary received updates words heard', () => {
    const textContent = 'You\'re all I can think of';
    setContent(textContent, readAloudModel);
    onPlayPauseToggle(textContent);
    const spoken = speech.getArgs('speak')[0];

    spoken.onboundary(createWordBoundaryEvent(spoken, 0, 6));
    spoken.onboundary(createWordBoundaryEvent(spoken, 7, 3));

    assertEquals(2, readingMode.wordsHeard);
  });

  test('words heard not updated for whitespace', () => {
    const textContent = 'Every drop I drink up';
    setContent(textContent, readAloudModel);
    onPlayPauseToggle(textContent);
    const spoken = speech.getArgs('speak')[0];

    spoken.onboundary(createWordBoundaryEvent(spoken, 0, 5));
    spoken.onboundary(createWordBoundaryEvent(spoken, 5, 1));

    assertEquals(1, readingMode.wordsHeard);
  });

  test('words heard reset on clear', () => {
    const textContent = 'You\'re my soda pop';
    setContent(textContent, readAloudModel);
    onPlayPauseToggle(textContent);
    const spoken = speech.getArgs('speak')[0];

    spoken.onboundary(createWordBoundaryEvent(spoken, 0, 5));
    spoken.onboundary(createWordBoundaryEvent(spoken, 6, 2));
    assertEquals(2, readingMode.wordsHeard);

    speechController.clearReadAloudState();
    spoken.onboundary(createWordBoundaryEvent(spoken, 9, 4));
    assertEquals(1, readingMode.wordsHeard);
  });

  test('sentence end with word boundaries, does not count sentence', () => {
    const textContent = 'My little soda pop';
    setContent(textContent, readAloudModel);
    onPlayPauseToggle(textContent);
    const spoken = speech.getArgs('speak')[0];

    spoken.onboundary(createWordBoundaryEvent(spoken, 0, 2));
    assertEquals(1, readingMode.wordsHeard);

    spoken.onend();
    assertEquals(1, readingMode.wordsHeard);
  });

  test('sentence end with no word boundaries, counts sentence', () => {
    const textContent = 'Cool me down, you\'re so hot';
    setContent(textContent, readAloudModel);
    onPlayPauseToggle(textContent);
    const spoken = speech.getArgs('speak')[0];

    spoken.onend();

    assertEquals(6, readingMode.wordsHeard);
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
      setContent(longSentences, readAloudModel);
    });

    test('uses max speech length', () => {
      const expectedNumSegments =
          Math.ceil(longSentences.length / MAX_SPEECH_LENGTH);

      onPlayPauseToggle(longSentences);

      assertGT(expectedNumSegments, 0);
      for (let i = 0; i < expectedNumSegments; i++) {
        assertEquals(i + 1, speech.getCallCount('speak'));
        assertGE(
            MAX_SPEECH_LENGTH, speech.getArgs('speak')[i].text.trim().length);
        speech.getArgs('speak')[i].onend();
      }
    });

    test('on text-too-long error smaller text segment plays', () => {
      voiceLanguageController.setUserPreferredVoice(createSpeechSynthesisVoice(
          {lang: 'en', name: 'Google Dinosaur', localService: true}));
      onPlayPauseToggle(longSentences);
      assertEquals(longSentences, getSpokenText());
      const utterance = speech.getArgs('speak')[0];
      speech.reset();

      utterance.onerror(createSpeechErrorEvent(utterance, 'text-too-long'));

      assertTrue(onEngineStateChange);
      assertEquals(1, metrics.getCallCount('recordSpeechError'));
      const spoken1 = speech.getArgs('speak')[0];
      const spokenTextLength = getSpokenText().length;
      assertGT(MAX_SPEECH_LENGTH, spokenTextLength);
      // When this segment is finished, we should speak the remaining text.
      speech.reset();
      spoken1.onend();
      assertEquals(
          longSentences.length - spokenTextLength, getSpokenText().length);
    });
  });

  test('stops speech on language-unavailable', async () => {
    const textContent = 'I\'m done cuz all this time';
    const pageLanguage = 'es';
    setContent(textContent, readAloudModel);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    const voice = createSpeechSynthesisVoice({lang: 'en', name: 'Google Og'});
    speech.setVoices([voice]);
    voiceLanguageController.setUserPreferredVoice(voice);
    chrome.readingMode.baseLanguageForSpeech = pageLanguage;
    voiceLanguageController.onPageLanguageChanged();

    onPlayPauseToggle(textContent);
    assertEquals(1, speech.getCallCount('speak'));
    const utterance = speech.getArgs('speak')[0];
    speech.reset();

    utterance.onerror(
        createSpeechErrorEvent(utterance, 'language-unavailable'));

    assertTrue(onEngineStateChange);
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
    setContent(textContent, readAloudModel);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    const voice = createSpeechSynthesisVoice({lang: 'en', name: 'Google Og'});
    speech.setVoices([voice]);
    voiceLanguageController.setUserPreferredVoice(voice);
    chrome.readingMode.baseLanguageForSpeech = pageLanguage;
    voiceLanguageController.onPageLanguageChanged();

    onPlayPauseToggle(textContent);
    assertEquals(1, speech.getCallCount('speak'));
    const utterance = speech.getArgs('speak')[0];
    speech.reset();

    utterance.onerror(createSpeechErrorEvent(utterance, 'voice-unavailable'));

    assertTrue(onEngineStateChange);
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
    setContent(textContent, readAloudModel);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    const voice = createSpeechSynthesisVoice({lang: 'en', name: 'Google Og'});
    speech.setVoices([voice]);
    voiceLanguageController.setUserPreferredVoice(voice);
    chrome.readingMode.speechRate = 4;
    chrome.readingMode.baseLanguageForSpeech = pageLanguage;
    voiceLanguageController.onPageLanguageChanged();

    onPlayPauseToggle(textContent);
    assertEquals(1, speech.getCallCount('speak'));
    const utterance = speech.getArgs('speak')[0];
    speech.reset();

    utterance.onerror(createSpeechErrorEvent(utterance, 'invalid-argument'));

    assertTrue(onEngineStateChange);
    assertEquals(1, chrome.readingMode.speechRate);
    assertEquals(2, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(1, speech.getCallCount('speak'));
    assertEquals(1, metrics.getCallCount('recordSpeechError'));
    assertEquals(0, metrics.getCallCount('recordSpeechStopSource'));
  });

  test('speech interrupt while repositioning keeps playing speech', () => {
    const textContent = 'So I picked up a pen and a microphone';
    const pageLanguage = 'es';
    setContent(textContent, readAloudModel);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    const voice = createSpeechSynthesisVoice({lang: 'en', name: 'Google Og'});
    speech.setVoices([voice]);
    voiceLanguageController.setUserPreferredVoice(voice);
    chrome.readingMode.speechRate = 4;
    chrome.readingMode.baseLanguageForSpeech = pageLanguage;
    voiceLanguageController.onPageLanguageChanged();

    onPlayPauseToggle(textContent);
    assertEquals(1, speech.getCallCount('speak'));
    const utterance = speech.getArgs('speak')[0];
    utterance.onstart(new SpeechSynthesisEvent('type', {utterance: utterance}));
    speechController.onNextGranularityClick();
    speech.reset();

    utterance.onerror(createSpeechErrorEvent(utterance, 'interrupted'));

    assertTrue(onEngineStateChange);
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
    setContent(textContent, readAloudModel);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    const voice = createSpeechSynthesisVoice({lang: 'en', name: 'Google Og'});
    speech.setVoices([voice]);
    voiceLanguageController.setUserPreferredVoice(voice);
    chrome.readingMode.speechRate = 4;
    chrome.readingMode.baseLanguageForSpeech = pageLanguage;
    voiceLanguageController.onPageLanguageChanged();

    onPlayPauseToggle(textContent);
    assertEquals(1, speech.getCallCount('speak'));
    const utterance = speech.getArgs('speak')[0];
    speech.reset();
    utterance.onstart(new SpeechSynthesisEvent('type', {utterance: utterance}));

    utterance.onerror(createSpeechErrorEvent(utterance, 'interrupted'));

    assertTrue(onEngineStateChange);
    assertFalse(speechController.isPausedFromButton());
    assertFalse(speechController.isTemporaryPause());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isSpeechBeingRepositioned());
    // We should not cancel again as the interrupt error can only happen with a
    // call to cancel.
    assertEquals(0, speech.getCallCount('cancel'));
    assertEquals(0, metrics.getCallCount('recordSpeechError'));
    assertEquals(
        chrome.readingMode.engineInterruptStopSource,
        await metrics.whenCalled('recordSpeechStopSource'));
  });

  test('speech finished clears state', async () => {
    const text = 'New phone who dis?';
    setContent(text, readAloudModel);

    onPlayPauseToggle(text);

    const spoken = await speech.whenCalled('speak');
    assertEquals(text, spoken.text);

    speech.reset();
    isSpeechActiveChanged = false;
    readAloudModel.setCurrentTextSegments([]);
    spoken.onend();

    assertTrue(isSpeechActiveChanged);
    assertEquals(1, readAloudModel.getCallCount('resetSpeechToBeginning'));
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isPausedFromButton());
    assertFalse(speechController.isTemporaryPause());
    assertEquals(1, metrics.getCallCount('recordSpeechPlaybackLength'));
    assertEquals(
        chrome.readingMode.contentFinishedStopSource,
        await metrics.whenCalled('recordSpeechStopSource'));
  });

  test('onNextGranularityClick propagates change', () => {
    speechController.onNextGranularityClick();
    assertEquals(1, readAloudModel.getCallCount('moveSpeechForward'));
  });

  test('onPreviousGranularityClick propagates change', () => {
    speechController.onPreviousGranularityClick();
    assertEquals(1, readAloudModel.getCallCount('moveSpeechBackwards'));
  });

  test('onHighlightGranularityChange draws highlight', () => {
    const granularity = chrome.readingMode.wordHighlighting;
    setContent('no more melon cake', readAloudModel);
    assertFalse(highlighter.hasCurrentGranularity());

    speechController.onHighlightGranularityChange(granularity);

    assertTrue(highlighter.hasCurrentGranularity());
  });

  test('onLockScreen while paused does nothing', () => {
    speechController.onLockScreen();

    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('speak'));
  });

  test('onLockScreen while playing cancels speech', () => {
    onPlayPauseToggle('Oui, oui bonjour');
    speech.reset();

    speechController.onLockScreen();

    assertEquals(1, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('speak'));
  });

  test('onVoiceSelected sets current voice', () => {
    const voice1 = createSpeechSynthesisVoice({lang: 'pt-pt', name: 'Donkey'});
    const voice2 = createSpeechSynthesisVoice({lang: 'pt-br', name: 'Corgi'});
    voiceLanguageController.setUserPreferredVoice(voice1);
    let sentName = '';
    let sentLang = '';
    chrome.readingMode.onVoiceChange = (name, lang) => {
      sentName = name;
      sentLang = lang;
    };

    speechController.onVoiceSelected(voice2);

    assertEquals(voice2, voiceLanguageController.getCurrentVoice());
    assertEquals(voice2.name, sentName);
    assertEquals(voice2.lang, sentLang);
  });

  test('onVoiceSelected resets word boundaries on different locale', () => {
    const voice1 = createSpeechSynthesisVoice({lang: 'pt-pt', name: 'Tabby'});
    const voice2 = createSpeechSynthesisVoice({lang: 'pt-PT', name: 'Cheetah'});
    const voice3 = createSpeechSynthesisVoice({lang: 'pt-br', name: 'Leopard'});
    voiceLanguageController.setUserPreferredVoice(voice1);
    wordBoundaries.updateBoundary(10);

    speechController.onVoiceSelected(voice2);
    assertTrue(wordBoundaries.hasBoundaries());

    speechController.onVoiceSelected(voice3);
    assertFalse(wordBoundaries.hasBoundaries());
  });
});
