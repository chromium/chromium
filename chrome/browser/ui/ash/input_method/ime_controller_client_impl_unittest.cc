// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/ime_controller_client_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/ime_info.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/input_method/test_ime_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/fake_input_method_delegate.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/ime/ash/mock_ime_candidate_window_handler.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"

namespace {

using ::ash::input_method::FakeInputMethodDelegate;
using ::ash::input_method::InputMethodDescriptor;
using ::ash::input_method::InputMethodManager;
using ::ash::input_method::InputMethodUtil;
using ::ash::input_method::MockInputMethodManager;
using ::ui::ime::InputMethodMenuItem;
using ::ui::ime::InputMethodMenuManager;

// Used to look up IME names.
std::u16string GetLocalizedString(int resource_id) {
  return u"localized string";
}

// InputMethodManager with available IMEs.
class TestInputMethodManager : public MockInputMethodManager {
 public:
  class TestState : public MockInputMethodManager::State {
   public:
    TestState() {
      // Set up two input methods.
      std::string layout("us");
      std::vector<std::string> languages({"en-US"});
      InputMethodDescriptor ime1("id1", "name1", "indicator1", layout,
                                 languages, true /* is_login_keyboard */,
                                 GURL(), GURL(),
                                 /*handwriting_language=*/std::nullopt);
      InputMethodDescriptor ime2("id2", "name2", "indicator2", layout,
                                 languages, false /* is_login_keyboard */,
                                 GURL(), GURL(),
                                 /*handwriting_language=*/std::nullopt);
      current_ime_id_ = ime1.id();
      input_methods_ = {ime1, ime2};
    }

    TestState(const TestState&) = delete;
    TestState& operator=(const TestState&) = delete;

    // MockInputMethodManager::State:
    void ChangeInputMethod(const std::string& input_method_id,
                           bool show_message) override {
      ++change_input_method_count_;
      current_ime_id_ = input_method_id;
      last_show_message_ = show_message;
    }
    std::vector<InputMethodDescriptor>
    GetEnabledInputMethodsSortedByLocalizedDisplayNames() const override {
      return input_methods_;
    }
    const InputMethodDescriptor* GetInputMethodFromId(
        const std::string& input_method_id) const override {
      for (const InputMethodDescriptor& descriptor : input_methods_) {
        if (input_method_id == descriptor.id())
          return &descriptor;
      }
      return nullptr;
    }
    InputMethodDescriptor GetCurrentInputMethod() const override {
      for (const InputMethodDescriptor& descriptor : input_methods_) {
        if (current_ime_id_ == descriptor.id())
          return descriptor;
      }
      return InputMethodUtil::GetFallbackInputMethodDescriptor();
    }
    void SwitchToNextInputMethod() override { ++next_input_method_count_; }
    void SwitchToLastUsedInputMethod() override {
      ++previous_input_method_count_;
    }

    std::string current_ime_id_;
    std::vector<InputMethodDescriptor> input_methods_;
    int next_input_method_count_ = 0;
    int previous_input_method_count_ = 0;
    int change_input_method_count_ = 0;
    bool last_show_message_ = false;

   protected:
    friend base::RefCounted<InputMethodManager::State>;
    ~TestState() override {}
  };

  TestInputMethodManager() : state_(new TestState), util_(&delegate_) {}

  TestInputMethodManager(const TestInputMethodManager&) = delete;
  TestInputMethodManager& operator=(const TestInputMethodManager&) = delete;

  ~TestInputMethodManager() override = default;

  // MockInputMethodManager:
  void AddObserver(InputMethodManager::Observer* observer) override {
    ++add_observer_count_;
  }
  void AddImeMenuObserver(ImeMenuObserver* observer) override {
    ++add_menu_observer_count_;
  }
  void RemoveObserver(InputMethodManager::Observer* observer) override {
    ++remove_observer_count_;
  }
  void RemoveImeMenuObserver(ImeMenuObserver* observer) override {
    ++remove_menu_observer_count_;
  }
  void ActivateInputMethodMenuItem(const std::string& key) override {
    last_activate_menu_item_key_ = key;
  }
  void OverrideKeyboardKeyset(ash::input_method::ImeKeyset keyset) override {
    keyboard_keyset_ = keyset;
  }

  InputMethodUtil* GetInputMethodUtil() override { return &util_; }
  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override {
    return state_;
  }

  scoped_refptr<TestState> state_;
  int add_observer_count_ = 0;
  int remove_observer_count_ = 0;
  int add_menu_observer_count_ = 0;
  int remove_menu_observer_count_ = 0;
  std::string last_activate_menu_item_key_;
  ash::input_method::ImeKeyset keyboard_keyset_;
  FakeInputMethodDelegate delegate_;
  InputMethodUtil util_;
};

class ImeControllerClientImplTest : public testing::Test {
 public:
  ImeControllerClientImplTest() {
    input_method_manager_.delegate_.set_get_localized_string_callback(
        base::BindRepeating(&GetLocalizedString));
  }

  ImeControllerClientImplTest(const ImeControllerClientImplTest&) = delete;
  ImeControllerClientImplTest& operator=(const ImeControllerClientImplTest&) =
      delete;

  ~ImeControllerClientImplTest() override = default;

 protected:
  TestInputMethodManager input_method_manager_;

  // Mock of mojo interface in ash.
  TestImeController ime_controller_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ImeControllerClientImplTest, Construction) {
  std::unique_ptr<ImeControllerClientImpl> client =
      std::make_unique<ImeControllerClientImpl>(&input_method_manager_);
  client->Init();
  EXPECT_EQ(1, input_method_manager_.add_observer_count_);
  EXPECT_EQ(1, input_method_manager_.add_menu_observer_count_);

  client.reset();
  EXPECT_EQ(1, input_method_manager_.remove_observer_count_);
  EXPECT_EQ(1, input_method_manager_.remove_menu_observer_count_);
}

TEST_F(ImeControllerClientImplTest, SetImesManagedByPolicy) {
  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();

  client.SetImesManagedByPolicy(true);
  EXPECT_TRUE(ime_controller_.managed_by_policy_);
}

TEST_F(ImeControllerClientImplTest, CapsLock) {
  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();

  client.OnCapsLockChanged(true);
  EXPECT_TRUE(ime_controller_.is_caps_lock_enabled_);

  client.OnCapsLockChanged(false);
  EXPECT_FALSE(ime_controller_.is_caps_lock_enabled_);
}

TEST_F(ImeControllerClientImplTest, LayoutName) {
  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();

  client.OnLayoutChanging("us(dvorak)");
  EXPECT_EQ("us(dvorak)", ime_controller_.keyboard_layout_name_);

  client.OnLayoutChanging("us");
  EXPECT_EQ("us", ime_controller_.keyboard_layout_name_);
}

TEST_F(ImeControllerClientImplTest, ExtraInputEnabledStateChange) {
  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();

  client.OnExtraInputEnabledStateChange(true, true, false, false);
  EXPECT_TRUE(ime_controller_.is_extra_input_options_enabled_);
  EXPECT_TRUE(ime_controller_.is_emoji_enabled_);
  EXPECT_FALSE(ime_controller_.is_handwriting_enabled_);
  EXPECT_FALSE(ime_controller_.is_voice_enabled_);

  client.OnExtraInputEnabledStateChange(true, false, true, true);
  EXPECT_TRUE(ime_controller_.is_extra_input_options_enabled_);
  EXPECT_FALSE(ime_controller_.is_emoji_enabled_);
  EXPECT_TRUE(ime_controller_.is_handwriting_enabled_);
  EXPECT_TRUE(ime_controller_.is_voice_enabled_);

  client.OnExtraInputEnabledStateChange(false, false, false, false);
  EXPECT_FALSE(ime_controller_.is_extra_input_options_enabled_);
  EXPECT_FALSE(ime_controller_.is_emoji_enabled_);
  EXPECT_FALSE(ime_controller_.is_handwriting_enabled_);
  EXPECT_FALSE(ime_controller_.is_voice_enabled_);
}

TEST_F(ImeControllerClientImplTest, ShowImeMenuOnShelf) {
  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();

  client.ImeMenuActivationChanged(true);
  EXPECT_TRUE(ime_controller_.show_ime_menu_on_shelf_);
}

TEST_F(ImeControllerClientImplTest, InputMethodChanged) {
  auto mock_candidate_window =
      std::make_unique<ash::MockIMECandidateWindowHandler>();
  ash::IMEBridge::Get()->SetCandidateWindowHandler(mock_candidate_window.get());

  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();

  // Simulate a switch to IME 2.
  input_method_manager_.state_->current_ime_id_ = "id2";
  client.InputMethodChanged(&input_method_manager_, nullptr /* profile */,
                            false /* show_message */);

  // IME controller received the change and the list of available IMEs.
  EXPECT_EQ("id2", ime_controller_.current_ime_id_);
  ASSERT_EQ(2u, ime_controller_.available_imes_.size());
  EXPECT_EQ("id1", ime_controller_.available_imes_[0].id);
  EXPECT_EQ(u"name1", ime_controller_.available_imes_[0].name);
  EXPECT_EQ("id2", ime_controller_.available_imes_[1].id);
  EXPECT_EQ(u"name2", ime_controller_.available_imes_[1].name);
  EXPECT_FALSE(ime_controller_.show_mode_indicator_);

  // Simulate a switch and show message.
  input_method_manager_.state_->current_ime_id_ = "id1";
  client.InputMethodChanged(&input_method_manager_, nullptr /* profile */,
                            true /* show_message */);

  // Mode indicator should be shown.
  EXPECT_TRUE(ime_controller_.show_mode_indicator_);
}

TEST_F(ImeControllerClientImplTest, NoActiveState) {
  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();

  input_method_manager_.state_ = nullptr;
  client.InputMethodChanged(&input_method_manager_, nullptr /* profile */,
                            false /* show_message */);
  EXPECT_TRUE(ime_controller_.current_ime_id_.empty());
  EXPECT_TRUE(ime_controller_.available_imes_.empty());
  EXPECT_TRUE(ime_controller_.menu_items_.empty());
}

TEST_F(ImeControllerClientImplTest, MenuItemChanged) {
  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();
  InputMethodMenuItem item1("key1", "label1", true /* checked */);
  InputMethodMenuItem item2("key2", "label2", false /* checked */);

  // Setting the list triggers the InputMethodMenuItemChanged event.
  InputMethodMenuManager::GetInstance()->SetCurrentInputMethodMenuItemList(
      {item1, item2});

  // IME controller received the menu items.
  ASSERT_EQ(2u, ime_controller_.menu_items_.size());
  EXPECT_EQ("key1", ime_controller_.menu_items_[0].key);
  EXPECT_TRUE(ime_controller_.menu_items_[0].checked);
  EXPECT_EQ("key2", ime_controller_.menu_items_[1].key);
  EXPECT_FALSE(ime_controller_.menu_items_[1].checked);
}

TEST_F(ImeControllerClientImplTest, SwitchToNextIme) {
  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();
  client.SwitchToNextIme();
  EXPECT_EQ(1, input_method_manager_.state_->next_input_method_count_);
}

TEST_F(ImeControllerClientImplTest, SwitchToPreviousIme) {
  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();
  client.SwitchToLastUsedIme();
  EXPECT_EQ(1, input_method_manager_.state_->previous_input_method_count_);
}

TEST_F(ImeControllerClientImplTest, SwitchImeById) {
  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();
  client.SwitchImeById("id2", true /* show_message */);
  EXPECT_EQ(1, input_method_manager_.state_->change_input_method_count_);
  EXPECT_EQ("id2", input_method_manager_.state_->current_ime_id_);
  EXPECT_TRUE(input_method_manager_.state_->last_show_message_);

  client.SwitchImeById("id1", false /* show_message */);
  EXPECT_EQ(2, input_method_manager_.state_->change_input_method_count_);
  EXPECT_EQ("id1", input_method_manager_.state_->current_ime_id_);
  EXPECT_FALSE(input_method_manager_.state_->last_show_message_);
}

TEST_F(ImeControllerClientImplTest, ActivateImeMenuItem) {
  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();
  client.ActivateImeMenuItem("key1");
  EXPECT_EQ("key1", input_method_manager_.last_activate_menu_item_key_);
}

TEST_F(ImeControllerClientImplTest, OverrideKeyboardKeyset) {
  ImeControllerClientImpl client(&input_method_manager_);
  client.Init();
  bool callback_called = false;
  client.OverrideKeyboardKeyset(
      ash::input_method::ImeKeyset::kEmoji,
      base::BindLambdaForTesting(
          [&callback_called]() { callback_called = true; }));
  EXPECT_EQ(ash::input_method::ImeKeyset::kEmoji,
            input_method_manager_.keyboard_keyset_);
  EXPECT_TRUE(callback_called);
}

}  // namespace
