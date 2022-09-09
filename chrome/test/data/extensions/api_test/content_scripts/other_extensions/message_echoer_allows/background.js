// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(
    function(message, sender, sendResponse) {
      sendResponse({receivedMessage: message, receivedSender: sender});
    });
