// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TEST_WITH_BROWSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TEST_WITH_BROWSER_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/test/base/browser_with_test_window_test.h"

class BrowserView;

// WARNING WARNING WARNING WARNING
// Do not use this class. See docs/chrome_browser_design_principles.md for
// details. If you want to write a test that has both a Browser and a
// BrowserView, create a browser_test. If you want to write a unit_test, your
// code must not reference Browser*.
//
// Base class for BrowserView based unit tests. TestWithBrowserView creates
// a Browser with a valid BrowserView and BrowserFrame with as little else as
// possible.
class TestWithBrowserView : public BrowserWithTestWindowTest {
 public:
  template <typename... Args>
  explicit TestWithBrowserView(Args... args)
      : BrowserWithTestWindowTest(args...) {
    // Media Router requires the IO thread, which doesn't exist in this setup.
    feature_list_.InitAndDisableFeature(media_router::kMediaRouter);
  }

  TestWithBrowserView(const TestWithBrowserView&) = delete;
  TestWithBrowserView& operator=(const TestWithBrowserView&) = delete;

  ~TestWithBrowserView() override;

  // BrowserWithTestWindowTest overrides:
  void SetUp() override;
  void TearDown() override;
  TestingProfile* CreateProfile(const std::string& profile_name) override;
  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override;
  TestingProfile::TestingFactories GetTestingFactories() override;

  BrowserView* browser_view() { return browser_view_; }

 private:
  // The BrowserWindow created because GetBrowserWindow was overridden to return
  // nil. While it's not actually "owned" by this code, this code is responsible
  // for ensuring it gets cleaned up.
  raw_ptr<BrowserView> browser_view_;
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TEST_WITH_BROWSER_VIEW_H_
