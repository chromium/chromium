// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateContent_Heading

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
  // ++heading htmlTag='h1' id=2
  // ++++staticText name='This is an h1.' id=3
  // ++heading htmlTag='h2' id=4
  // ++++staticText name='This is an h2.' id=5
  // ++heading htmlTag='h3' id=6
  // ++++staticText name='This is an h3.' id=7
  // ++heading htmlTag='h4' id=8
  // ++++staticText name='This is an h4.' id=9
  // ++heading htmlTag='h5' id=10
  // ++++staticText name='This is an h5.' id=11
  // ++heading htmlTag='h6' id=12
  // ++++staticText name='This is an h6.' id=13
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 4, 6, 8, 10, 12],
      },
      {
        id: 2,
        role: 'heading',
        htmlTag: 'h1',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This is an h1.',
      },
      {
        id: 4,
        role: 'heading',
        htmlTag: 'h2',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'This is an h2.',
      },
      {
        id: 6,
        role: 'heading',
        htmlTag: 'h3',
        childIds: [7],
      },
      {
        id: 7,
        role: 'staticText',
        name: 'This is an h3.',
      },
      {
        id: 8,
        role: 'heading',
        htmlTag: 'h4',
        childIds: [9],
      },
      {
        id: 9,
        role: 'staticText',
        name: 'This is an h4.',
      },
      {
        id: 10,
        role: 'heading',
        htmlTag: 'h5',
        childIds: [11],
      },
      {
        id: 11,
        role: 'staticText',
        name: 'This is an h5.',
      },
      {
        id: 12,
        role: 'heading',
        htmlTag: 'h6',
        childIds: [13],
      },
      {
        id: 13,
        role: 'staticText',
        name: 'This is an h6.',
      },
    ],
  };
  chrome.readingMode.setContentForTesting(axTree, [2, 4, 6, 8, 10, 12]);
  const expected = '<div><h1>This is an h1.</h1><h2>This is an h2.</h2>' +
      '<h3>This is an h3.</h3><h4>This is an h4.</h4>' +
      '<h5>This is an h5.</h5><h6>This is an h6.</h6></div>';
  assertContainerInnerHTML(expected);

  return result;
})();
