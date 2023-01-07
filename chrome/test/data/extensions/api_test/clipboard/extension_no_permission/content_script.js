// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(kalman): Consolidate this test script with the other clipboard tests.

function appendTextarea() {
  return document.body.appendChild(document.createElement('textarea'));
}

function run() {
  var textIn = appendTextarea();

  textIn.focus();
  textIn.value = 'foobar';
  textIn.selectionStart = 0;
  textIn.selectionEnd = 'foobar'.length;
  if (document.execCommand('copy'))
    return 'Succeeded to copy';

  var textOut = appendTextarea();

  textOut.focus();
  if (document.execCommand('paste'))
    return 'Succeeded to paste';
  if (textOut.value == 'foobar')
    return 'Successfully copied/pasted despite execCommand failures';

  return '';
}

chrome.runtime.onMessage.addListener(function(message, sender, sendResponse) {
  sendResponse(run());
});
