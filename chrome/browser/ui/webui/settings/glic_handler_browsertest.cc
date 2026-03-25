// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/glic_handler.h"

#include <memory>
#include <tuple>

#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permissions_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

using testing::_;
using testing::Return;

namespace settings {

class MockActorLoginPermissionsManager
    : public actor_login::ActorLoginPermissionsManager {
 public:
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(void,
              RevokePermission,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void,
              GetAllPermissions,
              (const syncer::SyncService*, GetAllPermissionsResult),
              (override));
};

class GlicHandlerBrowserTest : public InProcessBrowserTest {
 public:
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

    // Destroy the handler before the profile is destroyed to avoid dangling
    // pointers to KeyedServices.
    glic_handler_.reset();
    web_ui_.reset();
  }

  GlicHandler* glic_handler() { return glic_handler_.get(); }
  content::TestWebUI* web_ui() { return web_ui_.get(); }

 private:
  std::unique_ptr<GlicHandler> glic_handler_;
  std::unique_ptr<content::TestWebUI> web_ui_;
};

// TODO(crbug.com/388101855): Remove buildflag when GlobalAcceleratorListener
// supports Linux Wayland.
#if !BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
IN_PROC_BROWSER_TEST_F(GlicHandlerBrowserTest, UpdateShortcutSuspension) {
  auto* const global_accelerator_listener =
      ui::GlobalAcceleratorListener::GetInstance();
  EXPECT_FALSE(global_accelerator_listener->IsShortcutHandlingSuspended());

  glic_handler()->HandleSetShortcutSuspensionState(
      base::ListValue().Append(true));
  EXPECT_TRUE(global_accelerator_listener->IsShortcutHandlingSuspended());

  glic_handler()->HandleSetShortcutSuspensionState(
      base::ListValue().Append(false));
  EXPECT_FALSE(global_accelerator_listener->IsShortcutHandlingSuspended());
}
#endif  //  !BUILDFLAG(SUPPORTS_OZONE_WAYLAND)

// TODO(crbug.com/416160303): Enable the test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_UpdateGlicShortcut DISABLED_UpdateGlicShortcut
#else
#define MAYBE_UpdateGlicShortcut UpdateGlicShortcut
#endif
IN_PROC_BROWSER_TEST_F(GlicHandlerBrowserTest, MAYBE_UpdateGlicShortcut) {
  const ui::Accelerator invalid_shortcut(ui::VKEY_A, ui::EF_NONE);
  glic_handler()->HandleSetGlicShortcut(
      base::ListValue()
          .Append("callback_id")
          .Append(ui::Command::AcceleratorToString(invalid_shortcut)));
  ui::Accelerator saved_hotkey =
      glic::GlicLauncherConfiguration::GetGlobalHotkey();
  EXPECT_EQ(ui::VKEY_UNKNOWN, saved_hotkey.key_code());
  EXPECT_EQ(ui::EF_NONE, saved_hotkey.modifiers());

  const ui::Accelerator valid_shortcut(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  glic_handler()->HandleSetGlicShortcut(
      base::ListValue()
          .Append("callback_id")
          .Append(ui::Command::AcceleratorToString(valid_shortcut)));
  saved_hotkey = glic::GlicLauncherConfiguration::GetGlobalHotkey();
  EXPECT_EQ(valid_shortcut.key_code(), saved_hotkey.key_code());
  EXPECT_EQ(valid_shortcut.modifiers(), saved_hotkey.modifiers());
}

IN_PROC_BROWSER_TEST_F(GlicHandlerBrowserTest, GetActorLoginPermissions) {
  glic_handler()->AllowJavascript();

  auto mock_manager =
      std::make_unique<testing::NiceMock<MockActorLoginPermissionsManager>>();
  password_manager::ActorLoginPermission permission;
  permission.domain_info.signon_realm = "example.com";
  permission.username = u"user";
  permission.domain_info.name = "Example";
  permission.favicon_url = GURL("http://example.com/favicon.ico");
  EXPECT_CALL(*mock_manager, GetAllPermissions)
      .WillOnce(base::test::RunOnceCallback<1>(
          base::flat_set<password_manager::ActorLoginPermission>{permission}));

  glic_handler()->observation_.Reset();
  glic_handler()->actor_login_permissions_manager_ = std::move(mock_manager);
  glic_handler()->observation_.Observe(
      glic_handler()->actor_login_permissions_manager_.get());

  base::ListValue args;
  args.Append("callback-id");
  glic_handler()->HandleGetActorLoginPermissions(args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  EXPECT_EQ("callback-id", data.arg1()->GetString());
  EXPECT_TRUE(data.arg2()->GetBool());
  const base::ListValue& result = data.arg3()->GetList();
  ASSERT_EQ(1u, result.size());
  const base::DictValue& permission_dict = result[0].GetDict();
  EXPECT_EQ("example.com", *permission_dict.FindString("signonRealm"));
}

IN_PROC_BROWSER_TEST_F(GlicHandlerBrowserTest, RevokeActorLoginPermission) {
  glic_handler()->AllowJavascript();

  auto mock_manager =
      std::make_unique<testing::NiceMock<MockActorLoginPermissionsManager>>();
  EXPECT_CALL(*mock_manager, RevokePermission("example.com", "user"));

  glic_handler()->observation_.Reset();
  glic_handler()->actor_login_permissions_manager_ = std::move(mock_manager);
  glic_handler()->observation_.Observe(
      glic_handler()->actor_login_permissions_manager_.get());

  base::ListValue args;
  args.Append("example.com");
  args.Append("user");
  glic_handler()->HandleRevokeActorLoginPermission(args);
}
}  // namespace settings
