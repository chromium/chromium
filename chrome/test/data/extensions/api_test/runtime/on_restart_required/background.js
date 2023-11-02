// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onRestartRequired(reason) {
  chrome.test.sendMessage(reason);
}

chrome.runtime.onRestartRequired.addListener(onRestartRequired);
