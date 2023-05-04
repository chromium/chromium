// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateContent_TextStyle_Overline

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
  // ++++staticText name='This should be overlined.' textStyle='overline' id=3
  // ++++staticText name='Regular text.' id=4
  // ++++staticText name='This is overlined and bolded.' textStyle='overline
  // underline'id=5
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
        childIds: [3, 4, 5],
      },
      {
        id: 3,
        role: 'staticText',
        textStyle: 'overline',
        name: 'This should be overlined.',
      },
      {
        id: 4,
        role: 'staticText',
        name: 'Regular text.',
      },
      {
        id: 5,
        role: 'staticText',
        textStyle: 'overline underline',
        name: 'This is overlined and bolded.',
      },
    ],
  };
  chrome.readAnything.setContentForTesting(axTree, [2]);
  const expected = '<div><p><span style="text-decoration: overline;">This ' +
      'should be overlined.</span>Regular text.<b style="text-decoration: ' +
      'overline;">This is overlined and bolded.</b></p></div>';
  assertContainerInnerHTML(expected);

  return result;
})();
