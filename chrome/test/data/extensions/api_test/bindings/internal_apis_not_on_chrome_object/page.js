// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.assertFalse(chrome.hasOwnProperty('downloadsInternal'));
chrome.test.assertFalse(chrome.hasOwnProperty('events'));
chrome.test.assertFalse(chrome.hasOwnProperty('fileBrowserHandlerInternal'));
chrome.test.assertFalse(chrome.hasOwnProperty('webRequestInternal'));
chrome.test.assertFalse(chrome.hasOwnProperty('webview'));
chrome.test.succeed();
