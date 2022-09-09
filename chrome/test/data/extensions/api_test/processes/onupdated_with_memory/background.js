// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Add a simple listener to onUpdatedWithMemory to ensure that the task
// manager's refresh types are updated correctly.
chrome.processes.onUpdatedWithMemory.addListener(function(processes) {
  console.log("Received update with memory.");
});

chrome.test.sendMessage("ready");