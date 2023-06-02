// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateContent_Language_ParentLangSet

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
  // ++paragraph htmlTag='p' id=2 language='en'
  // ++++staticText name='This is in English' id=3
  // ++++link htmlTag='a' url='http://www.google.com/' id=4
  // ++++++staticText name='This link has no language set' id=5
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
        language: 'en',
        childIds: [3, 4],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This is in English',
      },
      {
        id: 4,
        role: 'link',
        htmlTag: 'a',
        url: 'http://www.google.com/',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'This link has no language set',
      },
    ],
  };
  chrome.readingMode.setContentForTesting(axTree, [2]);
  const expected = '<div><p lang="en">This is in English' +
      '<a href="http://www.google.com/">This link has no language set</a>' +
      '</p></div>';
  assertContainerInnerHTML(expected);

  return result;
})();
