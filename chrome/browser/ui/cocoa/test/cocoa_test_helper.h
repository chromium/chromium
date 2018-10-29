// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TEST_COCOA_TEST_HELPER_H_
#define CHROME_BROWSER_UI_COCOA_TEST_COCOA_TEST_HELPER_H_

#import <Cocoa/Cocoa.h>

#import "ui/base/test/cocoa_helper.h"

// A test class that all tests that depend on AppKit should inherit from.
// Sets up paths correctly, and makes sure that any windows created in the test
// are closed down properly by the test. If you need to inherit from a
// different test class, but need to set up the AppKit runtime environment, you
// can call BootstrapCocoa directly from your test class. You will have to deal
// with windows on your own though.  Note that NSApp is initialized by
// ChromeTestSuite.
class CocoaTest : public ui::CocoaTest {
 public:
  // Sets up paths correctly for unit tests. If you can't inherit from
  // CocoaTest but are going to be using any AppKit features directly, or
  // indirectly, you should be calling this from the c'tor or SetUp methods of
  // your test class. Note that NSApp is initialized by ChromeTestSuite.
  static void BootstrapCocoa();

  CocoaTest();
};

#endif  // CHROME_BROWSER_UI_COCOA_TEST_COCOA_TEST_HELPER_H_
