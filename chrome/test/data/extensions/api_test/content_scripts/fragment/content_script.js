// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that we don't re-inject scripts after fragment navigation.

// The background page should only see this once - it will then use tab.update
// to navigate this page to #foo.
chrome.runtime.sendMessage("content_script_start");

if (location.href.indexOf("#foo") != -1) {
  // This means the content script ran again.
  chrome.runtime.sendMessage("fail");
}
