// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {NEXT_GRANULARITY_EVENT, PauseActionSource, PREVIOUS_GRANULARITY_EVENT, RATE_EVENT, WordBoundaryMode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {emitEvent, suppressInnocuousErrors} from './common.js';
import {FakeSpeechSynthesis} from './fake_speech_synthesis.js';

// TODO: b/323960128 - Add tests for word boundaries here or in a
// separate file.
suite('Speech', () => {
  let app: ReadAnythingElement;
  let speechSynthesis: FakeSpeechSynthesis;
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
    'And you won\'t bring me down',
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

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Do not call the real `onConnected()`. As defined in
    // ReadAnythingAppController, onConnected creates mojo pipes to connect to
    // the rest of the Read Anything feature, which we are not testing here.
    chrome.readingMode.onConnected = () => {};

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    // skip highlighting for these tests as we're just focused on what's spoken
    // and the fake speech synthesis causes problems here
    app.highlightNodes = () => {};
    // No need to attempt to log a speech session while using the fake
    // synthesis.
    // @ts-ignore
    app.logSpeechPlaySession = () => {};
    chrome.readingMode.setContentForTesting(axTree, leafIds);
    speechSynthesis = new FakeSpeechSynthesis();
    app.synth = speechSynthesis;

    // @ts-ignore
    app.enabledLanguagesInPref = ['en'];
    app.getSpeechSynthesisVoice();
  });

  suite('on play', () => {
    setup(() => {
      app.playSpeech();
    });

    test('speaks all text by sentences', () => {
      assertEquals(
          speechSynthesis.spokenUtterances.length,
          paragraph1.length + paragraph2.length);
      const utteranceTexts = speechSynthesis.spokenUtterances.map(
          utterance => utterance.text.trim());
      assertTrue(
          paragraph1.every(sentence => utteranceTexts.includes(sentence)));
      assertTrue(
          paragraph2.every(sentence => utteranceTexts.includes(sentence)));
    });

    test('uses set rate', () => {
      let expectedRate = 1;
      assertTrue(
          speechSynthesis.spokenUtterances.every(
              utterance => utterance.rate === expectedRate),
          '1');

      speechSynthesis.clearSpokenUtterances();
      expectedRate = 1.5;
      emitEvent(app, RATE_EVENT, {detail: {rate: expectedRate}});
      app.playSpeech();

      assertTrue(
          speechSynthesis.spokenUtterances.every(
              utterance => utterance.rate === expectedRate),
          '1.5');

      speechSynthesis.clearSpokenUtterances();
      expectedRate = 4;
      emitEvent(app, RATE_EVENT, {detail: {rate: expectedRate}});
      app.playSpeech();

      assertTrue(
          speechSynthesis.spokenUtterances.every(
              utterance => utterance.rate === expectedRate),
          '4');
    });

    test('uses set language', () => {
      // no need to update fonts for this test
      app.$.toolbar.updateFonts = () => {};

      let expectedLang = 'en';
      assertTrue(
          speechSynthesis.spokenUtterances.every(
              utterance => utterance.lang === expectedLang),
          '1');

      speechSynthesis.clearSpokenUtterances();
      expectedLang = 'fr';
      chrome.readingMode.setLanguageForTesting(expectedLang);
      app.playSpeech();

      assertTrue(
          speechSynthesis.spokenUtterances.every(
              utterance => utterance.lang === expectedLang),
          '1.5');

      speechSynthesis.clearSpokenUtterances();
      expectedLang = 'zh';
      chrome.readingMode.setLanguageForTesting(expectedLang);
      app.playSpeech();

      assertTrue(
          speechSynthesis.spokenUtterances.every(
              utterance => utterance.lang === expectedLang),
          '4');
    });
  });


  suite('on pause via pause button', () => {
    setup(() => {
      chrome.readingMode.initAxPositionWithNode(2);
      app.speechPlayingState.speechStarted = true;
      app.stopSpeech(PauseActionSource.BUTTON_CLICK);
    });

    test('pauses speech', () => {
      assertTrue(speechSynthesis.paused);
      assertFalse(speechSynthesis.canceled);
    });

    suite('then play', () => {
      test('with no word boundaries resumes speech', () => {
        app.playSpeech();

        assertTrue(speechSynthesis.speaking);
        assertFalse(speechSynthesis.canceled);
      });

      test('with word boundaries cancels and re-speaks', () => {
        app.wordBoundaryState.mode = WordBoundaryMode.BOUNDARY_DETECTED;

        app.playSpeech();

        assertGT(speechSynthesis.spokenUtterances.length, 0);
        assertTrue(speechSynthesis.canceled);
      });
    });

    test('lock screen stays paused', () => {
      chrome.readingMode.onLockScreen();
      assertFalse(speechSynthesis.canceled);
      assertTrue(speechSynthesis.paused);
    });
  });

  test('next granularity plays from there', () => {
    chrome.readingMode.initAxPositionWithNode(2);
    const expectedNumSentences = paragraph1.length + paragraph2.length - 1;

    emitEvent(app, NEXT_GRANULARITY_EVENT);

    assertEquals(speechSynthesis.spokenUtterances.length, expectedNumSentences);
    const utteranceTexts = speechSynthesis.spokenUtterances.map(
        utterance => utterance.text.trim());
    assertFalse(utteranceTexts.includes(paragraph1[0]!));
    assertTrue(paragraph2.every(sentence => utteranceTexts.includes(sentence)));
  });

  test('previous granularity plays from there', () => {
    chrome.readingMode.initAxPositionWithNode(2);
    app.playSpeech();
    speechSynthesis.clearSpokenUtterances();

    emitEvent(app, PREVIOUS_GRANULARITY_EVENT);

    assertEquals(speechSynthesis.spokenUtterances.length, 1);
    assertEquals(speechSynthesis.spokenUtterances[0]!.text, paragraph2.at(-1)!);
  });

  test('very long text uses max speech length', () => {
    const longSentences =
        'A kingdom of isolation, and it looks like I am the queen and the ' +
        'wind is howling like this swirling storm inside, Couldn\t keep it ' +
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
    const leafs = [2];
    const longTree = {
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
          role: 'staticText',
          name: longSentences,
        },
      ],
    };
    const expectedNumSegments =
        Math.ceil(longSentences.length / app.maxSpeechLength);

    chrome.readingMode.setContentForTesting(longTree, leafs);
    app.playSpeech();

    assertEquals(speechSynthesis.spokenUtterances.length, expectedNumSegments);
    const spoken =
        speechSynthesis.spokenUtterances.map(utterance => utterance.text)
            .join('');
    assertEquals(spoken, longSentences);
  });

  suite('while playing', () => {
    setup(() => {
      chrome.readingMode.initAxPositionWithNode(2);
      app.speechPlayingState.speechStarted = true;
      app.speechPlayingState.paused = false;
    });


    test('voice change cancels and restarts speech', () => {
      chrome.readingMode.onVoiceChange = () => {};
      emitEvent(
          app, 'select-voice',
          {detail: {selectedVoice: speechSynthesis.getVoices()[1]}});

      assertGT(speechSynthesis.spokenUtterances.length, 0);
      assertTrue(speechSynthesis.canceled);
      assertFalse(speechSynthesis.paused);
    });

    test('rate change cancels and restarts speech', () => {
      emitEvent(app, RATE_EVENT, {detail: {rate: 0.8}});

      assertGT(speechSynthesis.spokenUtterances.length, 0);
      assertTrue(speechSynthesis.canceled);
      assertFalse(speechSynthesis.paused);
    });

    test('lock screen cancels speech', () => {
      chrome.readingMode.onLockScreen();
      assertTrue(speechSynthesis.canceled);
      assertFalse(speechSynthesis.paused);
    });

    suite('isReadAloudPlayable updates', () => {
      setup(() => {
        assertTrue(app.isReadAloudPlayable());
      });
      test('before utterance.onStarted', () => {
        app.playSpeech();
        assertFalse(app.isReadAloudPlayable());
      });
      test('after utterance.onStarted', () => {
        speechSynthesis.triggerUtteranceStartedOnNextSpeak();
        app.playSpeech();
        assertTrue(app.isReadAloudPlayable());
      });
    });

    suite('language change to unavailable language', () => {
      const pageLanguage = 'es';
      setup(() => {
        speechSynthesis.triggerErrorEventOnNextSpeak('language-unavailable');
        chrome.readingMode.onVoiceChange = () => {};
        app.$.toolbar.updateFonts = () => {};
        assertFalse(
            pageLanguage === chrome.readingMode.defaultLanguageForSpeech);
        assertFalse(
            app.speechSynthesisLanguage ===
            chrome.readingMode.defaultLanguageForSpeech);
        chrome.readingMode.setLanguageForTesting(pageLanguage);
        app.playSpeech();
      });

      test('selects default voice', () => {
        assertFalse(speechSynthesis.speaking);
        assertTrue(speechSynthesis.canceled);
        assertFalse(speechSynthesis.paused);
        assertEquals(
            app.speechSynthesisLanguage,
            chrome.readingMode.defaultLanguageForSpeech);
      });
    });

    suite('voice change to unavailable voice', () => {
      setup(() => {
        speechSynthesis.triggerErrorEventOnNextSpeak('voice-unavailable');
        chrome.readingMode.onVoiceChange = () => {};
      });

      test('cancels and selects default voice', () => {
        emitEvent(app, 'select-voice', {
          detail: {
            selectedVoice: {lang: 'en', name: 'Lisie'} as SpeechSynthesisVoice,
          },
        });

        assertFalse(speechSynthesis.speaking);
        assertTrue(speechSynthesis.canceled);
        assertFalse(speechSynthesis.paused);
        assertEquals(
            app.getSpeechSynthesisVoice()?.name,
            speechSynthesis.getVoices()[0]?.name);
      });

      test(
          'with voice still in getVoices() cancels and selects another voice',
          () => {
            // Updating the language triggers a font update, which is unneeded
            // for this test.
            app.$.toolbar.updateFonts = () => {};
            chrome.readingMode.setLanguageForTesting('en');
            emitEvent(app, 'select-voice', {
              detail: {
                selectedVoice: {lang: 'en', name: 'Lauren', default:true} as
                    SpeechSynthesisVoice,
              },
            });

            assertFalse(speechSynthesis.speaking);
            assertTrue(speechSynthesis.canceled);
            assertFalse(speechSynthesis.paused);
            assertEquals(
                app.getSpeechSynthesisVoice()?.name,
                speechSynthesis.getVoices()[1]?.name);
          });

      test(
          'continues to select default voice if no voices available in language',
          () => {
            // Updating the language triggers a font update, which is unneeded
            // for this test.
            app.$.toolbar.updateFonts = () => {};
            chrome.readingMode.setLanguageForTesting('elvish');

            emitEvent(app, 'select-voice', {
              detail: {
                selectedVoice: {lang: 'en', name: 'Lauren'} as
                    SpeechSynthesisVoice,
              },
            });

            assertFalse(speechSynthesis.speaking);
            assertTrue(speechSynthesis.canceled);
            assertFalse(speechSynthesis.paused);
            assertEquals(
                app.getSpeechSynthesisVoice()?.name,
                speechSynthesis.getVoices()[0]?.name);
          });
    });

    suite('and voice preview is played', () => {
      setup(() => {
        emitEvent(app, 'preview-voice', {detail: {previewVoice: null}});
      });

      test('cancels speech and plays preview', () => {
        assertTrue(speechSynthesis.canceled, 'canceled');
        assertTrue(speechSynthesis.speaking, 'speaking');
        assertFalse(speechSynthesis.paused, 'paused');
        assertEquals(speechSynthesis.spokenUtterances.length, 1);
      });

      test('then resumes speech after voice menu is closed', () => {
        speechSynthesis.clearSpokenUtterances();

        emitEvent(
            app, 'voice-menu-close',
            {detail: {voicePlayingWhenMenuOpened: true}});

        assertTrue(speechSynthesis.canceled);
        assertFalse(speechSynthesis.paused);
        assertEquals(
            speechSynthesis.spokenUtterances.length,
            paragraph1.length + paragraph2.length);
      });
    });
  });
});
