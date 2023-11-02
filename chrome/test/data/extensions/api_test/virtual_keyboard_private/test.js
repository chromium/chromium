// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbackPass = chrome.test.callbackPass;

const itemToDict = (item) => {
  return {
    displayFormat: item.displayFormat,
    textData: !!item.textData,
    imageData: !!item.imageData,
    timeCopied: !!item.timeCopied
  };
};

function checkFullResult(result) {
  const parsed = result.map(itemToDict);

  // Test that clipboard items are in the correct order with the correct data
  // types.
  chrome.test.assertEq(parsed, [
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

function checkEmptyResult(result) {
  const parsed = result.map(itemToDict);

  // Test that no clipboard items are returned.
  chrome.test.assertEq(parsed, []);
}

chrome.test.getConfig(function(config) {
  const screenLocked = config.customArg;
  chrome.test.runTests([function multipasteApi() {
    chrome.virtualKeyboardPrivate.getClipboardHistory(
        {}, callbackPass(screenLocked ? checkEmptyResult : checkFullResult));
  }]);
});
