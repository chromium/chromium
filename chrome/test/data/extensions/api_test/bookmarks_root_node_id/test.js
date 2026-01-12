// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const rootId = chrome.bookmarks.ROOT_NODE_ID;

// Send the root node ID to C++. We will check it matches the expected value
// there.
chrome.test.sendMessage(rootId);
chrome.test.succeed();
