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

  const sentence1 = 'The snow glows white on the mountain tonight. ';
  const sentence2 = 'Not a footprint to be seen. ';
  const sentence3 = 'A kingdom of isolation. ';
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

  // First sentence is highlighted and nothing is before it
  assertEquals(
      container.querySelector('.current-read-highlight').textContent,
      sentence1);
  assertEquals(
      container.querySelectorAll('.previous-read-highlight').length, 0);
  readAnythingApp.resetPreviousHighlight();

  // Second sentence is highlighted and first is before it
  readAnythingApp.playNextGranularity();
  assertEquals(
      container.querySelector('.current-read-highlight').textContent,
      sentence2);
  let previousHighlights =
      container.querySelectorAll('.previous-read-highlight');
  assertEquals(previousHighlights.length, 1);
  assertEquals(previousHighlights[0].textContent, sentence1);
  readAnythingApp.resetPreviousHighlight();

  // Third sentence is next.
  readAnythingApp.playNextGranularity();
  assertEquals(
      container.querySelector('.current-read-highlight').textContent,
      sentence3);
  previousHighlights = container.querySelectorAll('.previous-read-highlight');
  assertEquals(previousHighlights.length, 2);
  assertEquals(previousHighlights[0].textContent, sentence1);
  assertEquals(previousHighlights[1].textContent, sentence2);

  // Attempt to speak more utterance than are left.
  readAnythingApp.playNextGranularity();
  readAnythingApp.playNextGranularity();
  readAnythingApp.playNextGranularity();

  // After speech is stopped, playing the next / previous granularity cause no
  // crashes, and don't change utterances.
  readAnythingApp.playNextGranularity();
  readAnythingApp.playPreviousGranularity();

  return result;
})();
