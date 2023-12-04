// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.messageCount = 0;

chrome.runtime.onMessage.addListener(function(request, sender, sendResponse) {
  ++window.messageCount;
  window.message = request;
});
