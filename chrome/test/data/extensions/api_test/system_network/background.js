// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testGetNetworkInterfaces = function() {
  chrome.system.network.getNetworkInterfaces(function(list) {
    chrome.test.assertTrue(!!list, "Interface list is undefined.");
    chrome.test.assertTrue(list.length > 0, "Interface list is empty.");
    chrome.test.succeed();
  });
};

chrome.test.runTests([testGetNetworkInterfaces]);
