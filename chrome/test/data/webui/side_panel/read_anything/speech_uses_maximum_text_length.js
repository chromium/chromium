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

  // Add this check to ensure the text used in this test stays up to date
  // in case the maximum speech length changes.
  assertEquals(shortSentence.length < readAnythingApp.maxSpeechLength, true);
  assertEquals(longSentence.length > readAnythingApp.maxSpeechLength, true);

  assertEquals(
      readAnythingApp.getAccessibleTextLength(longSentence) <
          longSentence.length,
      true);

  readAnythingApp.playSpeech();

  readAnythingApp.highlightAndPlayNextMessage();
  assertEquals(
      container.querySelector('.current-read-highlight').textContent,
      longSentence);

  // TODO(crbug.com/1474951): Expose methods in TypeScript that allow us to
  // test a specific utterance more specifically.

  return result;
})();
