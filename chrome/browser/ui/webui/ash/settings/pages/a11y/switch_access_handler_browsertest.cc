// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/a11y/switch_access_handler.h"

#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "url/gurl.h"

namespace ash::settings {

class TestSwitchAccessHandler : public SwitchAccessHandler {
 public:
  explicit TestSwitchAccessHandler(content::TestWebUI* web_ui,
                                   PrefService* pref_service)
      : SwitchAccessHandler(pref_service) {
    set_web_ui(web_ui);
    skip_pre_target_handler_for_testing_ = true;
  }

  void set_on_listeners_added(base::OnceClosure callback) {
    on_pre_target_handler_added_for_testing_ = std::move(callback);
  }

  ~TestSwitchAccessHandler() override = default;
};

class SwitchAccessHandlerTest : public InProcessBrowserTest {
 public:
  SwitchAccessHandlerTest() = default;
  ~SwitchAccessHandlerTest() override = default;
  SwitchAccessHandlerTest(const SwitchAccessHandlerTest&) = delete;
  SwitchAccessHandlerTest& operator=(const SwitchAccessHandlerTest&) = delete;

  void SetUpOnMainThread() override {
    handler_ = std::make_unique<TestSwitchAccessHandler>(
        &web_ui_, browser()->profile()->GetPrefs());
  }

  void TearDownOnMainThread() override { handler_.reset(); }

  void SetPaneActive() {
    handler()->HandleNotifySwitchAccessActionAssignmentPaneActive(/*args=*/{});
  }

  void SetPaneInactive() {
    handler()->HandleNotifySwitchAccessActionAssignmentPaneInactive(
        /*args=*/{});
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  TestSwitchAccessHandler* handler() { return handler_.get(); }

  void AssertKeyPressData(ui::KeyboardCode code, const std::string& name) {
    auto* call_data =
        FindCallForEvent("switch-access-got-key-press-for-assignment");
    ASSERT_NE(call_data, nullptr);
    ASSERT_EQ(call_data->args().size(), 2u);

    ASSERT_TRUE(call_data->arg2()->is_dict());
    auto& details = call_data->arg2()->GetDict();
    ASSERT_EQ(details.size(), 3u);

    std::optional<int> key_code = details.FindInt("keyCode");
    ASSERT_NE(key_code, std::nullopt);
    ASSERT_EQ(*key_code, code);

    const std::string* key = details.FindString("key");
    ASSERT_NE(key, nullptr);
    ASSERT_EQ(*key, name);

    const std::string* device = details.FindString("device");
    ASSERT_NE(device, nullptr);
    ASSERT_EQ(*device, "unknown");
  }

  content::TestWebUI::CallData* FindCallForEvent(const std::string& event) {
    auto iter = std::find_if(
        web_ui()->call_data().cbegin(), web_ui()->call_data().cend(),
        [&event](const std::unique_ptr<content::TestWebUI::CallData>& data) {
          if (data->function_name() != "cr.webUIListenerCallback") {
            return false;
          }
          if (data->args().size() < 1) {
            return false;
          }
          if (!data->arg1()->is_string()) {
            return false;
          }
          return data->arg1()->GetString() == event;
        });

    if (iter != web_ui()->call_data().cend()) {
      return iter->get();
    }
    return nullptr;
  }

 private:
  std::unique_ptr<TestSwitchAccessHandler> handler_;
  content::TestWebUI web_ui_;
};

IN_PROC_BROWSER_TEST_F(SwitchAccessHandlerTest,
                       SwitchAssignmentReceivesKeyEvents) {
  // First test the case where JavaScript is not yet allowed.
  ASSERT_FALSE(handler()->IsJavascriptAllowed());
  base::RunLoop run_loop;
  handler()->set_on_listeners_added(run_loop.QuitClosure());
  SetPaneActive();
  // Confirm the listeners are added.
  run_loop.Run();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                         ui::EF_NONE);
  handler()->OnKeyEvent(&key_event);

  // Verify the method was called correctly.
  AssertKeyPressData(ui::VKEY_SPACE, "Space");

  web_ui()->ClearTrackedCalls();
  SetPaneInactive();

  // Next test the case where JavaScript has already been allowed.
  ASSERT_TRUE(handler()->IsJavascriptAllowed());
  base::RunLoop run_loop_2;
  handler()->set_on_listeners_added(run_loop_2.QuitClosure());
  SetPaneActive();
  // Confirm the listeners are added.
  run_loop_2.Run();

  key_event =
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_TAB, ui::EF_NONE);
  handler()->OnKeyEvent(&key_event);

  // Verify the method was called correctly.
  AssertKeyPressData(ui::VKEY_TAB, "Tab");
}

}  // namespace ash::settings
