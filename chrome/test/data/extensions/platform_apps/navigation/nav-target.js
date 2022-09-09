// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We should never reach this page; if we have then it's a signal that we've
// navigated away from the app page, and we should have the test fail.
chrome.test.notifyFail('Navigated to ' + window.location.href);
