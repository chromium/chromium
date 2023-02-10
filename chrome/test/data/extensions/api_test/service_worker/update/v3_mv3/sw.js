// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.oninstall = function(e) {
  // We'd like to make sure onInstall event is executed after loading
  // the service worker.
  e.waitUntil(self.skipWaiting());
  chrome.test.sendMessage('Pong from version 3');
};
