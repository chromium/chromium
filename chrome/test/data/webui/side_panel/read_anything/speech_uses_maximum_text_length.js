// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppReadAloudTest.ReadAloud_SpeechUsesMaximumTextLength

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp = document.querySelector('read-anything-app');
  const container = readAnythingApp.shadowRoot.getElementById('container');
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

  let result = true;

  const assertEquals = (actual, expected) => {
    const isEqual = actual === expected;
    if (!isEqual) {
      console.error(
          'Expected: ' + JSON.stringify(expected) + ', ' +
          'Actual: ' + JSON.stringify(actual));
    }
    result = result && isEqual;
    return isEqual;
  };

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
  chrome.readingMode.setContentForTesting(axTree, [2, 3]);

  const maxSpeechLength = readAnythingApp.maxSpeechLength;

  // Add this check to ensure the text used in this test stays up to date
  // in case the maximum speech length changes.
  assertEquals(shortSentence.length < maxSpeechLength, true);
  assertEquals(longSentence.length > maxSpeechLength, true);
  assertEquals(longSentenceWithFewCommas.length > maxSpeechLength, true);
  assertEquals(longSentenceWithLateComma.length > maxSpeechLength, true);
  assertEquals(longSentenceWithLateComma.indexOf(',') > maxSpeechLength, true);

  const fewCommasFirstIndex =
      readAnythingApp.getAccessibleTextLength(longSentenceWithFewCommas);
  assertEquals(fewCommasFirstIndex < longSentenceWithFewCommas.length, true);

  const fewCommasAfterFirstSplice = longSentenceWithFewCommas.substring(
      fewCommasFirstIndex, longSentenceWithFewCommas.length);
  assertEquals(fewCommasAfterFirstSplice.lastIndexOf(','), 0);

  const fewCommasSecondIndex =
      readAnythingApp.getAccessibleTextLength(fewCommasAfterFirstSplice);
  assertEquals(fewCommasAfterFirstSplice.length > maxSpeechLength, true);
  assertEquals(fewCommasSecondIndex < fewCommasAfterFirstSplice.length, true);

  const fewCommasAfterSecondSplice = fewCommasAfterFirstSplice.substring(
      fewCommasSecondIndex, fewCommasAfterFirstSplice.length);
  // If the only comma in fewCommasAfterSecondSplice was ignored, the
  // two substrings should not be equal. If the comma that the previous
  // segment was spliced on was used, the strings will be equivalent and this
  // test will fail.
  assertEquals(fewCommasAfterFirstSplice === fewCommasAfterSecondSplice, false);

  // Verify that any commas after the maximum speech length aren't used.
  const lateCommasFirstIndex =
      readAnythingApp.getAccessibleTextLength(longSentenceWithLateComma);
  assertEquals(lateCommasFirstIndex < maxSpeechLength, true);

  // Verify that the long sentence is highlighted correctly.
  assertEquals(
      readAnythingApp.getAccessibleTextLength(longSentence) <
          longSentence.length,
      true);

  readAnythingApp.playSpeech();

  readAnythingApp.highlightAndPlayMessage();
  assertEquals(
      container.querySelector('.current-read-highlight').textContent,
      longSentence);

  // TODO(crbug.com/1474951): Expose methods in TypeScript that allow us to
  // test a specific utterance more specifically.

  return result;
})();
