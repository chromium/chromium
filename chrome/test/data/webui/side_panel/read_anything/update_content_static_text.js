// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateContent_StaticText

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp =
      document.querySelector('read-anything-app').shadowRoot;
  const container = readAnythingApp.getElementById('container');
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

  const assertContainerInnerHTML = (expected) => {
    const actual = container.innerHTML;
    assertEquals(actual, expected);
  };

  // root htmlTag='#document' id=1
  // ++staticText name='This is some text.' id=2
  // ++staticText name='This is some more text.' id=3
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
        name: 'This is some text.',
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This is some more text.',
      },
    ],
  };
  chrome.readingMode.setContentForTesting(axTree, [2, 3]);
  const expected = '<div>This is some text.This is some more text.</div>';
  assertContainerInnerHTML(expected);

  return result;
})();
