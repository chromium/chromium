// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testWordStartAndEndOffsets() {
    var node = rootNode.find(
        { attributes: { name: 'Example text for testing purposes' } });
    var expectedWordStarts = [0, 8, 13, 17, 25];
    var expectedWordEnds = [7, 12, 16, 24, 33];
    var wordStarts = node.nonInlineTextWordStarts;
    var wordEnds = node.nonInlineTextWordEnds;
    assertEq(expectedWordStarts.length, wordStarts.length);
    assertEq(expectedWordEnds.length, wordEnds.length);
    assertEq(wordStarts.length, wordEnds.length);
    for (var i = 0; i < expectedWordStarts.length; ++i){
        assertEq(expectedWordStarts[i], wordStarts[i]);
        assertEq(expectedWordEnds[i], wordEnds[i]);
    }
    chrome.test.succeed();
  }
];

setUpAndRunTabsTests(allTests, 'word_boundaries.html');
