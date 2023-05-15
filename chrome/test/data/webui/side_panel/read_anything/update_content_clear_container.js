// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateContent_ClearContainer

// The container clears its old content when it receives new content.

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readAnything.onConnected = () => {};

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
  // ++staticText name='First set of content.' id=2
  const axTree1 = {
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
        name: 'First set of content.',
      },
    ],
  };
  chrome.readAnything.setContentForTesting(axTree1, [2]);
  const expected1 = '<div>First set of content.</div>';
  assertContainerInnerHTML(expected1);

  // root htmlTag='#document' id=1
  // ++staticText name='Second set of content.' id=2
  const axTree2 = {
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
        name: 'Second set of content.',
      },
    ],
  };
  chrome.readAnything.setContentForTesting(axTree2, [2]);
  const expected2 = '<div>Second set of content.</div>';
  assertContainerInnerHTML(expected2);

  return result;
})();
