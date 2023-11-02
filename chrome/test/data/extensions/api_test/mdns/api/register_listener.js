// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function registerListener() {
      var numEvents = 0;
      chrome.mdns.onServiceList.addListener(function(services) {
        if (services[0].serviceName != '_googlecast._tcp.local') {
          chrome.test.fail();
          return;
        } else if (numEvents == 1) {
          chrome.test.succeed();
        } else {
          numEvents++;
        }
      }, {'serviceType': '_googlecast._tcp.local'});
      chrome.test.notifyPass();
    }
  ]);
};
