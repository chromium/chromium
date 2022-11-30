// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.assertTrue(chrome.hasOwnProperty('runtime'));

var iframe = document.createElement('iframe');
iframe.src = 'http://mock.http/';
iframe.onload = function() {
  chrome.test.assertTrue(chrome.test.getModuleSystem(window) instanceof Object);
  chrome.test.assertEq(undefined,
                       chrome.test.getModuleSystem(iframe.contentWindow));
  chrome.test.notifyPass();
};
document.body.appendChild(iframe);
