// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertGT, assertLT, assertNotEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';

suite('SpeechUsesMaxTextLength', () => {
  let app: AppElement;
  let maxSpeechLength: number;

  const shortSentence =
      'The snow glows white on the mountain tonight, not a footprint to be ' +
      'seen. ';
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

  // The page needs some text to start speaking
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 3],
      },
      {
        id: 2,
        role: 'staticText',
        name: longSentence,
      },
      {
        id: 3,
        role: 'staticText',
        name: shortSentence,
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
    maxSpeechLength = app.maxSpeechLength;
  });
  // These checks ensure the text used in this test stays up to date
  // in case the maximum speech length changes.
  suite('compared to max speech length', () => {
    test('short sentence is shorter', () => {
      assertLT(shortSentence.length, maxSpeechLength);
    });

    test('long sentences are longer', () => {
      assertGT(longSentence.length, maxSpeechLength);
      assertGT(longSentenceWithFewCommas.length, maxSpeechLength);
      assertGT(longSentenceWithLateComma.length, maxSpeechLength);
    });

    test('first comma of late comma sentence is later', () => {
      assertGT(longSentenceWithLateComma.indexOf(','), maxSpeechLength);
    });
  });

  suite('on long sentence', () => {
    test('accessible text boundary is before max speech length', () => {
      const firstBoundary = app.getAccessibleTextLength(longSentence);
      assertLT(firstBoundary, maxSpeechLength);
    });

    test('highlights full sentence', () => {
      chrome.readingMode.setContentForTesting(axTree, [2, 3]);
      app.playSpeech();
      app.highlightAndPlayMessage();

      assertEquals(
          app.$.container.querySelector('.current-read-highlight')!.textContent,
          longSentence);
    });
  });

  suite('on long sentence with few commas', () => {
    let firstBoundary: number;

    setup(() => {
      firstBoundary = app.getAccessibleTextLength(longSentenceWithFewCommas);
    });


    test('first accessible text boundary is at last comma', () => {
      assertLT(firstBoundary, longSentenceWithFewCommas.length);
      assertEquals(firstBoundary, longSentenceWithFewCommas.lastIndexOf(','));
    });

    test('next accessible text boundary is before end of string', () => {
      const afterFirstBoundary = longSentenceWithFewCommas.substring(
          firstBoundary, longSentenceWithFewCommas.length);
      const secondBoundary = app.getAccessibleTextLength(afterFirstBoundary);
      const afterSecondBoundary = longSentenceWithFewCommas.substring(
          secondBoundary, longSentenceWithFewCommas.length);

      assertGT(afterFirstBoundary.length, maxSpeechLength);
      assertLT(secondBoundary, afterFirstBoundary.length);
      assertNotEquals(afterSecondBoundary, afterFirstBoundary);
    });
  });

  suite('on long sentence with commas after max speech length', () => {
    let firstBoundary: number;

    setup(() => {
      firstBoundary = app.getAccessibleTextLength(longSentenceWithLateComma);
    });


    test('commas after max speech length aren\'t used', () => {
      const afterFirstBoundary = longSentenceWithFewCommas.substring(
          firstBoundary, longSentenceWithFewCommas.length);
      const secondBoundary = app.getAccessibleTextLength(afterFirstBoundary);
      const afterSecondBoundary = longSentenceWithFewCommas.substring(
          firstBoundary, longSentenceWithFewCommas.length);
      const thirdBoundary = app.getAccessibleTextLength(afterSecondBoundary);

      assertLT(firstBoundary, maxSpeechLength);
      assertLT(secondBoundary, maxSpeechLength);
      assertLT(thirdBoundary, maxSpeechLength);
    });
  });
});
