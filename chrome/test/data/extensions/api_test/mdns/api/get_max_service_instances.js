// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function getMaxServiceInstancesPerEvent() {
      // Ensure that the constant is actually set.
      chrome.test.assertTrue(chrome.mdns.MAX_SERVICE_INSTANCES_PER_EVENT > 0);
      chrome.test.notifyPass();
    }
  ]);
};
