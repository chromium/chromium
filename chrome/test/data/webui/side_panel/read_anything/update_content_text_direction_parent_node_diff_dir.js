// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.\
//    UpdateContent_TextDirection_ParentNodeDiffDir

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
  // ++paragraph htmlTag='p' id=2 direction='ltr'
  // ++++staticText name='This is ltr' id=3
  // ++++link htmlTag='a' url='http://www.google.com/' id=4 direction='rtl'
  // ++++++staticText name='This link is rtl' id=5
  const axTree = {
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
        role: 'paragraph',
        htmlTag: 'p',
        direction: 1,
        childIds: [3, 4],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This is ltr',
      },
      {
        id: 4,
        role: 'link',
        htmlTag: 'a',
        direction: 2,
        url: 'http://www.google.com/',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'This link is rtl',
      },
    ],
  };
  chrome.readingMode.setContentForTesting(axTree, [2]);
  const expected = '<div><p dir="ltr">This is ltr' +
      '<a dir="rtl" href="http://www.google.com/">' +
      'This link is rtl</a></p></div>';
  assertContainerInnerHTML(expected);

  return result;
})();
