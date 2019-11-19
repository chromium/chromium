// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.printingMetrics.onPrintJobFinished.addListener(function(printJob) {
  chrome.storage.local.get('printJobs', function(result) {
    let printJobs = result.printJobs || 0;
    printJobs++;
    chrome.browserAction.setBadgeText({text: printJobs.toString()});
    chrome.storage.local.set({printJobs: printJobs});
  });
});
