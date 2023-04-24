// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateContent_Link

const readAnythingApp = document.querySelector('read-anything-app').shadowRoot;
const container = readAnythingApp.getElementById('container');
let result = true;

function assertEquals(actual, expected) {
  const isEqual = actual === expected;
  if (!isEqual) {
    console.error(
        'Expected: ' + JSON.stringify(expected) + ', ' +
        'Actual: ' + JSON.stringify(actual));
  }
  result = result && isEqual;
  return isEqual;
}

function assertContainerInnerHTML(expected) {
  const actual = container.innerHTML;
  assertEquals(actual, expected);
}

// root htmlTag='#document' id=1
// ++link htmlTag='a' url='http://www.google.com' id=2
// ++++staticText name='This is a link.' id=3
// ++link htmlTag='a' url='http://www.youtube.com' id=4
// ++++staticText name='This is another link.' id=5
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
      url: 'http://www.google.com',
      childIds: [3],
    },
    {
      id: 3,
      role: 'staticText',
      name: 'This is a link.',
    },
    {
      id: 4,
      role: 'link',
      htmlTag: 'a',
      url: 'http://www.youtube.com',
      childIds: [5],
    },
    {
      id: 5,
      role: 'staticText',
      name: 'This is another link.',
    },
  ],
};
chrome.readAnything.setContentForTesting(axTree, [2, 4]);
const expected = '<div><a href="http://www.google.com">This is a link.' +
    '</a><a href="http://www.youtube.com">This is another link.</a></div>';
assertContainerInnerHTML(expected);

domAutomationController.send(result);
