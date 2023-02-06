// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Connects to another extension via a persistent port.
var port = chrome.runtime.connect('eagjmgdicfmccfhiiihnaehbfheheidk');

port.onDisconnect.addListener(() => {
  chrome.test.sendMessage('Persistent port disconnected');
});
