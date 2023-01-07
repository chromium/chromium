// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Add a simple listener to onUpdated to ensure it does not conflict with the
// task manager.
chrome.processes.onUpdated.addListener(function(processes) {
  console.log("Received update.");
});

// Add a second listener to onUpdated to ensure the task manager only hears
// about one extension listener per process.
chrome.processes.onUpdated.addListener(function(processes) {
  console.log("Second listener received update.");
});

chrome.test.sendMessage("ready");
