// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const allTests = [
  function testTextareaAppend() {
    const runButton = rootNode.find({role: 'button'});
    assertEq('button', runButton.role);

    const doneLink = rootNode.find({role: 'link'});
    assertEq('link', doneLink.role);

    rootNode.addEventListener('childrenChanged', (evt) => {});

    doneLink.addEventListener('focus', () => {
      chrome.test.succeed();
    });

    runButton.doDefault();
  },
];

setUpAndRunTabsTests(allTests, 'textarea_append.html');
