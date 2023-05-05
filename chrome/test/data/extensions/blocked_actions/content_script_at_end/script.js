// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set the title of the document to the success state (so that it's easily
// readable from the C++ side).
document.title = 'success';

// Send a succeeded injection message that we can wait for in the test.
chrome.test.sendMessage("injection succeeded");
