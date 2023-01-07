// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.sendMessage('sender_ready', function(id) {
  chrome.runtime.sendMessage(id, 'from_sender');
});
