// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_EXO_TEST_BASE_VIEWS_H_
#define COMPONENTS_EXO_TEST_EXO_TEST_BASE_VIEWS_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/views/test/views_test_base.h"

namespace exo {

class WMHelper;

namespace test {

// Implementation of a test::Base built on Views without ChromeOS Ash
// dependencies.
class ExoTestBaseViews : public views::ViewsTestBase {
 public:
  ExoTestBaseViews();
  ~ExoTestBaseViews() override;

  // Overridden from test::Test.
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<WMHelper> wm_helper_;
  ui::ScopedTestInputMethodFactory scoped_test_input_method_factory_;
};

}  // namespace test
}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_EXO_TEST_BASE_VIEWS_H_
