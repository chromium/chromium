// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test() {
  chrome.test.assertTrue(chrome.runtime != null);

  var iframe = document.createElement('iframe');
  document.body.appendChild(iframe);
  iframe.contentWindow.chrome = chrome;

  // The context-wide bindings recalculation happens when extensions are
  // enabled and disabled.
  chrome.test.sendMessage('load', chrome.test.callbackPass(function(msg) {
    chrome.test.assertTrue(chrome.runtime != null);
  }));
}

chrome.test.runTests([test]);
