// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Send a string to the browser.
//
// Use as follows:
//
//  In JavaScript:
//   〈script〉
//      sendValueToTest("Hello");
//   〈/script〉
//
//  In C++:
//
//    #include "content/public/test/browser_test_utils.h"
//
//    ...
//    DOMMessageQueue message_queue;
//
//    std::string message;
//    message_queue.WaitForMessage(&message);
//    EXPECT_EQ(message, "Hello")
//
function sendValueToTest(value) {
  window.domAutomationController.send(value);
}
