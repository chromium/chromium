// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/ntp_custom_background_service_base.h"

#include <memory>

#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestNtpCustomBackgroundServiceBase : public NtpCustomBackgroundServiceBase {
 public:
  TestNtpCustomBackgroundServiceBase(PrefService* pref_service,
                                     NtpBackgroundService* background_service)
      : NtpCustomBackgroundServiceBase(pref_service, background_service) {}
};

class NtpCustomBackgroundServiceBaseTest : public testing::Test {
 public:
  NtpCustomBackgroundServiceBaseTest() = default;

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    service_ = std::make_unique<TestNtpCustomBackgroundServiceBase>(
        pref_service_.get(), nullptr);
  }

  void TearDown() override {
    service_.reset();
    pref_service_.reset();
  }

 protected:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestNtpCustomBackgroundServiceBase> service_;
};

TEST_F(NtpCustomBackgroundServiceBaseTest, Initialization) {
  EXPECT_NE(service_, nullptr);
}
