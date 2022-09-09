// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(port => {
  // Expect no message from content script.
  port.onMessage.addListener(msg => { chrome.test.fail(); });
  // Expect disconnect from content script.
  port.onDisconnect.addListener(() => { chrome.test.succeed(); });
});
