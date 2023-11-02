// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fail = chrome.test.callbackFail;
var pass = chrome.test.callbackPass;

function terminateBrowserProcess() {
  chrome.processes.getProcessInfo(0, false, pass(function(info) {
    chrome.test.assertEq('browser', info[0].type);
    var error = 'Not allowed to terminate process: 0.';
    chrome.processes.terminate(0, fail(error, function(){}));
  }));
}

chrome.test.runTests([terminateBrowserProcess]);
