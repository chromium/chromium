// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {MAX_SPEECH_LENGTH_FOR_REMOTE_VOICES, PauseActionSource, playFromSelectionTimeout, SpeechBrowserProxyImpl, ToolbarEvent, WordBoundaryMode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createAndSetVoices, createSpeechErrorEvent, createSpeechSynthesisVoice, emitEvent, mockMetrics, setSimpleAxTreeWithText, setupBasicSpeech} from './common.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('Speech', () => {
  let app: AppElement;
  let speech: TestSpeechBrowserProxy;
  let metrics: TestMetricsBrowserProxy;

  const paragraph1: string[] = [
    'Something has changed within me, something is not the same.',
    'I\'m through with playing by the rules of someone else\'s game.',
    'Too late for second guessing; too late to go back to sleep.',
    'It\'s time to trust my instincts, close my eyes, and leap!',
  ];
  const paragraph2: string[] = [
    'It\'s time to try defying gravity.',
    'I think I\'ll try defying gravity.',
    'Kiss me goodbye, I\'m defying gravity.',
    'And you won\'t bring me down.',
  ];

  const leafIds = [3, 5];
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 4],
      },
      {
        id: 2,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: paragraph1.join(' '),
      },
      {
        id: 4,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: paragraph2.join(' '),
      },
    ],
  };

  function getSpokenText(): string {
    assertEquals(1, speech.getCallCount('speak'));
    return speech.getArgs('speak')[0].text.trim();
  }

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Do not call the real `onConnected()`. As defined in
    // ReadAnythingAppController, onConnected creates mojo pipes to connect to
    // the rest of the Read Anything feature, which we are not testing here.
    chrome.readingMode.onConnected = () => {};
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    chrome.readingMode.shouldShowUi = () => true;
    chrome.readingMode.showLoading = () => {};
    chrome.readingMode.restoreSettingsFromPrefs = () => {};
    chrome.readingMode.languageChanged = () => {};
    chrome.readingMode.onTtsEngineInstalled = () => {};
    metrics = mockMetrics();

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    setupBasicSpeech(app, speech);
    chrome.readingMode.setContentForTesting(axTree, leafIds);
    speech.reset();
  });

  suite('on play', () => {
    setup(() => {
      app.playSpeech();
    });

    test('speaks all text by sentences', () => {
      assertEquals(1, speech.getCallCount('speak'));
      const spoken1 = speech.getArgs('speak')[0];
      assertEquals(paragraph1[0], spoken1.text.trim());

      spoken1.onend();
      assertEquals(2, speech.getCallCount('speak'));
      const spoken2 = speech.getArgs('speak')[1];
      assertEquals(paragraph1[1], spoken2.text.trim());

      spoken2.onend();
      assertEquals(3, speech.getCallCount('speak'));
      const spoken3 = speech.getArgs('speak')[2];
      assertEquals(paragraph1[2], spoken3.text.trim());

      spoken3.onend();
      assertEquals(4, speech.getCallCount('speak'));
      const spoken4 = speech.getArgs('speak')[3];
      assertEquals(paragraph1[3], spoken4.text.trim());

      spoken4.onend();
      assertEquals(5, speech.getCallCount('speak'));
      const spoken5 = speech.getArgs('speak')[4];
      assertEquals(paragraph2[0], spoken5.text.trim());

      spoken5.onend();
      assertEquals(6, speech.getCallCount('speak'));
      const spoken6 = speech.getArgs('speak')[5];
      assertEquals(paragraph2[1], spoken6.text.trim());

      spoken6.onend();
      assertEquals(7, speech.getCallCount('speak'));
      const spoken7 = speech.getArgs('speak')[6];
      assertEquals(paragraph2[2], spoken7.text.trim());

      spoken7.onend();
      assertEquals(8, speech.getCallCount('speak'));
      const spoken8 = speech.getArgs('speak')[7];
      assertEquals(paragraph2[3], spoken8.text.trim());

      spoken8.onend();
      assertEquals(8, speech.getCallCount('speak'));
    });

    test('uses set language', () => {
      let expectedLang = 'en';
      assertEquals(1, speech.getCallCount('speak'));
      assertEquals(expectedLang, speech.getArgs('speak')[0].lang);

      expectedLang = 'fr';
      chrome.readingMode.setLanguageForTesting(expectedLang);
      speech.reset();
      app.playSpeech();

      assertEquals(1, speech.getCallCount('speak'));
      assertEquals(expectedLang, speech.getArgs('speak')[0].lang);

      expectedLang = 'zh';
      chrome.readingMode.setLanguageForTesting(expectedLang);
      speech.reset();
      app.playSpeech();

      assertEquals(1, speech.getCallCount('speak'));
      assertEquals(expectedLang, speech.getArgs('speak')[0].lang);
    });

    test('speechPlayingState initialized correctly', () => {
      assertFalse(app.speechPlayingState.isSpeechBeingRepositioned);
    });
  });

  test('on finished, logs speech stop source', async () => {
    app.playSpeech();
    for (let i = 0; i < paragraph1.length + paragraph2.length; i++) {
      const spoken = speech.getArgs('speak')[i];
      assertTrue(!!spoken);
      spoken.onend();
    }
    assertEquals(
        chrome.readingMode.contentFinishedStopSource,
        await metrics.whenCalled('recordSpeechStopSource'));
  });

  suite('with text selected', () => {
    let mockTimer: MockTimer;

    function selectAndPlay(
        baseTree: any, anchorId: number, anchorOffset: number, focusId: number,
        focusOffset: number, isBackward: boolean = false): void {
      select(
          baseTree, anchorId, anchorOffset, focusId, focusOffset, isBackward);
      playFromSelection();
    }

    function select(
        baseTree: any, anchorId: number, anchorOffset: number, focusId: number,
        focusOffset: number, isBackward: boolean = false): void {
      mockTimer.install();
      const selectedTree = Object.assign(
          {
            selection: {
              anchor_object_id: anchorId,
              focus_object_id: focusId,
              anchor_offset: anchorOffset,
              focus_offset: focusOffset,
              is_backward: isBackward,
            },
          },
          baseTree);
      chrome.readingMode.setContentForTesting(selectedTree, leafIds);
      app.updateSelection();
    }

    function playFromSelection() {
      app.playSpeech();
      mockTimer.tick(playFromSelectionTimeout);
      mockTimer.uninstall();
    }

    setup(() => {
      mockTimer = new MockTimer();
      return microtasksFinished();
    });

    test('first play starts from selected node', () => {
      selectAndPlay(axTree, 5, 0, 5, 7);
      assertEquals(paragraph2[0], getSpokenText());
    });

    test('selection is cleared after play', () => {
      selectAndPlay(axTree, 5, 0, 5, 10);
      assertEquals('None', app.getSelection().type);
    });

    test('in middle of node, play from beginning of node', () => {
      selectAndPlay(axTree, 5, 10, 5, 20);
      assertEquals(paragraph2[0], getSpokenText());
    });

    test('when selection crosses nodes, play from earlier node', () => {
      selectAndPlay(axTree, 3, 10, 5, 10);
      assertEquals(paragraph1[0], getSpokenText());
    });

    test('when selection is backward, play from earlier node', () => {
      selectAndPlay(axTree, 5, 10, 3, 10, /*isBackward=*/ true);
      assertEquals(paragraph1[0], getSpokenText());
    });

    test('after speech started, cancels and plays from selection', () => {
      select(axTree, 5, 0, 5, 10);
      app.speechPlayingState.isSpeechTreeInitialized = true;
      app.speechPlayingState.hasSpeechBeenTriggered = true;
      speech.reset();

      playFromSelection();

      assertEquals(1, speech.getCallCount('cancel'));
      assertEquals(paragraph2[0], getSpokenText());
    });

    test('play from selection when node split across sentences', () => {
      const fragment1 = ' This is a sentence';
      const fragment2 = ' that ends in the next node. ';
      const fragment3 =
          'And a following sentence in the same node to be selected.';
      const splitNodeTree = {
        rootId: 1,
        nodes: [
          {
            id: 1,
            role: 'rootWebArea',
            htmlTag: '#document',
            childIds: [2],
          },
          {
            id: 2,
            role: 'paragraph',
            htmlTag: 'p',
            childIds: [3, 5],
          },
          {
            id: 3,
            role: 'link',
            htmlTag: 'a',
            url: 'http://www.google.com',
            childIds: [4],
          },
          {
            id: 4,
            role: 'staticText',
            name: fragment1,
          },
          {
            id: 5,
            role: 'staticText',
            name: fragment2 + fragment3,
          },
        ],
      };
      selectAndPlay(
          splitNodeTree, 5, fragment2.length + 1, 5,
          fragment2.length + fragment3.length);

      // We shouldn't speak fragment2 even though it's in the same node
      // because the selection only covers fragment 3.
      assertEquals(fragment3, getSpokenText());
    });
  });

  suite('on pause via pause button', () => {
    setup(() => {
      app.speechPlayingState.isSpeechTreeInitialized = true;
      app.speechPlayingState.hasSpeechBeenTriggered = true;
      app.stopSpeech(PauseActionSource.BUTTON_CLICK);
    });

    test('pauses speech', () => {
      assertEquals(1, speech.getCallCount('pause'));
      assertEquals(0, speech.getCallCount('cancel'));
    });

    suite('then play', () => {
      test('with no word boundaries resumes speech', () => {
        app.playSpeech();

        assertEquals(1, speech.getCallCount('resume'));
        assertEquals(0, speech.getCallCount('cancel'));
      });

      test('with word boundaries cancels and re-speaks', () => {
        app.wordBoundaryState.mode = WordBoundaryMode.BOUNDARY_DETECTED;

        app.playSpeech();

        assertGT(speech.getCallCount('speak'), 0);
        assertEquals(1, speech.getCallCount('cancel'));
      });
    });

    test('lock screen stays paused', () => {
      chrome.readingMode.onLockScreen();

      assertEquals(1, speech.getCallCount('pause'));
      assertEquals(0, speech.getCallCount('cancel'));
    });
  });

  test('next granularity plays from there', () => {
    emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);
    assertEquals(paragraph1[1], getSpokenText());
  });

  test('previous granularity plays from there', () => {
    chrome.readingMode.initAxPositionWithNode(2);
    app.playSpeech();
    speech.reset();

    emitEvent(app, ToolbarEvent.PREVIOUS_GRANULARITY);

    assertEquals(paragraph1[0], getSpokenText());
  });

  test(
      'after previous granularity, onstart stops repositioning for speech',
      () => {
        chrome.readingMode.initAxPositionWithNode(2);
        app.playSpeech();

        emitEvent(app, ToolbarEvent.PREVIOUS_GRANULARITY);

        assertTrue(app.speechPlayingState.isSpeechBeingRepositioned);
        app.playSpeech();
        assertFalse(app.speechPlayingState.isSpeechBeingRepositioned);
      });

  test('after next granularity, onstart stops repositioning for speech', () => {
    app.playSpeech();

    emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);

    assertTrue(app.speechPlayingState.isSpeechBeingRepositioned);
    app.playSpeech();
    assertFalse(app.speechPlayingState.isSpeechBeingRepositioned);
  });

  test('interrupt error after next granularity keeps playing speech', () => {
    app.playSpeech();
    speech.reset();

    app.speechPlayingState.isSpeechTreeInitialized = true;
    app.speechPlayingState.isAudioCurrentlyPlaying = true;

    emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);
    assertEquals(1, speech.getCallCount('speak'));
    const utterance = speech.getArgs('speak')[0];
    utterance.onerror(createSpeechErrorEvent(utterance, 'interrupted'));

    assertTrue(app.speechPlayingState.isAudioCurrentlyPlaying);
    assertTrue(app.speechPlayingState.isSpeechActive);

    // Because we triggered onerror in fake_speech_synthesis, onstart was
    // never triggered on the current utterance, so this should still be
    // true after the next button press.
    assertTrue(app.speechPlayingState.isSpeechBeingRepositioned);
  });

  test(
      'interrupt error after previous granularity keeps playing speech', () => {
        chrome.readingMode.initAxPositionWithNode(2);
        app.playSpeech();
        app.speechPlayingState.isSpeechTreeInitialized = true;
        app.speechPlayingState.isAudioCurrentlyPlaying = true;
        speech.reset();

        emitEvent(app, ToolbarEvent.PREVIOUS_GRANULARITY);
        assertEquals(1, speech.getCallCount('speak'));
        const utterance = speech.getArgs('speak')[0];
        utterance.onerror(createSpeechErrorEvent(utterance, 'interrupted'));

        assertTrue(app.speechPlayingState.isAudioCurrentlyPlaying);
        assertTrue(app.speechPlayingState.isSpeechActive);
        // Because we triggered onerror in fake_speech_synthesis, onstart was
        // never triggered on the current utterance, so this should still be
        // true after the previous button press.
        assertTrue(app.speechPlayingState.isSpeechBeingRepositioned);
      });

  test('interrupt error stops speech', async () => {
    app.speechPlayingState.isSpeechTreeInitialized = true;
    app.speechPlayingState.isAudioCurrentlyPlaying = true;
    app.playSpeech();

    assertEquals(1, speech.getCallCount('speak'));
    const utterance = speech.getArgs('speak')[0];
    utterance.onerror(createSpeechErrorEvent(utterance, 'interrupted'));

    assertFalse(app.speechPlayingState.isAudioCurrentlyPlaying);
    assertFalse(app.speechPlayingState.isSpeechActive);
    assertFalse(app.speechPlayingState.isSpeechBeingRepositioned);
    assertEquals(
        chrome.readingMode.engineInterruptStopSource,
        await metrics.whenCalled('recordSpeechStopSource'));
  });

  suite('very long text', () => {
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
    });

    test('uses max speech length', () => {
      const expectedNumSegments =
          Math.ceil(
              longSentences.length / MAX_SPEECH_LENGTH_FOR_REMOTE_VOICES) +
          1;

      app.playSpeech();

      assertGT(expectedNumSegments, 0);
      for (let i = 0; i < expectedNumSegments; i++) {
        assertEquals(i + 1, speech.getCallCount('speak'));
        assertGT(
            MAX_SPEECH_LENGTH_FOR_REMOTE_VOICES,
            speech.getArgs('speak')[i].text.trim().length);
        speech.getArgs('speak')[i].onend();
      }
    });

    test('on text-too-long error smaller text segment plays', () => {
      createAndSetVoices(
          app, speech, [{lang: 'en', name: 'Google Bob', localService: true}]);
      const accessibleTextLength = app.getAccessibleTextLength(longSentences);
      app.playSpeech();
      assertEquals(longSentences, getSpokenText());
      const utterance = speech.getArgs('speak')[0];
      speech.reset();

      utterance.onerror(createSpeechErrorEvent(utterance, 'text-too-long'));

      assertEquals(0, speech.getCallCount('pause'));
      assertEquals(1, speech.getCallCount('cancel'));
      assertEquals(0, metrics.getCallCount('recordSpeechStopSource'));
      const spoken1 = speech.getArgs('speak')[0];
      assertEquals(
          longSentences.substring(0, accessibleTextLength), getSpokenText());
      // When this segment is finished, we should speak the remaining text.
      speech.reset();
      spoken1.onend();
      const spoken2 = speech.getArgs('speak')[0];
      assertEquals(
          longSentences.substring(accessibleTextLength), getSpokenText());

      // There's nothing more to speak.
      speech.reset();
      spoken2.onend();
      assertEquals(0, speech.getCallCount('speak'));
    });
  });

  suite('while playing', () => {
    setup(() => {
      app.speechPlayingState.isSpeechTreeInitialized = true;
      app.speechPlayingState.hasSpeechBeenTriggered = true;
      app.speechPlayingState.isSpeechActive = true;
    });


    test('voice change cancels and restarts speech', () => {
      createAndSetVoices(app, speech, [
        {lang: 'en', name: 'Google Sheldon'},
        {lang: 'en', name: 'Google Mary'},
      ]);
      speech.reset();

      emitEvent(
          app, ToolbarEvent.VOICE,
          {detail: {selectedVoice: speech.getVoices()[1]}});

      assertEquals(2, speech.getCallCount('cancel'));
      assertEquals(1, speech.getCallCount('speak'));
      assertEquals(0, speech.getCallCount('pause'));
    });

    test('rate change cancels and restarts speech', () => {
      emitEvent(app, ToolbarEvent.RATE);

      assertEquals(2, speech.getCallCount('cancel'));
      assertEquals(1, speech.getCallCount('speak'));
      assertEquals(0, speech.getCallCount('pause'));
    });

    test('lock screen cancels speech', () => {
      chrome.readingMode.onLockScreen();

      assertEquals(1, speech.getCallCount('cancel'));
      assertEquals(0, speech.getCallCount('pause'));
      assertEquals(0, speech.getCallCount('speak'));
    });

    test('is playable', async () => {
      await microtasksFinished();
      assertTrue(app.$.toolbar.isReadAloudPlayable);
    });

    test('before utterance.onStarted is not playable', async () => {
      app.playSpeech();
      await microtasksFinished();

      assertFalse(app.$.toolbar.isReadAloudPlayable);
    });

    test('after utterance.onStarted is playable', async () => {
      app.playSpeech();
      assertEquals(1, speech.getCallCount('speak'));
      speech.getArgs('speak')[0].onstart();
      await microtasksFinished();

      assertTrue(app.$.toolbar.isReadAloudPlayable);
    });

    test('selects default voice on language-unavailable', async () => {
      const pageLanguage = 'es';
      assertFalse(pageLanguage === chrome.readingMode.defaultLanguageForSpeech);
      assertFalse(
          app.speechSynthesisLanguage ===
          chrome.readingMode.defaultLanguageForSpeech);
      chrome.readingMode.setLanguageForTesting(pageLanguage);
      app.playSpeech();
      assertEquals(1, speech.getCallCount('speak'));
      const utterance = speech.getArgs('speak')[0];
      speech.reset();

      utterance.onerror(
          createSpeechErrorEvent(utterance, 'language-unavailable'));

      assertEquals(1, speech.getCallCount('cancel'));
      assertEquals(0, speech.getCallCount('pause'));
      assertEquals(0, speech.getCallCount('speak'));
      assertEquals(
          chrome.readingMode.defaultLanguageForSpeech,
          app.speechSynthesisLanguage);
      assertEquals(
          chrome.readingMode.engineErrorStopSource,
          await metrics.whenCalled('recordSpeechStopSource'));
    });

    suite('voice change to unavailable voice', () => {
      let utterance: SpeechSynthesisUtterance;

      setup(() => {
        app.playSpeech();
        assertEquals(1, speech.getCallCount('speak'));
        utterance = speech.getArgs('speak')[0];
      });

      test('cancels and selects default voice', async () => {
        emitEvent(app, ToolbarEvent.VOICE, {
          detail: {
            selectedVoice:
                createSpeechSynthesisVoice({lang: 'en', name: 'Lisie'}),
          },
        });
        speech.reset();

        assertTrue(!!utterance.onerror);
        utterance.onerror(
            createSpeechErrorEvent(utterance, 'voice-unavailable'));

        assertEquals(1, speech.getCallCount('cancel'));
        assertEquals(0, speech.getCallCount('pause'));
        assertEquals(0, speech.getCallCount('speak'));
        assertEquals(speech.getVoices()[0], app.getSpeechSynthesisVoice());
        assertEquals(
            chrome.readingMode.engineErrorStopSource,
            await metrics.whenCalled('recordSpeechStopSource'));
      });

      test('still in getVoices(), cancels and selects another voice', () => {
        chrome.readingMode.setLanguageForTesting('en');
        createAndSetVoices(app, speech, [
          {lang: 'en', name: 'Google George'},
          {lang: 'en', name: 'Google Connie'},
        ]);
        emitEvent(app, ToolbarEvent.VOICE, {
          detail: {selectedVoice: speech.getVoices()[0]},
        });
        speech.reset();

        assertTrue(!!utterance.onerror);
        utterance.onerror(
            createSpeechErrorEvent(utterance, 'voice-unavailable'));

        assertEquals(1, speech.getCallCount('cancel'));
        assertEquals(0, speech.getCallCount('pause'));
        assertEquals(0, speech.getCallCount('speak'));
        assertEquals(speech.getVoices()[1], app.getSpeechSynthesisVoice());
      });

      test(
          'continues to select default voice if no voices available in language',
          () => {
            chrome.readingMode.setLanguageForTesting('elvish');
            emitEvent(app, ToolbarEvent.VOICE, {
              detail: {
                selectedVoice: createSpeechSynthesisVoice(
                    {lang: 'en', name: 'Google Lauren'}),
              },
            });
            speech.reset();

            assertTrue(!!utterance.onerror);
            utterance.onerror(
                createSpeechErrorEvent(utterance, 'voice-unavailable'));

            assertEquals(1, speech.getCallCount('cancel'));
            assertEquals(0, speech.getCallCount('pause'));
            assertEquals(0, speech.getCallCount('speak'));
            assertEquals(speech.getVoices()[0], app.getSpeechSynthesisVoice());
          });
    });

    test('invalid argument cancels and uses default rate', () => {
      app.playSpeech();
      assertEquals(1, speech.getCallCount('speak'));
      const utterance = speech.getArgs('speak')[0];
      let speechRate = 4;
      chrome.readingMode.onSpeechRateChange = rate => {
        speechRate = rate;
      };
      emitEvent(app, ToolbarEvent.VOICE, {
        detail: {
          selectedVoice:
              createSpeechSynthesisVoice({lang: 'en', name: 'Google Lisie'}),
        },
      });
      speech.reset();

      assertTrue(!!utterance.onerror);
      utterance.onerror(createSpeechErrorEvent(utterance, 'invalid-argument'));

      assertEquals(2, speech.getCallCount('cancel'));
      assertEquals(0, speech.getCallCount('pause'));
      assertEquals(1, speech.getCallCount('speak'));
      assertEquals(1, speechRate);
      assertEquals(0, metrics.getCallCount('recordSpeechStopSource'));
    });

    suite('and voice preview is played', () => {
      setup(() => {
        emitEvent(app, 'preview-voice', {detail: {previewVoice: null}});
      });

      test('cancels speech and plays preview', () => {
        assertEquals(1, speech.getCallCount('cancel'));
        assertEquals(0, speech.getCallCount('pause'));
        assertEquals(1, speech.getCallCount('speak'));
      });

      test('then resumes speech after voice menu is closed', () => {
        speech.reset();
        emitEvent(
            app, 'voice-menu-close',
            {detail: {voicePlayingWhenMenuOpened: true}});

        assertEquals(1, speech.getCallCount('cancel'));
        assertEquals(0, speech.getCallCount('pause'));
        assertEquals(1, speech.getCallCount('speak'));
      });
    });
  });
});
