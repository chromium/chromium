// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(
    (request, sender, sendResponse) => {
      chrome.test.assertEq('initiator->target', request);
      sendResponse('initiator->target->initiator');
    });
