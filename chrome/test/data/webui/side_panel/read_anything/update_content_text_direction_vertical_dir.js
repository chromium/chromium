// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateContent_TextDirection_VerticalDir

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
  // ++paragraph htmlTag='p' id=2 direction='ttb'
  // ++++staticText name='This should be auto' id=3
  // ++paragraph htmlTag='p' id=4 direction='btt'
  // ++++staticText name='This should be also be auto' id=4
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 4],
      },
      {
        id: 2,
        role: 'paragraph',
        htmlTag: 'p',
        direction: 3,
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This should be auto',
      },
      {
        id: 4,
        role: 'paragraph',
        htmlTag: 'p',
        direction: 4,
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'This should be also be auto',
      },
    ],
  };
  chrome.readAnything.setContentForTesting(axTree, [2, 4]);
  const expected = '<div><p dir="auto">This should be auto</p>' +
      '<p dir="auto">This should be also be auto</p></div>';
  assertContainerInnerHTML(expected);

  return result;
})();
