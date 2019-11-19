// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var port = chrome.runtime.connect();
port.onMessage.addListener(msg => {
  // Expect no messages from the extension SW.
  chrome.test.fail();
});
port.disconnect();
