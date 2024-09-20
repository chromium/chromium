// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {PauseActionSource, ToolbarEvent, WordBoundaryMode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createSpeechSynthesisVoice, emitEvent, setSimpleAxTreeWithText, suppressInnocuousErrors, waitForPlayFromSelection} from './common.js';
import {FakeSpeechSynthesis} from './fake_speech_synthesis.js';

// TODO: b/323960128 - Add tests for word boundaries here or in a
// separate file.
suite('Speech', () => {
  let app: AppElement;
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
    'And you won\'t bring me down.',
  ];

  const leafIds = [3, 5];
  const totalSentences = paragraph1.length + paragraph2.length;
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

  function getSpokenTexts(): string[] {
    return speechSynthesis.spokenUtterances.map(
        utterance => utterance.text.trim());
  }

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
    app.highlightCurrentGranularity = () => {};
    chrome.readingMode.setContentForTesting(axTree, leafIds);
    speechSynthesis = new FakeSpeechSynthesis();
    app.synth = speechSynthesis;
    speechSynthesis.setDefaultVoices();

    app.enabledLangs = ['en'];
    app.getSpeechSynthesisVoice();

    return microtasksFinished();
  });

  suite('on play', () => {
    setup(() => {
      app.playSpeech();
      return microtasksFinished();
    });

    test('speaks all text by sentences', () => {
      assertEquals(totalSentences, speechSynthesis.spokenUtterances.length);
      const utteranceTexts = getSpokenTexts();
      assertTrue(
          paragraph1.every(sentence => utteranceTexts.includes(sentence)));
      assertTrue(
          paragraph2.every(sentence => utteranceTexts.includes(sentence)));
    });

    test('uses set language', async () => {
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
      await microtasksFinished();

      assertTrue(
          speechSynthesis.spokenUtterances.every(
              utterance => utterance.lang === expectedLang),
          '1.5');

      speechSynthesis.clearSpokenUtterances();
      expectedLang = 'zh';
      chrome.readingMode.setLanguageForTesting(expectedLang);
      app.playSpeech();
      await microtasksFinished();

      assertTrue(
          speechSynthesis.spokenUtterances.every(
              utterance => utterance.lang === expectedLang),
          '4');
    });
  });

  suite('with text selected', () => {
    async function selectAndPlay(
        baseTree: any, anchorId: number, anchorOffset: number, focusId: number,
        focusOffset: number, isBackward: boolean = false): Promise<void> {
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
      app.playSpeech();
      return waitForPlayFromSelection();
    }

    test('first play starts from selected node', async () => {
      await selectAndPlay(axTree, 5, 0, 5, 7);

      const utteranceTexts = getSpokenTexts();
      assertEquals(totalSentences - paragraph1.length, utteranceTexts.length);
      assertTrue(
          paragraph2.every(sentence => utteranceTexts.includes(sentence)));
    });

    test('selection is cleared after play', async () => {
      await selectAndPlay(axTree, 5, 0, 5, 10);
      assertEquals('None', app.getSelection().type);
    });

    test(
        'when selection starts in middle of node, play from beginning of node',
        async () => {
          await selectAndPlay(axTree, 5, 10, 5, 20);

          const utteranceTexts = getSpokenTexts();
          assertEquals(
              totalSentences - paragraph1.length, utteranceTexts.length);
          assertTrue(
              paragraph2.every(sentence => utteranceTexts.includes(sentence)));
        });

    test('when selection crosses nodes, play from earlier node', async () => {
      await selectAndPlay(axTree, 3, 10, 5, 10);

      const utteranceTexts = getSpokenTexts();
      assertEquals(totalSentences, utteranceTexts.length);
      assertTrue(
          paragraph1.every(sentence => utteranceTexts.includes(sentence)));
      assertTrue(
          paragraph2.every(sentence => utteranceTexts.includes(sentence)));
    });

    test('when selection is backward, play from earlier node', async () => {
      await selectAndPlay(axTree, 5, 10, 3, 10, /*isBackward=*/ true);

      const utteranceTexts = getSpokenTexts();
      assertEquals(totalSentences, utteranceTexts.length);
      assertTrue(
          paragraph1.every(sentence => utteranceTexts.includes(sentence)));
      assertTrue(
          paragraph2.every(sentence => utteranceTexts.includes(sentence)));
    });

    test(
        'after speech started, cancels speech and plays from selection',
        async () => {
          app.speechPlayingState.isSpeechTreeInitialized = true;
          app.speechPlayingState.hasSpeechBeenTriggered = true;

          await selectAndPlay(axTree, 5, 0, 5, 10);

          assertTrue(speechSynthesis.canceled);
          const utteranceTexts = getSpokenTexts();
          assertEquals(
              totalSentences - paragraph1.length, utteranceTexts.length);
          assertTrue(
              paragraph2.every(sentence => utteranceTexts.includes(sentence)));
        });

    test('play from selection when node split across sentences', async () => {
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
      await selectAndPlay(
          splitNodeTree, 5, fragment2.length + 1, 5,
          fragment2.length + fragment3.length);

      const utteranceTexts = getSpokenTexts();
      // We shouldn't speak fragment2 even though it's in the same node
      // because the selection only covers fragment 3.
      assertEquals(1, utteranceTexts.length);
      assertTrue(utteranceTexts.includes(fragment3));
    });
  });

  suite('on pause via pause button', () => {
    setup(() => {
      chrome.readingMode.initAxPositionWithNode(2);
      app.speechPlayingState.isSpeechTreeInitialized = true;
      app.speechPlayingState.hasSpeechBeenTriggered = true;
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

  test('next granularity plays from there', async () => {
    chrome.readingMode.initAxPositionWithNode(2);
    const expectedNumSentences = totalSentences - 1;

    emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);
    await microtasksFinished();

    assertEquals(expectedNumSentences, speechSynthesis.spokenUtterances.length);
    const utteranceTexts = getSpokenTexts();
    assertFalse(utteranceTexts.includes(paragraph1[0]!));
    assertTrue(paragraph2.every(sentence => utteranceTexts.includes(sentence)));
  });

  test('previous granularity plays from there', async () => {
    speechSynthesis.setMaxSegments(7);
    chrome.readingMode.initAxPositionWithNode(2);
    app.playSpeech();
    speechSynthesis.clearSpokenUtterances();

    speechSynthesis.setMaxSegments(1);
    emitEvent(app, ToolbarEvent.PREVIOUS_GRANULARITY);
    await microtasksFinished();

    assertEquals(1, speechSynthesis.spokenUtterances.length);
    assertEquals(
        paragraph2.at(-2)!, speechSynthesis.spokenUtterances[0]!.text.trim());
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
          Math.ceil(longSentences.length / app.maxSpeechLength);

      app.playSpeech();

      assertEquals(
          expectedNumSegments, speechSynthesis.spokenUtterances.length);
      const spoken =
          speechSynthesis.spokenUtterances.map(utterance => utterance.text)
              .join('');
      assertEquals(longSentences, spoken);
    });

    test('on text-too-long error smaller text segment plays', () => {
      // Remote voices already reduce the size of a speech segment to avoid
      // the bug where speech stops without an error callback.
      speechSynthesis.useLocalVoices();
      speechSynthesis.setDefaultVoices();
      chrome.readingMode.onVoiceChange = () => {};
      emitEvent(
          app, 'select-voice',
          {detail: {selectedVoice: speechSynthesis.getVoices()[5]}});

      speechSynthesis.triggerErrorEventOnNextSpeak('text-too-long');
      app.playSpeech();

      assertFalse(speechSynthesis.speaking);
      assertTrue(speechSynthesis.canceled);
      assertFalse(speechSynthesis.paused);
      assertEquals(3, speechSynthesis.spokenUtterances.length);

      // The first utterance should contain the entire text, but it should
      // be canceled. The second utterance should be the smaller text
      // segment after receiving the text-too-long error. The third utterance
      // should be the remaining text, as there is no longer a text-too-long
      // error triggered.
      const accessibleTextLength = app.getAccessibleTextLength(longSentences);
      assertEquals(longSentences, speechSynthesis.spokenUtterances[0]!.text);
      assertEquals(
          longSentences.substring(0, accessibleTextLength),
          speechSynthesis.spokenUtterances[1]!.text);
      assertEquals(
          longSentences.substring(accessibleTextLength),
          speechSynthesis.spokenUtterances[2]!.text);
      assertEquals(1, speechSynthesis.canceledUtterances.length);
      assertEquals(
          speechSynthesis.spokenUtterances[0]!,
          speechSynthesis.canceledUtterances[0]!);
    });
  });

  suite('while playing', () => {
    setup(() => {
      chrome.readingMode.initAxPositionWithNode(2);
      app.speechPlayingState.isSpeechTreeInitialized = true;
      app.speechPlayingState.hasSpeechBeenTriggered = true;
      app.speechPlayingState.isSpeechActive = true;
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
      emitEvent(app, ToolbarEvent.RATE);

      assertGT(speechSynthesis.spokenUtterances.length, 0);
      assertTrue(speechSynthesis.canceled);
      assertFalse(speechSynthesis.paused);
    });

    test('lock screen cancels speech', () => {
      chrome.readingMode.onLockScreen();
      assertTrue(speechSynthesis.canceled);
      assertFalse(speechSynthesis.paused);
    });

    test('is playable', () => {
      assertTrue(app.$.toolbar.isReadAloudPlayable);
    });

    test('before utterance.onStarted is not playable', async () => {
      app.playSpeech();
      await microtasksFinished();

      assertFalse(app.$.toolbar.isReadAloudPlayable);
    });

    test('after utterance.onStarted is playable', async () => {
      speechSynthesis.triggerUtteranceStartedOnNextSpeak();
      app.playSpeech();
      await microtasksFinished();
      assertTrue(app.$.toolbar.isReadAloudPlayable);
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
        return microtasksFinished();
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

      test('cancels and selects default voice', async () => {
        emitEvent(app, 'select-voice', {
          detail: {
            selectedVoice:
                createSpeechSynthesisVoice({lang: 'en', name: 'Lisie'}),
          },
        });
        await microtasksFinished();

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
                selectedVoice: createSpeechSynthesisVoice(
                    {lang: 'en', name: 'Google Lauren', default: true}),
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
                selectedVoice: createSpeechSynthesisVoice(
                    {lang: 'en', name: 'Google Lauren'}),
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

    suite('invalid argument', () => {
      setup(() => {
        speechSynthesis.triggerErrorEventOnNextSpeak('invalid-argument');
      });

      test('cancels and uses default rate', () => {
        let speechRate = 4;
        chrome.readingMode.onSpeechRateChange = rate => {
          speechRate = rate;
        };
        emitEvent(app, 'select-voice', {
          detail: {
            selectedVoice:
                createSpeechSynthesisVoice({lang: 'en', name: 'Google Lisie'}),
          },
        });

        assertFalse(speechSynthesis.speaking);
        assertTrue(speechSynthesis.canceled);
        assertFalse(speechSynthesis.paused);
        assertEquals(1, speechRate);
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
        assertEquals(1, speechSynthesis.spokenUtterances.length);
      });

      test('then resumes speech after voice menu is closed', () => {
        speechSynthesis.clearSpokenUtterances();

        emitEvent(
            app, 'voice-menu-close',
            {detail: {voicePlayingWhenMenuOpened: true}});

        assertTrue(speechSynthesis.canceled);
        assertFalse(speechSynthesis.paused);
        assertEquals(totalSentences, speechSynthesis.spokenUtterances.length);
      });
    });
  });
});
