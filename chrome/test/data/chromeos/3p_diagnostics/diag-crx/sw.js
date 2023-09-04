// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onInstalled.addListener((details) => {
  console.log('Service worker is installed!', details);
});

chrome.runtime.onMessageExternal.addListener((req, sender, res) => {});
