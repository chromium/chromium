// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/glic_handler.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace settings {
class GlicHandlerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton}, {});

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(
        browser()->tab_strip_model()->GetActiveWebContents());
    glic_handler_ = std::make_unique<GlicHandler>();
    glic_handler_->SetWebUIForTesting(web_ui_.get());
  }

  void TearDownOnMainThread() override {
    // Disable glic so that the glic_background_mode_manager won't prevent the
    // browser process from closing which causes the test to hang.
    g_browser_process->local_state()->SetBoolean(
        glic::prefs::kGlicLauncherEnabled, false);
  }

  GlicHandler* glic_handler() { return glic_handler_.get(); }

 private:
  std::unique_ptr<GlicHandler> glic_handler_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::TestWebUI> web_ui_;
};

// TODO(crbug.com/388101855): Remove buildflag when GlobalAcceleratorListener
// supports Linux Wayland.
#if !BUILDFLAG(IS_OZONE_WAYLAND)
IN_PROC_BROWSER_TEST_F(GlicHandlerBrowserTest, UpdateShortcutSuspension) {
  auto* const global_accelerator_listener =
      ui::GlobalAcceleratorListener::GetInstance();
  EXPECT_FALSE(global_accelerator_listener->IsShortcutHandlingSuspended());

  glic_handler()->HandleSetShortcutSuspensionState(
      base::Value::List().Append(true));
  EXPECT_TRUE(global_accelerator_listener->IsShortcutHandlingSuspended());

  glic_handler()->HandleSetShortcutSuspensionState(
      base::Value::List().Append(false));
  EXPECT_FALSE(global_accelerator_listener->IsShortcutHandlingSuspended());
}
#endif  //  !BUILDFLAG(IS_OZONE_WAYLAND)

// TODO(crbug.com/416160303): Enable the test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_UpdateGlicShortcut DISABLED_UpdateGlicShortcut
#else
#define MAYBE_UpdateGlicShortcut UpdateGlicShortcut
#endif
IN_PROC_BROWSER_TEST_F(GlicHandlerBrowserTest, MAYBE_UpdateGlicShortcut) {
  const ui::Accelerator invalid_shortcut(ui::VKEY_A, ui::EF_NONE);
  glic_handler()->HandleSetGlicShortcut(
      base::Value::List()
          .Append("callback_id")
          .Append(ui::Command::AcceleratorToString(invalid_shortcut)));
  ui::Accelerator saved_hotkey =
      glic::GlicLauncherConfiguration::GetGlobalHotkey();
  EXPECT_EQ(ui::VKEY_UNKNOWN, saved_hotkey.key_code());
  EXPECT_EQ(ui::EF_NONE, saved_hotkey.modifiers());

  const ui::Accelerator valid_shortcut(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  glic_handler()->HandleSetGlicShortcut(
      base::Value::List()
          .Append("callback_id")
          .Append(ui::Command::AcceleratorToString(valid_shortcut)));
  saved_hotkey = glic::GlicLauncherConfiguration::GetGlobalHotkey();
  EXPECT_EQ(valid_shortcut.key_code(), saved_hotkey.key_code());
  EXPECT_EQ(valid_shortcut.modifiers(), saved_hotkey.modifiers());
}
}  // namespace settings
