// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TEST_COCOA_PROFILE_TEST_H_
#define CHROME_BROWSER_UI_COCOA_TEST_COCOA_PROFILE_TEST_H_

#include <memory>

#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/views/test/scoped_views_test_helper.h"

namespace content {
class BrowserTaskEnvironment;
}

class Browser;
class TestingProfile;

// Base class which contains a valid Browser*.  Lots of boilerplate to
// recycle between unit test classes.
//
// This class creates fake UI, file, and IO threads because many objects that
// are attached to the TestingProfile (and other objects) have traits that limit
// their destruction to certain threads. For example, the net::URLRequestContext
// can only be deleted on the IO thread; without this fake IO thread, the object
// would never be deleted and would report as a leak under Valgrind. Note that
// these are fake threads and they all share the same MessageLoop.
//
// TODO(rsesek): There is very little "Cocoa" about this class anymore. It
// should likely be removed in favor of
// chrome/browser/ui/views/frame/test_with_browser_view.h.
class CocoaProfileTest : public CocoaTest {
 public:
  CocoaProfileTest();
  ~CocoaProfileTest() override;

  // This constructs a a Browser and a TestingProfile. It is guaranteed to
  // succeed, else it will ASSERT and cause the test to fail. Subclasses that
  // do work in SetUp should ASSERT that either browser() or profile() are
  // non-NULL before proceeding after the call to super (this).
  void SetUp() override;

  void TearDown() override;

  TestingProfileManager* testing_profile_manager() { return &profile_manager_; }
  TestingProfile* profile() { return profile_; }
  Browser* browser() { return browser_.get(); }

  // Closes the window for this browser. This will automatically be called as
  // part of TearDown() if it's not been done already.
  void CloseBrowserWindow();

 protected:
  // Overridden by test subclasses to create their own browser, e.g. with a
  // test window.
  virtual Browser* CreateBrowser();

 private:
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;

  views::ScopedViewsTestHelper views_helper_;

  // test_url_loader_factory_ is declared before profile_manager_
  // to guarantee it outlives profile_.
  network::TestURLLoaderFactory test_url_loader_factory_;

  TestingProfileManager profile_manager_;
  TestingProfile* profile_;  // Weak; owned by profile_manager_.
  std::unique_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_COCOA_TEST_COCOA_PROFILE_TEST_H_
