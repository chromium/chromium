// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This extension makes a single call to send a message using the test API,
// it is used to test the activity log page as it should display a call for
// this extension. This test might be expanded upon in the future.
chrome.test.sendMessage('ready');
