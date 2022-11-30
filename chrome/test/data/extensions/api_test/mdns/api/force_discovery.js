// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function registerListener() {
      var numEvents = 0;
      chrome.mdns.onServiceList.addListener(function(services) {
        chrome.mdns.forceDiscovery(function() {
          chrome.test.assertTrue(!chrome.runtime.lastError);
          chrome.test.succeed();
        });
      }, {'serviceType': '_googlecast._tcp.local'});
      chrome.test.notifyPass();
    }
  ]);
};
