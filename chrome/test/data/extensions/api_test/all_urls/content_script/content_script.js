// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Let the background page know this content script executed.
chrome.runtime.sendMessage({greeting: "hello"});
