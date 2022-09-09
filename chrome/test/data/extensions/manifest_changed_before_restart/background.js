// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
chrome.tabs.onCreated.addListener(() => {});

chrome.test.sendMessage('ready');
