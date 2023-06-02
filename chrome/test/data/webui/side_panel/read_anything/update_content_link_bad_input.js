// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateContent_Link_BadInput

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
  // ++link htmlTag='a' id=2
  // ++++staticText name='This link does not have a url.' id=3
  // ++image htmlTag='img' url='http://www.mycat.com' id=4
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
        role: 'link',
        htmlTag: 'a',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This link does not have a url.',
      },
      {
        id: 4,
        role: 'image',
        htmlTag: 'img',
        url: 'http://www.mycat.com',
      },
    ],
  };
  chrome.readingMode.setContentForTesting(axTree, [2, 4]);
  const expected = '<div><a>This link does not have a url.</a><img></div>';
  assertContainerInnerHTML(expected);

  return result;
})();
