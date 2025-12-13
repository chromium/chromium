// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ContentController, MAX_SPEECH_LENGTH, SpeechBrowserProxyImpl, SpeechController, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertGT} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createApp, createSpeechSynthesisVoice, emitEvent} from './common.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('SpeechUsesMaxTextLength', () => {
  let app: AppElement;
  let maxSpeechLength: number;
  let speech: TestSpeechBrowserProxy;
  let speechController: SpeechController;

  const longSentence =
      'A kingdom of isolation, and it looks like I am the queen and the ' +
      'wind is howling like this swirling storm inside, Couldn\t keep it ' +
      'in, heaven knows I tried, but don\'t let them in, don\'t let them ' +
      'see, be the good girl you always have to be, and conceal, don\'t ' +
      'feel, don\'t let them know.';

  // A text segment with no commas occurring after the first splice of text.
  const longSentenceWithFewCommas =
      'Well, now they know, let it go, let it go, can\'t hold it back ' +
      'anymore, let it go, let it go, turn away and slam the ' +
      'door- I don\'t care what they\'re going to say, let the storm rage ' +
      'on- the cold never bothered me anyway- it\'s funny how some distance ' +
      'makes everything seem small and the fears that once controlled me ' +
      'can\'t get to me at all- it\'s time to see what I can do to test the ' +
      'limits and break through- no right no wrong no rules for me- I\'m ' +
      'free- let it go let it go I am one with the wind and sky let it go ' +
      'let it go you\'ll never see me cry- here I stand and here I stay- ' +
      'let the storm rage on';

  // A text segment with no commas occurring before the first splice of text.
  const longSentenceWithLateComma = 'my power flurries through the air ' +
      'into the ground- my soul is spiraling in frozen fractals all ' +
      'around and one thought crystallizes like an icy blast I\'m never ' +
      'going back- the past is in the past- let it go let it go and I\'ll ' +
      'rise like the break of dawn- let it go, let it go, that perfect ' +
      'girl is gone- here I stand in the light of day, let the storm rage ' +
      'on- the cold never bothered me anyway';

  const localVoice = createSpeechSynthesisVoice(
      {lang: 'en', name: 'Google Raccoon', localService: true});
  const remoteVoice = createSpeechSynthesisVoice(
      {lang: 'en', name: 'Google Red Panda', localService: false});

  function setContentWithText(text: string) {
    const axTree = {
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
          name: text,
        },
      ],
    };

    chrome.readingMode.setContentForTesting(axTree, [2]);
  }

  async function assertSpeaksWithNumSegments(expectedNumSegments: number) {
    assertGT(expectedNumSegments, 0);
    for (let i = 0; i < expectedNumSegments; i++) {
      console.error('waiting for speak', i);
      const spoken = await speech.whenCalled('speak');
      assertGT(maxSpeechLength, spoken.text.length);
      speech.reset();
      spoken.onend();
    }

    assertEquals(0, speech.getCallCount('speak'));
  }

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Do not call the real `onConnected()`. As defined in
    // ReadAnythingAppController, onConnected creates mojo pipes to connect to
    // the rest of the Read Anything feature, which we are not testing here.
    chrome.readingMode.onConnected = () => {};
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
    ContentController.setInstance(new ContentController());

    app = await createApp();
    maxSpeechLength = MAX_SPEECH_LENGTH;
  });

  // These checks ensure the text used in this test stays up to date
  // in case the maximum speech length changes.
  suite('compared to max speech length', () => {
    test('long sentences are longer', () => {
      assertGT(longSentence.length, maxSpeechLength);
      assertGT(longSentenceWithFewCommas.length, maxSpeechLength);
      assertGT(longSentenceWithLateComma.length, maxSpeechLength);
    });

    test('first comma of late comma sentence is later', () => {
      assertGT(longSentenceWithLateComma.indexOf(','), maxSpeechLength);
    });
  });

  test('long sentence with local voice speaks full sentence', async () => {
    setContentWithText(longSentence);
    emitEvent(app, ToolbarEvent.VOICE, {detail: {selectedVoice: localVoice}});

    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const spoken = await speech.whenCalled('speak');
    assertEquals(longSentence, spoken.text);
  });

  test('long sentence with remote voice uses max length', () => {
    setContentWithText(longSentence);
    const expectedNumSegments =
        Math.ceil(longSentence.length / maxSpeechLength);
    emitEvent(app, ToolbarEvent.VOICE, {detail: {selectedVoice: remoteVoice}});

    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    assertSpeaksWithNumSegments(expectedNumSegments);
  });

  test(
      'long sentence with few commas with local voice speaks full sentence',
      async () => {
        setContentWithText(longSentenceWithFewCommas);
        emitEvent(
            app, ToolbarEvent.VOICE, {detail: {selectedVoice: localVoice}});

        emitEvent(app, ToolbarEvent.PLAY_PAUSE);

        const spoken = await speech.whenCalled('speak');
        assertEquals(longSentenceWithFewCommas, spoken.text);
      });

  test(
      'long sentence with few commas with remote voice uses last comma and ' +
          'then max length',
      async () => {
        const lastComma =
            longSentenceWithFewCommas.substring(0, maxSpeechLength)
                .lastIndexOf(', ');
        const expectedFirstText =
            longSentenceWithFewCommas.substring(0, lastComma);
        const remainingText = longSentenceWithFewCommas.substring(lastComma);
        const expectedNumSegments =
            Math.ceil(remainingText.length / maxSpeechLength);
        setContentWithText(longSentenceWithFewCommas);
        emitEvent(
            app, ToolbarEvent.VOICE, {detail: {selectedVoice: remoteVoice}});

        emitEvent(app, ToolbarEvent.PLAY_PAUSE);

        const spoken1 = await speech.whenCalled('speak');
        assertEquals(expectedFirstText, spoken1.text);
        speech.reset();
        spoken1.onend();
        assertSpeaksWithNumSegments(expectedNumSegments);
      });

  test(
      'long sentence with late commas with local voice speaks full sentence',
      async () => {
        setContentWithText(longSentenceWithLateComma);
        emitEvent(
            app, ToolbarEvent.VOICE, {detail: {selectedVoice: localVoice}});

        emitEvent(app, ToolbarEvent.PLAY_PAUSE);

        const spoken = await speech.whenCalled('speak');
        assertEquals(longSentenceWithLateComma, spoken.text);
      });

  test(
      'long sentence with late commas with remote voice uses comma before max' +
          ' length',
      () => {
        // Since there are no commas within the first `maxSpeechLength`
        // characters, this will break on a word boundary. It's hard to know
        // exactly how many segments there will be, so just verify it's more
        // than one.
        const expectedNumSegments =
            Math.ceil(longSentenceWithLateComma.length / maxSpeechLength);
        setContentWithText(longSentenceWithLateComma);
        emitEvent(
            app, ToolbarEvent.VOICE, {detail: {selectedVoice: remoteVoice}});

        emitEvent(app, ToolbarEvent.PLAY_PAUSE);

        assertSpeaksWithNumSegments(expectedNumSegments);
      });
});
