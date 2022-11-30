// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', function () {
  var button1 = document.querySelector('#button1');
  var button2 = document.querySelector('#button2');
  var button3 = document.querySelector('#button3');
  button1.addEventListener('focus', function(e) {
    chrome.test.sendMessage('button1-focused');
  });
  button2.addEventListener('focus', function(e) {
    chrome.test.sendMessage('button2-focused');
  });
  button3.addEventListener('focus', function(e) {
    chrome.test.sendMessage('button3-focused');
  });
  chrome.test.sendMessage("ready");
});

