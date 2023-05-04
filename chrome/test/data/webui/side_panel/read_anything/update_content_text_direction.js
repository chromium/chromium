// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateContent_TextDirection

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
  // ++paragraph htmlTag='p' id=2 direction='ltr'
  // ++++staticText name='This is left to right writing' id=3
  // ++paragraph htmlTag='p' id=4 direction='rtl'
  // ++++staticText name='This is right to left writing' id=4
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
        direction: 1,
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This is left to right writing',
      },
      {
        id: 4,
        role: 'paragraph',
        htmlTag: 'p',
        direction: 2,
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'This is right to left writing',
      },
    ],
  };
  chrome.readAnything.setContentForTesting(axTree, [2, 4]);
  const expected = '<div><p dir="ltr">This is left to right writing</p>' +
      '<p dir="rtl">This is right to left writing</p></div>';
  assertContainerInnerHTML(expected);

  return result;
})();
