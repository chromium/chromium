// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {BrowserProxy, currentReadHighlightClass, MAX_SPEECH_LENGTH, NodeStore, ReadAloudHighlighter, SpeechBrowserProxyImpl, SpeechController, VoiceLanguageController, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertNotEquals, assertStringContains, assertStringExcludes, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechErrorEvent, createSpeechSynthesisVoice, mockMetrics, setSimpleAxTreeWithText, setSimpleNodeStoreWithText} from './common.js';
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
  let voiceLanguageController: VoiceLanguageController;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
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
    };

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
    speechController = new SpeechController();
    speechController.addListener(speechListener);
    speech.reset();
  });

  test('clearReadAloudState', () => {
    const text = 'And I am a massive deal';
    setSimpleNodeStoreWithText(text);
    wordBoundaries.updateBoundary(4);
    speechController.onHighlightGranularityChange(
        chrome.readingMode.sentenceHighlighting);
    speechController.onPlayPauseToggle(null, text);
    assertTrue(speechController.isSpeechActive());
    assertTrue(wordBoundaries.hasBoundaries());
    assertTrue(highlighter.hasCurrentHighlights());

    speech.reset();
    isSpeechActiveChanged = false;
    isAudioCurrentlyPlayingChanged = false;

    speechController.clearReadAloudState();

    assertTrue(isSpeechActiveChanged);
    assertEquals(1, speech.getCallCount('cancel'));
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isPausedFromButton());
    assertFalse(speechController.isTemporaryPause());
    assertFalse(wordBoundaries.hasBoundaries());
    assertFalse(highlighter.hasCurrentHighlights());
  });

  test('isPausedFromButton', () => {
    assertFalse(speechController.isPausedFromButton());

    speechController.onPlayPauseToggle(null, 'No matter how many times');
    speechController.onPlayPauseToggle(null, 'No matter how many times');
    assertTrue(speechController.isPausedFromButton());

    speechController.previewVoice(null);
    assertFalse(speechController.isPausedFromButton());
  });

  test('previewVoice stops speech', () => {
    speechController.onPlayPauseToggle(null, 'Grew up in the French court');

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

  suite('initializeSpeechTree', () => {
    let initAxPositionWithNode: number;

    setup(() => {
      chrome.readingMode.initAxPositionWithNode = (nodeId) => {
        initAxPositionWithNode = nodeId;
      };
    });

    test('with no node id does nothing', () => {
      speechController.initializeSpeechTree();

      assertFalse(!!initAxPositionWithNode);
      assertFalse(speechController.isSpeechTreeInitialized());
    });

    test('when already initialized does nothing', () => {
      const id1 = 10;
      const id2 = 12;
      speechController.initializeSpeechTree(id1);
      speechController.initializeSpeechTree(id2);
      assertEquals(id1, initAxPositionWithNode);
    });

    test('initializes speech tree after content is set', () => {
      const id = 14;
      speechController.initializeSpeechTree(id);
      assertEquals(id, initAxPositionWithNode);

      // The speech tree is not initialized until content has been set.
      assertFalse(speechController.isSpeechTreeInitialized());

      setSimpleAxTreeWithText('hello');
      assertTrue(speechController.isSpeechTreeInitialized());
    });
  });

  test('onSpeechSettingsChange cancels and resumes speech if playing', () => {
    const text = 'In all the time I\'ve been by your side';
    setSimpleAxTreeWithText(text);
    setSimpleNodeStoreWithText(text);
    speechController.onPlayPauseToggle(null, text);
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
    const text = 'I\'ve never lost control';
    speechController.setHasSpeechBeenTriggered(true);
    setSimpleAxTreeWithText(text);
    setSimpleNodeStoreWithText(text);

    speechController.onSpeechSettingsChange();

    assertFalse(isSpeechActiveChanged);
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertFalse(speechController.isPausedFromButton());
    assertTrue(speechController.isTemporaryPause());
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(1, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('speak'));
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
    setSimpleAxTreeWithText(text);
    setSimpleNodeStoreWithText(text);

    speechController.onPlayPauseToggle(null, text);
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
    setSimpleAxTreeWithText(text);
    setSimpleNodeStoreWithText(text);

    speechController.onPlayPauseToggle(null, text);

    const spoken = await speech.whenCalled('speak');
    assertEquals(rate, spoken.rate);
    assertEquals(lang, spoken.lang);
  });

  test('onPlayPauseToggle pauses with button click', () => {
    speechController.onPlayPauseToggle(null, 'A story that you think');
    speechController.onPlayPauseToggle(null, 'A story that you think');

    assertTrue(isSpeechActiveChanged);
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isAudioCurrentlyPlaying());
    assertTrue(speechController.isPausedFromButton());
    assertFalse(speechController.isTemporaryPause());
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
      setSimpleAxTreeWithText(longSentences);
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
      voiceLanguageController.setUserPreferredVoice(createSpeechSynthesisVoice(
          {lang: 'en', name: 'Google Dinosaur', localService: true}));
      speechController.onPlayPauseToggle(null, longSentences);
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
    setSimpleAxTreeWithText(textContent);
    setSimpleNodeStoreWithText(textContent);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    chrome.readingMode.baseLanguageForSpeech = pageLanguage;
    voiceLanguageController.onPageLanguageChanged();

    speechController.onPlayPauseToggle(null, textContent);
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
    setSimpleAxTreeWithText(textContent);
    setSimpleNodeStoreWithText(textContent);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    chrome.readingMode.baseLanguageForSpeech = pageLanguage;
    voiceLanguageController.onPageLanguageChanged();

    speechController.onPlayPauseToggle(null, textContent);
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
    setSimpleAxTreeWithText(textContent);
    setSimpleNodeStoreWithText(textContent);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    chrome.readingMode.speechRate = 4;
    chrome.readingMode.baseLanguageForSpeech = pageLanguage;
    voiceLanguageController.onPageLanguageChanged();

    speechController.onPlayPauseToggle(null, textContent);
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
    setSimpleAxTreeWithText(textContent);
    setSimpleNodeStoreWithText(textContent);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    chrome.readingMode.speechRate = 4;
    chrome.readingMode.baseLanguageForSpeech = pageLanguage;
    voiceLanguageController.onPageLanguageChanged();

    speechController.onPlayPauseToggle(null, textContent);
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
    setSimpleAxTreeWithText(textContent);
    setSimpleNodeStoreWithText(textContent);
    assertNotEquals(chrome.readingMode.defaultLanguageForSpeech, pageLanguage);
    chrome.readingMode.speechRate = 4;
    chrome.readingMode.baseLanguageForSpeech = pageLanguage;
    voiceLanguageController.onPageLanguageChanged();

    speechController.onPlayPauseToggle(null, textContent);
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
    assertEquals(1, speech.getCallCount('cancel'));
    assertEquals(0, metrics.getCallCount('recordSpeechError'));
    assertEquals(
        chrome.readingMode.engineInterruptStopSource,
        await metrics.whenCalled('recordSpeechStopSource'));
  });

  test('speech finished clears state', async () => {
    const text = 'New phone who dis?';
    setSimpleAxTreeWithText(text);
    setSimpleNodeStoreWithText(text);

    speechController.onPlayPauseToggle(null, text);

    const spoken = await speech.whenCalled('speak');
    assertEquals(text, spoken.text);

    speech.reset();
    isSpeechActiveChanged = false;
    chrome.readingMode.getCurrentText = () => [];
    spoken.onend();

    assertTrue(isSpeechActiveChanged);
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isPausedFromButton());
    assertFalse(speechController.isTemporaryPause());
    assertEquals(1, metrics.getCallCount('recordSpeechPlaybackLength'));
    assertEquals(
        chrome.readingMode.contentFinishedStopSource,
        await metrics.whenCalled('recordSpeechStopSource'));
  });

  test('onNextGranularityClick propagates change', () => {
    let movedToNext = false;
    chrome.readingMode.getCurrentText = () => [];
    chrome.readingMode.movePositionToNextGranularity = () => {
      movedToNext = true;
    };

    speechController.onNextGranularityClick();

    assertTrue(movedToNext);
  });

  test('onPreviousGranularityClick propagates change', () => {
    let movedToPrevious: boolean = false;
    chrome.readingMode.getCurrentText = () => [];
    chrome.readingMode.movePositionToPreviousGranularity = () => {
      movedToPrevious = true;
    };

    speechController.onPreviousGranularityClick();

    assertTrue(movedToPrevious);
  });

  test('onNextGranularityClick updates state', () => {
    setSimpleNodeStoreWithText('Know all about the glories');
    wordBoundaries.updateBoundary(5);

    speechController.onNextGranularityClick();

    assertTrue(speechController.isSpeechBeingRepositioned());
    assertFalse(wordBoundaries.hasBoundaries());
    assertEquals(1, speech.getCallCount('cancel'));
  });

  test('onPreviousGranularityClick updates state', () => {
    setSimpleNodeStoreWithText('And the disgraces');
    wordBoundaries.updateBoundary(5);

    speechController.onPreviousGranularityClick();

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
    speechController.onPlayPauseToggle(null, 'Oui, oui bonjour');
    speech.reset();

    speechController.onLockScreen();

    assertEquals(1, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('speak'));
  });

  test('onVoiceMenuClose resume speech only if it was active before', () => {
    const text = 'You must agree that baby';
    setSimpleNodeStoreWithText(text);
    speechController.onVoiceMenuOpen();

    speechController.onVoiceMenuClose();

    assertEquals(0, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('speak'));

    speechController.onPlayPauseToggle(null, text);
    speechController.onVoiceMenuOpen();
    speechController.onPlayPauseToggle(null, text);
    speech.reset();

    speechController.onVoiceMenuClose();

    assertEquals(1, speech.getCallCount('resume'));
    assertEquals(0, speech.getCallCount('cancel'));
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

  test('onLinksToggled rehighlights', () => {
    const text = 'Life was a chore, so';
    const id = 2;
    chrome.readingMode.getCurrentText = () => [id];
    chrome.readingMode.getTextContent = () => text;
    chrome.readingMode.getCurrentTextStartIndex = () => 0;
    chrome.readingMode.getCurrentTextEndIndex = () => text.length;
    const sentence = document.createElement('p');
    sentence.appendChild(document.createTextNode(text));
    nodeStore.setDomNode(sentence, id);
    nodeStore.setDomNode(sentence, id);
    speechController.onHighlightGranularityChange(
        chrome.readingMode.sentenceHighlighting);
    speechController.onPlayPauseToggle(null, text);
    assertTrue(highlighter.hasCurrentHighlights());
    speech.reset();

    speechController.onLinksToggled();

    assertTrue(highlighter.hasCurrentHighlights());
    assertStringContains(
        (nodeStore.getDomNode(id) as Element).innerHTML,
        currentReadHighlightClass);
  });

  test('onLinksToggled does not highlight if no highlights', () => {
    const text = 'She set sail';
    const id = 2;
    chrome.readingMode.getCurrentText = () => [];
    const sentence = document.createElement('p');
    sentence.appendChild(document.createTextNode(text));
    nodeStore.setDomNode(sentence, id);
    speechController.onHighlightGranularityChange(
        chrome.readingMode.sentenceHighlighting);
    assertFalse(highlighter.hasCurrentHighlights());

    speechController.onLinksToggled();

    assertFalse(highlighter.hasCurrentHighlights());
    assertStringExcludes(
        (nodeStore.getDomNode(id) as Element).innerHTML,
        currentReadHighlightClass);
  });

  test('set previous reading position without saved state does nothing', () => {
    const text = 'But I took your hand';
    setSimpleNodeStoreWithText(text);
    wordBoundaries.updateBoundary(4);
    speechController.onHighlightGranularityChange(
        chrome.readingMode.sentenceHighlighting);
    speechController.onPlayPauseToggle(null, text);
    speechController.onPlayPauseToggle(null, text);
    assertTrue(speechController.hasSpeechBeenTriggered());
    assertTrue(wordBoundaries.hasBoundaries());
    assertTrue(highlighter.hasCurrentHighlights());

    speechController.clearReadAloudState();
    speechController.setPreviousReadingPositionIfExists();

    assertFalse(speechController.hasSpeechBeenTriggered());
    assertFalse(wordBoundaries.hasBoundaries());
    assertFalse(highlighter.hasCurrentHighlights());
  });

  test('set previous reading position restores saved state', () => {
    const text = 'And promised I\'d withstand';
    setSimpleNodeStoreWithText(text);
    wordBoundaries.updateBoundary(4);
    speechController.onHighlightGranularityChange(
        chrome.readingMode.sentenceHighlighting);
    speechController.onPlayPauseToggle(null, text);
    speechController.onPlayPauseToggle(null, text);
    assertTrue(speechController.hasSpeechBeenTriggered());
    assertTrue(wordBoundaries.hasBoundaries());
    assertTrue(highlighter.hasCurrentHighlights());

    speechController.saveReadAloudState();
    speechController.clearReadAloudState();
    speechController.setPreviousReadingPositionIfExists();

    assertTrue(speechController.hasSpeechBeenTriggered());
    assertTrue(wordBoundaries.hasBoundaries());
    assertTrue(highlighter.hasCurrentHighlights());
  });
});
