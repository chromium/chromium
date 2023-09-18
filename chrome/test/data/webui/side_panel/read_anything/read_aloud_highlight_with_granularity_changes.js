// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppReadAloudTest.
//      ReadAloud_GranularityChangesUpdatesHighlight

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp = document.querySelector('read-anything-app');
  const container = readAnythingApp.shadowRoot.getElementById('container');

  const sentence1 = 'The snow glows white on the mountain tonight.';
  const sentence2 = 'Not a footprint to be seen';
  const sentence3 = 'A kingdom of isolation.';
  const sentence4 = 'And it looks like I\'m the queen.';
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 3, 4, 5],
      },
      {
        id: 2,
        role: 'staticText',
        name: sentence1,
      },
      {
        id: 3,
        role: 'staticText',
        name: sentence2,
      },
      {
        id: 4,
        role: 'staticText',
        name: sentence3,
      },
      {
        id: 5,
        role: 'staticText',
        name: sentence4,
      },
    ],
  };
  chrome.readingMode.setContentForTesting(axTree, [2, 3, 4, 5]);

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

  // Speech doesn't actually run in tests, so manually call start
  readAnythingApp.playSpeech();
  utterances = readAnythingApp.getUtterancesToSpeak();
  assertEquals(utterances.length, 4);

  // First sentence is highlighted and nothing is before it
  let utterance = utterances[0];
  assertEquals(utterance.text, sentence1);
  utterance.onstart();
  assertEquals(
      container.querySelector('.current-read-highlight').textContent,
      sentence1);
  assertEquals(
      container.querySelectorAll('.previous-read-highlight').length, 0);
  utterance.onend();

  // Third sentence is highlighted and first is before it
  readAnythingApp.playNextGranularity();
  utterance = readAnythingApp.getCurrentUtterance();
  assertEquals(utterance.text, sentence3);
  utterance.onstart();
  assertEquals(
      container.querySelector('.current-read-highlight').textContent,
      sentence3);

  // TODO(crbug.com/1474951): Add tests for the previous highlight and for the
  //  the highlight when calling playPreviousGranularity.

  // Fourth sentence is next.
  readAnythingApp.playNextGranularity();
  utterance = readAnythingApp.getCurrentUtterance();
  assertEquals(utterance.text, sentence4);

  // Third sentence is now the current utterance.
  readAnythingApp.playPreviousGranularity();
  utterance = readAnythingApp.getCurrentUtterance();
  assertEquals(utterance.text, sentence3);

  // Fourth sentence is now the current utterance again.
  readAnythingApp.playNextGranularity();
  utterance = readAnythingApp.getCurrentUtterance();
  assertEquals(utterance.text, sentence4);
  utterance.onstart();
  assertEquals(
      container.querySelector('.current-read-highlight').textContent,
      sentence4);

  // Attempt to speak the previous granularity before the beginning of the text.
  for (let i = 0; i < utterances.length + 1; i++) {
    readAnythingApp.playPreviousGranularity();
  }

  // Even though call playPreviousGranularity more than the number of
  // utterances, the next utterance is stopped at the first sentence.
  utterance = readAnythingApp.getCurrentUtterance();
  assertEquals(utterance.text, sentence1);

  // Attempt to speak more utterance than are left.
  for (let i = 0; i < utterances.length + 1; i++) {
    readAnythingApp.playNextGranularity();
  }

  // When all text has been cycled through, speech is stopped.
  utterances = readAnythingApp.getUtterancesToSpeak();
  assertEquals(utterances.length, 0);

  // After speech is stopped, playing the next / previous granularity cause no
  // crashes, and don't change utterances.
  readAnythingApp.playNextGranularity();
  readAnythingApp.playPreviousGranularity();
  utterances = readAnythingApp.getUtterancesToSpeak();
  assertEquals(utterances.length, 0);

  return result;
})();
