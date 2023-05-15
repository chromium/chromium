// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.\
//    UpdateContent_Selection_OutsideDistilledContent

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
  // ++paragraph htmlTag='p' id=2
  // ++++staticText name='Hello' id=3
  // ++paragraph htmlTag='p' id=4
  // ++++staticText name='World' id=5
  // ++paragraph htmlTag='p' id=6
  // ++++staticText name='Friend' id=7
  // ++++staticText name='!' id=8
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 4, 6],
      },
      {
        id: 2,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'Hello',
      },
      {
        id: 4,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'World',
      },
      {
        id: 6,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [7, 8],
      },
      {
        id: 7,
        role: 'staticText',
        name: 'Friend',
      },
      {
        id: 8,
        role: 'staticText',
        name: '!',
      },
    ],
    selection: {
      anchor_object_id: 5,
      focus_object_id: 7,
      anchor_offset: 1,
      focus_offset: 2,
      is_backward: false,
    },
  };
  chrome.readAnything.setContentForTesting(axTree, [2]);
  // The selection is outside the content nodes.The expected string contains
  // the complete text of each node in the selection.
  const expected = '<div><p>World</p><p>Friend!</p></div>';
  assertContainerInnerHTML(expected);

  return result;
})();
