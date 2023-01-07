// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/sync_promo_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/base/command_line_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class SyncPromoUITest : public testing::Test {
 public:
  SyncPromoUITest() = default;

  SyncPromoUITest(const SyncPromoUITest&) = delete;
  SyncPromoUITest& operator=(const SyncPromoUITest&) = delete;

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    TestingProfile::Builder builder;
    profile_ = builder.Build();
  }

 protected:
  void DisableSync() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(syncer::kDisableSync);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

// Verifies that ShouldShowSyncPromo returns false if sync is disabled by
// policy.
TEST_F(SyncPromoUITest, ShouldShowSyncPromoSyncDisabled) {
  DisableSync();
  EXPECT_FALSE(SyncPromoUI::ShouldShowSyncPromo(profile_.get()));
}

// Verifies that ShouldShowSyncPromo returns true if all conditions to
// show the promo are met.
TEST_F(SyncPromoUITest, ShouldShowSyncPromoSyncEnabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // No sync promo on CrOS.
  EXPECT_FALSE(SyncPromoUI::ShouldShowSyncPromo(profile_.get()));
#else
  EXPECT_TRUE(SyncPromoUI::ShouldShowSyncPromo(profile_.get()));
#endif
}
