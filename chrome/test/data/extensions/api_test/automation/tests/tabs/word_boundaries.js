// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const allTests = [
  function testWordStartAndEndOffsets() {
    const node = rootNode.find(
        {attributes: {name: 'Example text for testing purposes'}});
    const expectedWordStarts = [0, 8, 13, 17, 25];
    const expectedWordEnds = [7, 12, 16, 24, 33];
    const wordStarts = node.nonInlineTextWordStarts;
    const wordEnds = node.nonInlineTextWordEnds;
    assertEq(expectedWordStarts.length, wordStarts.length);
    assertEq(expectedWordEnds.length, wordEnds.length);
    assertEq(wordStarts.length, wordEnds.length);
    for (let i = 0; i < expectedWordStarts.length; ++i) {
      assertEq(expectedWordStarts[i], wordStarts[i]);
      assertEq(expectedWordEnds[i], wordEnds[i]);
    }
    chrome.test.succeed();
  },
];

setUpAndRunTabsTests(allTests, 'word_boundaries.html');
