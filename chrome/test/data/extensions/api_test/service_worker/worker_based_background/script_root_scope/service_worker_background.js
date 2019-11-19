// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Open a tab to the doc that includes a script that will attempt to
// register a service worker at the root scope.
chrome.tabs.create({url: chrome.runtime.getURL('a.html')});
