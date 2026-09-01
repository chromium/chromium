// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/locked_state/locked_state_controller.h"

#include <array>

#include "ash/wm/window_state.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"

namespace chromeos {
namespace {

class LockedStateControllerBrowserTest : public InProcessBrowserTest {
 public:
  LockedStateControllerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kUseUnifiedLockedStateController);
  }
  ~LockedStateControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    controller_ = LockedStateController::From(browser());
    ASSERT_NE(controller_, nullptr);
  }

  void TearDownOnMainThread() override {
    controller_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  bool IsBrowserLockedFullscreen() {
    aura::Window* window = browser()->GetWindow()->GetNativeWindow();
    auto* window_state = ash::WindowState::Get(window);
    return window_state && window_state->IsLockedFullscreen();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<LockedStateController> controller_ = nullptr;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest, Initialization) {
  EXPECT_FALSE(controller_->IsLocked());
  EXPECT_EQ(controller_->GetState(), LockedState::kUnlocked);
  EXPECT_EQ(controller_->GetCapabilities(), LockedStateCapabilities());
  EXPECT_FALSE(IsBrowserLockedFullscreen());
}

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest, LockExtensionSuccess) {
  ASSERT_TRUE(controller_->Lock(LockedState::kExtensionLocked));
  EXPECT_TRUE(controller_->IsLocked());
  EXPECT_EQ(controller_->GetState(), LockedState::kExtensionLocked);
  EXPECT_TRUE(IsBrowserLockedFullscreen());

  LockedStateCapabilities expected_caps{
      .supports_tab_strip = false,
      .allow_find = false,
      .allow_browser_navigation = false,
      .allow_tab_modification = false,
      .use_immersive_mode = false,
      .focus_toolbar_on_lock = false,
      .context_menu_policy = ContextMenuPolicy::kBlockAll,
  };
  EXPECT_EQ(controller_->GetCapabilities(), expected_caps);

  // Clean up
  EXPECT_TRUE(controller_->Unlock(LockedState::kExtensionLocked));
  EXPECT_FALSE(IsBrowserLockedFullscreen());
}

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest, LockOnTaskSuccess) {
  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskLocked));
  EXPECT_TRUE(controller_->IsLocked());
  EXPECT_EQ(controller_->GetState(), LockedState::kOnTaskLocked);
  EXPECT_TRUE(IsBrowserLockedFullscreen());

  LockedStateCapabilities expected_caps{
      .supports_tab_strip = true,
      .allow_find = true,
      .allow_browser_navigation = true,
      .allow_tab_modification = false,
      .use_immersive_mode = true,
      .focus_toolbar_on_lock = true,
      .context_menu_policy = ContextMenuPolicy::kLimited,
  };
  EXPECT_EQ(controller_->GetCapabilities(), expected_caps);

  // Clean up
  EXPECT_TRUE(controller_->Unlock(LockedState::kOnTaskLocked));
  EXPECT_FALSE(IsBrowserLockedFullscreen());
}

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest,
                       LockOnTaskPreparedSuccess) {
  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskPrepared));
  EXPECT_FALSE(controller_->IsLocked());
  EXPECT_TRUE(controller_->IsLockedForOnTask());
  EXPECT_EQ(controller_->GetState(), LockedState::kOnTaskPrepared);
  EXPECT_FALSE(IsBrowserLockedFullscreen());

  LockedStateCapabilities expected_caps{
      .supports_tab_strip = true,
      .allow_find = true,
      .allow_browser_navigation = true,
      .allow_tab_modification = false,
      .use_immersive_mode = true,
      .focus_toolbar_on_lock = true,
      .context_menu_policy = ContextMenuPolicy::kLimited,
  };
  EXPECT_EQ(controller_->GetCapabilities(), expected_caps);

  // Clean up
  EXPECT_TRUE(controller_->Unlock(LockedState::kOnTaskLocked));
  EXPECT_FALSE(IsBrowserLockedFullscreen());
}

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest,
                       TransitionFromPreparedToLocked) {
  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskPrepared));
  EXPECT_FALSE(IsBrowserLockedFullscreen());

  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskLocked));
  EXPECT_TRUE(controller_->IsLocked());
  EXPECT_TRUE(controller_->IsLockedForOnTask());
  EXPECT_EQ(controller_->GetState(), LockedState::kOnTaskLocked);
  EXPECT_TRUE(IsBrowserLockedFullscreen());

  // Clean up
  EXPECT_TRUE(controller_->Unlock(LockedState::kOnTaskLocked));
  EXPECT_FALSE(IsBrowserLockedFullscreen());
}

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest,
                       TransitionFromPreparedToPaused) {
  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskPrepared));
  EXPECT_FALSE(IsBrowserLockedFullscreen());

  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskLockedPaused));
  EXPECT_TRUE(controller_->IsLocked());
  EXPECT_TRUE(controller_->IsLockedForOnTask());
  EXPECT_EQ(controller_->GetState(), LockedState::kOnTaskLockedPaused);
  EXPECT_TRUE(IsBrowserLockedFullscreen());

  LockedStateCapabilities expected_caps{
      .supports_tab_strip = true,
      .allow_find = true,
      .allow_browser_navigation = true,
      .allow_tab_modification = false,
      .use_immersive_mode = false,
      .focus_toolbar_on_lock = false,
      .context_menu_policy = ContextMenuPolicy::kLimited,
  };
  EXPECT_EQ(controller_->GetCapabilities(), expected_caps);

  // Clean up
  EXPECT_TRUE(controller_->Unlock(LockedState::kOnTaskLocked));
  EXPECT_FALSE(IsBrowserLockedFullscreen());
}

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest,
                       TransitionBetweenOnTaskAndPaused) {
  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskLocked));
  EXPECT_TRUE(IsBrowserLockedFullscreen());

  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskLockedPaused));
  EXPECT_EQ(controller_->GetState(), LockedState::kOnTaskLockedPaused);
  EXPECT_TRUE(IsBrowserLockedFullscreen());

  LockedStateCapabilities expected_caps{
      .supports_tab_strip = true,
      .allow_find = true,
      .allow_browser_navigation = true,
      .allow_tab_modification = false,
      .use_immersive_mode = false,
      .focus_toolbar_on_lock = false,
      .context_menu_policy = ContextMenuPolicy::kLimited,
  };
  EXPECT_EQ(controller_->GetCapabilities(), expected_caps);

  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskLocked));
  EXPECT_EQ(controller_->GetState(), LockedState::kOnTaskLocked);
  EXPECT_TRUE(controller_->GetCapabilities().use_immersive_mode);
  EXPECT_TRUE(IsBrowserLockedFullscreen());

  // Clean up
  EXPECT_TRUE(controller_->Unlock(LockedState::kOnTaskLocked));
  EXPECT_FALSE(IsBrowserLockedFullscreen());
}

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest,
                       LockWhenAlreadyLockedSameStateNoOp) {
  ASSERT_TRUE(controller_->Lock(LockedState::kExtensionLocked));
  EXPECT_TRUE(IsBrowserLockedFullscreen());

  ASSERT_TRUE(controller_->Lock(LockedState::kExtensionLocked));
  EXPECT_TRUE(IsBrowserLockedFullscreen());

  // Clean up
  EXPECT_TRUE(controller_->Unlock(LockedState::kExtensionLocked));
  EXPECT_FALSE(IsBrowserLockedFullscreen());
}
IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest,
                       LockForExtensionLockedWhenAlreadyOnTaskLockedFails) {
  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskLocked));
  EXPECT_FALSE(controller_->Lock(LockedState::kExtensionLocked));
  EXPECT_EQ(controller_->GetState(), LockedState::kOnTaskLocked);

  // Clean up
  EXPECT_TRUE(controller_->Unlock(LockedState::kOnTaskLocked));
}

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest,
                       LockForOnTaskLockedWhenAlreadyExtensionLockedFails) {
  ASSERT_TRUE(controller_->Lock(LockedState::kExtensionLocked));
  EXPECT_FALSE(controller_->Lock(LockedState::kOnTaskLocked));
  EXPECT_EQ(controller_->GetState(), LockedState::kExtensionLocked);

  // Clean up
  EXPECT_TRUE(controller_->Unlock(LockedState::kExtensionLocked));
}

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest, UnlockSuccess) {
  ASSERT_TRUE(controller_->Lock(LockedState::kExtensionLocked));
  EXPECT_TRUE(IsBrowserLockedFullscreen());

  EXPECT_TRUE(controller_->Unlock(LockedState::kExtensionLocked));
  EXPECT_FALSE(controller_->IsLocked());
  EXPECT_EQ(controller_->GetState(), LockedState::kUnlocked);
  EXPECT_EQ(controller_->GetCapabilities(), LockedStateCapabilities());
  EXPECT_FALSE(IsBrowserLockedFullscreen());
}

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest,
                       UnlockOnTaskPausedWithOnTaskSuccess) {
  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskLocked));
  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskLockedPaused));
  EXPECT_TRUE(IsBrowserLockedFullscreen());

  EXPECT_TRUE(controller_->Unlock(LockedState::kOnTaskLocked));
  EXPECT_FALSE(controller_->IsLocked());
  EXPECT_FALSE(IsBrowserLockedFullscreen());
}

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest,
                       UnlockWhenUnlockedNoOp) {
  EXPECT_TRUE(controller_->Unlock(LockedState::kExtensionLocked));
  EXPECT_FALSE(IsBrowserLockedFullscreen());
}

IN_PROC_BROWSER_TEST_F(LockedStateControllerBrowserTest,
                       CommandIdEnabledAndBlockedAcrossStates) {
  constexpr auto kClipboardCommands =
      std::to_array({IDC_CUT, IDC_COPY, IDC_PASTE});
  constexpr auto kPageNavCommands = std::to_array(
      {IDC_BACK, IDC_FORWARD, IDC_RELOAD, IDC_RELOAD_BYPASSING_CACHE,
       IDC_RELOAD_CLEARING_CACHE, IDC_STOP});
  constexpr auto kAllowedContentContextCommands = std::to_array(
      {IDC_CONTENT_CONTEXT_COPYIMAGE, IDC_CONTENT_CONTEXT_COPYIMAGELOCATION});
  constexpr auto kTabManagementCommands = std::to_array(
      {IDC_SELECT_NEXT_TAB, IDC_SELECT_PREVIOUS_TAB, IDC_SELECT_TAB_0,
       IDC_SELECT_TAB_1, IDC_SELECT_TAB_2, IDC_SELECT_TAB_3, IDC_SELECT_TAB_4,
       IDC_SELECT_TAB_5, IDC_SELECT_TAB_6, IDC_SELECT_TAB_7});
  constexpr auto kFindInPageCommands = std::to_array(
      {IDC_FIND, IDC_FIND_NEXT, IDC_FIND_PREVIOUS, IDC_CLOSE_FIND_OR_STOP});
  constexpr auto kExtensionCustomCommands =
      std::to_array({IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST,
                     IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + 500});
  constexpr auto kDisallowedCommands =
      std::to_array({IDC_EXIT, IDC_PRINT, IDC_VIEW_SOURCE, IDC_NEW_TAB,
                     IDC_CONTENT_CONTEXT_OPENLINKNEWTAB});

  // 1. Unlocked State
  for (int id : kClipboardCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kPageNavCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kAllowedContentContextCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kTabManagementCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kFindInPageCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kExtensionCustomCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kDisallowedCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }

  // 2. Extension Locked State
  ASSERT_TRUE(controller_->Lock(LockedState::kExtensionLocked));
  for (int id : kClipboardCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kPageNavCommands) {
    EXPECT_FALSE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kAllowedContentContextCommands) {
    EXPECT_FALSE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kTabManagementCommands) {
    EXPECT_FALSE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kFindInPageCommands) {
    EXPECT_FALSE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kExtensionCustomCommands) {
    EXPECT_FALSE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kDisallowedCommands) {
    EXPECT_FALSE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  EXPECT_TRUE(controller_->Unlock(LockedState::kExtensionLocked));

  // 3. OnTask Prepared State
  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskPrepared));
  for (int id : kClipboardCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kPageNavCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kAllowedContentContextCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kTabManagementCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kFindInPageCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kExtensionCustomCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kDisallowedCommands) {
    EXPECT_FALSE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  EXPECT_TRUE(controller_->Unlock(LockedState::kOnTaskLocked));

  // 4. OnTask Locked State
  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskLocked));
  for (int id : kClipboardCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kPageNavCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kAllowedContentContextCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kTabManagementCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kFindInPageCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kExtensionCustomCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kDisallowedCommands) {
    EXPECT_FALSE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }

  // 5. OnTask Locked Paused State
  ASSERT_TRUE(controller_->Lock(LockedState::kOnTaskLockedPaused));
  for (int id : kClipboardCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kPageNavCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kAllowedContentContextCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kTabManagementCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kFindInPageCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_FALSE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kExtensionCustomCommands) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  for (int id : kDisallowedCommands) {
    EXPECT_FALSE(controller_->IsCommandIdEnabled(id));
    EXPECT_TRUE(controller_->IsCommandUpdateBlocked(id));
  }
  EXPECT_TRUE(controller_->Unlock(LockedState::kOnTaskLocked));
}

}  // namespace chromeos
