// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Wait for the API test framework to decide we're done loading before
// uninstalling, or it'll wait forever.
chrome.runtime.onInstalled.addListener(function() {
  chrome.management.uninstallSelf();
});
