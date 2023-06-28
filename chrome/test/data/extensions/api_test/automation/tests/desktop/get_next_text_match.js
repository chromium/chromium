// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testGetMatch() {
    assertTrue(!!rootNode.getNextTextMatch('outer', false), 'Forward error');
    assertTrue(!!rootNode.getNextTextMatch('OUTER', false), 'Case error');
    assertFalse(!!rootNode.getNextTextMatch('outer', true), 'No obj expected');
    assertFalse(!!rootNode.getNextTextMatch('asdf', false), 'no obj expected');
    chrome.test.succeed();
  },
];

setUpAndRunTestsInPage(allTests, 'iframe_outer.html');
