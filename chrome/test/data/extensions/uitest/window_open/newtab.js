// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testExtensionApi() {
  try {
    chrome.tabs.getAllInWindow(null, function() {
      window.domAutomationController.send(
          !chrome.runtime.lastError);
    });
  } catch (e) {
    window.domAutomationController.send(false);
  }
}
