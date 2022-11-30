// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function registerListeners() {
      var numEvents = 0;
      chrome.mdns.onServiceList.addListener(function(services) {
        if (numEvents++ == 1) {
          chrome.test.succeed();
        }
      }, {'serviceType': '_googlecast._tcp.local'});

      chrome.mdns.onServiceList.addListener(function(services) {
        if (numEvents++ == 1) {
          chrome.test.succeed();
        }
      }, {'serviceType': '_testing._tcp.local'});
      chrome.test.notifyPass();
    }
  ]);
};
