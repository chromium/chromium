// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var validate = function(policy) {
  // This is the policy set by component_cloud_policy_browsertest.cc.
  if (JSON.stringify(policy) == '{"Another":"turn_it_off"}')
    chrome.test.sendMessage('ok');
  else
    chrome.test.sendMessage('fail');
}

// Get the initial policy, in case it was fetched before the extension started.
chrome.storage.managed.get(function(policy) {
  if (JSON.stringify(policy) == '{}') {
    // Start listening for the update event.
    chrome.storage.onChanged.addListener(function(changes, namespace) {
      if (namespace == 'managed') {
        // Get all the policies and validate them.
        chrome.storage.managed.get(validate);
      }
    });
  } else {
    validate(policy);
  }
});
