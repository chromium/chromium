// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/context_hub/memory_banks_hotkey_manager.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_banks_shortcut.h"
#include "chrome/browser/context_hub/prefs.h"
#include "chrome/browser/ui/views/context_hub/memory_banks_hotkey_manager_factory.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

using browser::context_hub::mojom::kMemoryBanksSearchPalette;
using browser::context_hub::mojom::MemoryBanksShortcutStatus;
using context_hub::features::kMemoryBanks;

class MemoryBanksHotkeyManagerTest : public testing::Test {
 public:
  MemoryBanksHotkeyManagerTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kMemoryBanks, kMemoryBanksSearchPalette},
        /*disabled_features=*/{});
    // Created after the features are set, since the manager is created with
    // the profile.
    profile_ = std::make_unique<TestingProfile>();
  }

 protected:
  MemoryBanksHotkeyManager* manager() {
    return MemoryBanksHotkeyManagerFactory::GetForProfile(profile_.get());
  }

  PrefService* prefs() { return profile_->GetPrefs(); }

  // Saves `shortcut` the same way the palette Settings panel does.
  MemoryBanksShortcutStatus SetShortcut(const ui::Accelerator& shortcut) {
    return context_hub::SetBrowseMemoryBanksShortcutInPrefs(
        *prefs(), ui::Command::AcceleratorToString(shortcut));
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(MemoryBanksHotkeyManagerTest, NoShortcutByDefault) {
  ASSERT_TRUE(manager());
  EXPECT_TRUE(manager()->accelerator_for_testing().IsEmpty());
}

TEST_F(MemoryBanksHotkeyManagerTest, FollowsShortcutPref) {
  const ui::Accelerator shortcut =
      context_hub::GetSuggestedBrowseMemoryBanksShortcut();
  if (shortcut.IsEmpty()) {
    GTEST_SKIP() << "No suggested shortcut on this platform.";
  }

  ASSERT_EQ(MemoryBanksShortcutStatus::kValid, SetShortcut(shortcut));
  EXPECT_EQ(shortcut, manager()->accelerator_for_testing());

  // Ctrl+Shift+K (Command+Shift+K on macOS) is the fallback suggestion, so
  // it's free on every platform and different from `shortcut`.
  const ui::Accelerator other_shortcut(
      ui::VKEY_K, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN);
  ASSERT_NE(shortcut, other_shortcut);
  ASSERT_EQ(MemoryBanksShortcutStatus::kValid, SetShortcut(other_shortcut));
  EXPECT_EQ(other_shortcut, manager()->accelerator_for_testing());

  ASSERT_EQ(MemoryBanksShortcutStatus::kValid,
            context_hub::SetBrowseMemoryBanksShortcutInPrefs(*prefs(), ""));
  EXPECT_TRUE(manager()->accelerator_for_testing().IsEmpty());
}

TEST_F(MemoryBanksHotkeyManagerTest, IgnoresInvalidStoredShortcut) {
  const ui::Accelerator shortcut(
      ui::VKEY_K, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN);
  ASSERT_EQ(MemoryBanksShortcutStatus::kValid, SetShortcut(shortcut));
  ASSERT_EQ(shortcut, manager()->accelerator_for_testing());

  // "K" has no Ctrl, Alt or Command, so it can't be used.
  prefs()->SetString(context_hub::prefs::kContextHubBrowseMemoryBanksShortcut,
                     "K");
  EXPECT_TRUE(manager()->accelerator_for_testing().IsEmpty());
}

TEST_F(MemoryBanksHotkeyManagerTest, PressingShortcutWithoutWindowDoesNothing) {
  const ui::Accelerator shortcut(
      ui::VKEY_K, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN);
  ASSERT_EQ(MemoryBanksShortcutStatus::kValid, SetShortcut(shortcut));

  // The test profile has no browser window to open the palette in.
  EXPECT_FALSE(manager()->AcceleratorPressed(shortcut));
}

}  // namespace
