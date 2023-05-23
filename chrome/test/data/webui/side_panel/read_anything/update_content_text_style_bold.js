// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateContent_TextStyle_Bold

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
  // ++paragraph htmlTag='p' id=2
  // ++++staticText name='Regular text.' id=3
  // ++++staticText name='This should be bolded.' textStyle='underline' id=4
  // ++paragraph htmlTag='p' id=5
  // ++++staticText name='Bolded text.' textStyle='italic' id=6
  // ++++staticText name='Bolded text.' textStyle='bold' id=7
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 5],
      },
      {
        id: 2,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [3, 4],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'Regular text.',
      },
      {
        id: 4,
        role: 'staticText',
        textStyle: 'underline',
        name: 'This should be bolded.',
      },
      {
        id: 5,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [6, 7],
      },
      {
        id: 6,
        role: 'staticText',
        textStyle: 'italic',
        name: 'Bolded text.',
      },
      {
        id: 7,
        role: 'staticText',
        textStyle: 'bold',
        name: 'Bolded text.',
      },
    ],
  };
  chrome.readingMode.setContentForTesting(axTree, [2, 5]);
  const expected = '<div><p>Regular text.<b>This should be bolded.</b></p>' +
      '<p><b>Bolded text.</b><b>Bolded text.</b></p></div>';
  assertContainerInnerHTML(expected);

  return result;
})();
