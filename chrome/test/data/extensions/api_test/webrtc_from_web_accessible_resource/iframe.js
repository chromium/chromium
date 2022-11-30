// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var successCallback = chrome.test.succeed;
var failCallback = chrome.test.fail;

if (document.location.search === '?fail') {
  successCallback = chrome.test.fail.bind(
      chrome.test, 'gUM unexpectedly invoked the success callback');
  failCallback = chrome.test.succeed;
}

navigator.webkitGetUserMedia({
 audio: true,
 video: false
}, successCallback, failCallback);
