// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_VIEWS_CHROME_VIEWS_TEST_BASE_H_
#define CHROME_TEST_VIEWS_CHROME_VIEWS_TEST_BASE_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/views/test/views_test_base.h"

// A base class for Chrome views unit tests. Changes the dependencies when they
// need to be different than non-Chrome views.
class ChromeViewsTestBase : public views::ViewsTestBase {
 public:
  ChromeViewsTestBase();
  ChromeViewsTestBase(const ChromeViewsTestBase&) = delete;
  ChromeViewsTestBase& operator=(const ChromeViewsTestBase&) = delete;
  ~ChromeViewsTestBase() override;

  // views::ViewsTestBase:
  void SetUp() override;
  void TearDown() override;
  std::unique_ptr<views::Widget> AllocateTestWidget() override;
};

#endif  // CHROME_TEST_VIEWS_CHROME_VIEWS_TEST_BASE_H_
