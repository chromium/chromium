// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.action.onClicked.addListener(function(tab) {
  chrome.test.notifyPass();
});

chrome.test.notifyPass();
