// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Devtools APIs are all marked nocompile, so they shouldn't exist.
chrome.test.assertEq(undefined, chrome.devtools);
chrome.test.assertEq('undefined', typeof chrome.devtools);
chrome.test.assertFalse(chrome.hasOwnProperty('devtools'));
chrome.test.succeed();
