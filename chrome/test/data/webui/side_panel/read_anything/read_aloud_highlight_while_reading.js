// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppReadAloudTest.ReadAloud_HighlightWhileReading

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp = document.querySelector('read-anything-app');
  const container = readAnythingApp.shadowRoot.getElementById('container');

  const sentence1 = 'Only need the light when it\'s burning low.';
  const sentence2 = 'Only miss the sun when it starts to snow.';
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
        name: sentence1,
      },
      {
        id: 3,
        role: 'staticText',
        name: sentence2,
      },
    ],
  };
  chrome.readingMode.setContentForTesting(axTree, [2, 3]);

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
  const utterances = readAnythingApp.getUtterancesToSpeak();
  assertEquals(utterances.length, 2);

  // First sentence is highlighted and nothing is before it
  let utterance = utterances[0];
  utterance.onstart();
  assertEquals(
      container.querySelector('.current-read-highlight').textContent,
      sentence1);
  assertEquals(
      container.querySelectorAll('.previous-read-highlight').length, 0);
  utterance.onend();

  // Second sentence is highlighted and first is before it
  utterance = utterances[1];
  utterance.onstart();
  assertEquals(
      container.querySelector('.current-read-highlight').textContent,
      sentence2);
  assertEquals(
      container.querySelector('.previous-read-highlight').textContent,
      sentence1);
  utterance.onend();

  // All text is read so no current highlight - everything is "previous" now
  assertEquals(container.querySelectorAll('.current-read-highlight').length, 0);
  const previousReadHighlights =
      container.querySelectorAll('.previous-read-highlight');
  assertEquals(previousReadHighlights.length, 2);
  assertEquals(previousReadHighlights[0].textContent, sentence1);
  assertEquals(previousReadHighlights[1].textContent, sentence2);

  return result;
})();
