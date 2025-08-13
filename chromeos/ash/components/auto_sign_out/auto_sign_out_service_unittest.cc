// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "auto_sign_out_service.h"

#include <memory>

#include "chromeos/ash/components/auto_sign_out/auto_sign_out_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class AutoSignOutTest : public testing::Test {
 public:
  AutoSignOutTest() = default;
  ~AutoSignOutTest() override = default;

 protected:
  AutoSignOutService* auto_sign_out_service() {
    return auto_sign_out_service_.get();
  }

  void InitService() {
    auto_sign_out_service_ = std::make_unique<AutoSignOutService>();
  }

  void SetUp() override {}

  void TearDown() override {}

 private:
  std::unique_ptr<AutoSignOutService> auto_sign_out_service_;
};

}  // namespace ash
