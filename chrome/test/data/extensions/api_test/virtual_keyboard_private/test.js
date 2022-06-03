// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

function callbackResult(result) {
  var result = result.map((item) => {
    return {
      displayFormat: item.displayFormat,
      textData: !!item.textData,
      imageData: !!item.imageData,
      timeCopied: !!item.timeCopied
    };
  });

  // Test that clipboard items are in the correct order with the correct data
  // types.
  chrome.test.assertEq(result, [
    {
      'displayFormat': 'file',
      'textData': true,
      'imageData': true,
      'timeCopied': true
    },
    {
      'displayFormat': 'png',
      'textData': false,
      'imageData': true,
      'timeCopied': true
    },
    {
      'displayFormat': 'text',
      'textData': true,
      'imageData': false,
      'timeCopied': true
    },
    {
      'displayFormat': 'html',
      'textData': false,
      'imageData': true,
      'timeCopied': true
    }
  ]);
}

chrome.test.runTests([
  function multipasteApi() {
    chrome.virtualKeyboardPrivate.getClipboardHistory({},
      callbackPass(callbackResult));
  }
]);
