// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testSentenceStartBoundary() {
    const expectations = getExpections();

    for (const expectation of expectations) {
      assertArrayEquals(expectation.node.sentenceStarts, expectation.starts);
    }
    chrome.test.succeed();
  },
  function testSentenceEndBoundary() {
    const expectations = getExpections();

    for (const expectation of expectations) {
      assertArrayEquals(expectation.node.sentenceEnds, expectation.ends);
    }
    chrome.test.succeed();
  }
];

function assertArrayEquals(a, b) {
  assertEq(a.length, b.length);

  for (var i = 0; i < a.length; ++i) {
    assertEq(a[i], b[i]);
    assertEq(a[i], b[i]);
  }
}

function getExpections() {
  const [node1, node2, node3, node4, node5, node6] =
      rootNode.findAll({role: chrome.automation.RoleType.INLINE_TEXT_BOX});
  return expectations = [
    {starts: [0], ends: [], text: 'This is the ', node: node1},
    {starts: [], ends: [], text: 'first sentence', node: node2},
    {starts: [2], ends: [2], text: '. This is the second ', node: node3},
    {starts: [], ends: [], text: 'sentence', node: node4},
    {starts: [], ends: [1], text: '.', node: node5},
    {starts: [0], ends: [27], text: 'This is the third sentence.', node: node6}
  ];
}
setUpAndRunTabsTests(allTests, 'sentence_boundaries.html');
