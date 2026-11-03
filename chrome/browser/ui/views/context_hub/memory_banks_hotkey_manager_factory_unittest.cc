// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/context_hub/memory_banks_hotkey_manager_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using browser::context_hub::mojom::kMemoryBanksSearchPalette;
using context_hub::features::kMemoryBanks;

class MemoryBanksHotkeyManagerFactoryTest : public testing::Test {
 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(MemoryBanksHotkeyManagerFactoryTest, CreatesManagerForRegularProfile) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kMemoryBanks, kMemoryBanksSearchPalette},
      /*disabled_features=*/{});
  TestingProfile profile;

  EXPECT_TRUE(MemoryBanksHotkeyManagerFactory::GetForProfile(&profile));
}

TEST_F(MemoryBanksHotkeyManagerFactoryTest, NoManagerForIncognito) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kMemoryBanks, kMemoryBanksSearchPalette},
      /*disabled_features=*/{});
  TestingProfile profile;

  EXPECT_FALSE(MemoryBanksHotkeyManagerFactory::GetForProfile(
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

TEST_F(MemoryBanksHotkeyManagerFactoryTest, NoManagerForGuestProfile) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kMemoryBanks, kMemoryBanksSearchPalette},
      /*disabled_features=*/{});
  std::unique_ptr<TestingProfile> guest_profile =
      TestingProfile::Builder().SetGuestSession().Build();

  EXPECT_FALSE(
      MemoryBanksHotkeyManagerFactory::GetForProfile(guest_profile.get()));
}

TEST_F(MemoryBanksHotkeyManagerFactoryTest, NoManagerWhenMemoryBanksDisabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kMemoryBanksSearchPalette},
      /*disabled_features=*/{kMemoryBanks});
  TestingProfile profile;

  EXPECT_FALSE(MemoryBanksHotkeyManagerFactory::GetForProfile(&profile));
}

TEST_F(MemoryBanksHotkeyManagerFactoryTest, NoManagerWhenPaletteDisabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kMemoryBanks},
      /*disabled_features=*/{kMemoryBanksSearchPalette});
  TestingProfile profile;

  EXPECT_FALSE(MemoryBanksHotkeyManagerFactory::GetForProfile(&profile));
}

}  // namespace
