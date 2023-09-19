// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppToolbarTest.HighlightCallback_TogglesHighlight
// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};
  const readAnythingApp = document.querySelector('read-anything-app');
  const container = readAnythingApp.shadowRoot.getElementById('container');
  const toolbar =
      readAnythingApp.shadowRoot.querySelector('read-anything-toolbar')
          .shadowRoot;
  const highlightButton = toolbar.getElementById('highlight');
  const sentence1 = 'Big wheel keep on turning.';
  const sentence2 = 'Proud Mary keep on burning.';
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
  const assertNE = (actual, expected) => {
    const isNotEqual = actual !== expected;
    if (!isNotEqual) {
      console.error(
          'Expected ' + JSON.stringify(actual) + ' to be not equal to ' +
          JSON.stringify(expected));
    }
    result = result && isNotEqual;
    return isNotEqual;
  };

  // Default on
  assertEquals(
      highlightButton.getAttribute('iron-icon'), 'read-anything:highlight-on');

  // Speech doesn't actually run in tests, so manually call start
  readAnythingApp.playSpeech();
  const utterances = readAnythingApp.getUtterancesToSpeak();
  assertEquals(utterances.length, 2);
  let utterance = utterances[0];
  utterance.onstart();
  utterance.onend();
  utterance = utterances[1];
  utterance.onstart();

  // Color should start visible, and the previous highlight should be there
  const highlights = container.querySelectorAll('.current-read-highlight');
  assertEquals(highlights.length, 1);
  assertEquals(
      container.querySelectorAll('.previous-read-highlight').length, 1);
  let highlightColor = window.getComputedStyle(highlights[0])
                           .getPropertyValue('--current-highlight-bg-color');
  assertNE(highlightColor, 'transparent');

  // After turning off the highlight, it should be transparent, and the
  // previous highlight should still be there
  highlightButton.click();
  assertEquals(
      highlightButton.getAttribute('iron-icon'), 'read-anything:highlight-off');
  assertEquals(
      container.querySelectorAll('.previous-read-highlight').length, 1);
  highlightColor = window.getComputedStyle(highlights[0])
                       .getPropertyValue('--current-highlight-bg-color');
  assertEquals(highlightColor, 'transparent');

  return result;
})();
