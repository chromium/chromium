// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic test to ensure we can send messages to the browser process and back.
chrome.test.waitForRoundTrip('foo', (response) => {
  chrome.test.assertEq('foo', response);
  chrome.test.notifyPass();
});
