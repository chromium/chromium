// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test clipboard extension api chrome.clipboard.onClipboardDataChanged event.


function copyTextData(text) {
  var input = document.getElementById('copy_text');
  input.value = text;
  input.focus();
  input.select();
  if (document.execCommand('Copy'))
    chrome.test.succeed();
  else
    chrome.test.fail('copy text failed');
}

function testCopyFoo() {
  copyTextData('Foo');
}

function testCopyBar() {
  copyTextData('Bar');
}

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    testCopyFoo,
    testCopyBar
  ]);
})
