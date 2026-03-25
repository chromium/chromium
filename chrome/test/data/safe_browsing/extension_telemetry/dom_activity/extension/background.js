// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
chrome.action.onClicked.addListener((tab) => {
  console.log('Action clicked, injecting script...');
  if (tab.id) {
    chrome.scripting.executeScript({
      target: {tabId: tab.id},
      func: () => {
        console.log(
            'Injected script running via chrome.scripting.executeScript.');
        // We can optionally trigger DOM signals from here as well,
        // but this call itself triggers TelemetrySignalType::kScriptInjection
        // according to the provided C++ code.
      }
    });
  }
});
