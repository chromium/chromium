// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api_nonchromeos.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"

namespace extensions {

class InputImeApiTest : public ExtensionApiTest {
 public:
  InputImeApiTest() {}

 protected:
  // Sets the private flag of |track_key_events_for_testing_| in InputMethod.
  void SetTrackKeyEvents(ui::InputMethod* input_method, bool track) {
    input_method->track_key_events_for_testing_ = track;
  }

  // Returns true if the key events get from input method equals to the expected
  // key events.
  bool CompareKeyEvents(
      const std::vector<std::unique_ptr<ui::KeyEvent>>& expected_key_events,
      ui::InputMethod* input_method) {
    if (expected_key_events.size() != GetKeyEvents(input_method).size())
      return false;
    for (size_t i = 0; i < expected_key_events.size(); i++) {
      ui::KeyEvent* event1 = expected_key_events[i].get();
      ui::KeyEvent* event2 = GetKeyEvents(input_method)[i].get();
      if (event1->type() != event2->type() ||
          event1->key_code() != event2->key_code() ||
          event1->flags() != event2->flags()) {
        return false;
      }
    }
    return true;
  }

 private:
  // Returns the tracked key events of using input.ime.sendKeyEvents API.
  const std::vector<std::unique_ptr<ui::KeyEvent>>& GetKeyEvents(
      ui::InputMethod* input_method) {
    return input_method->GetKeyEventsForTesting();
  }

  DISALLOW_COPY_AND_ASSIGN(InputImeApiTest);
};

// TODO(crbug.com/882338) This test fails basically once per try run.
// See bug for details.
IN_PROC_BROWSER_TEST_F(InputImeApiTest, DISABLED_BasicApiTest) {
  // Manipulates the focused text input client because the follow cursor
  // window requires the text input focus.
  ui::InputMethod* input_method =
      browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod();
  std::unique_ptr<ui::DummyTextInputClient> client(
      new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
  input_method->SetFocusedTextInputClient(client.get());
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  base::AutoReset<bool> auto_reset_disable_bubble(
      &InputImeActivateFunction::disable_bubble_for_testing_, true);
  SetTrackKeyEvents(input_method, true);

  // Listens for the input.ime.onBlur event.
  ExtensionTestMessageListener blur_listener("get_blur_event", false);

  ASSERT_TRUE(RunExtensionTest("input_ime_nonchromeos")) << message_;

  // Test the input.ime.commitText API.
  const std::vector<base::string16>& insert_text_history =
      client->insert_text_history();
  ASSERT_EQ(1UL, insert_text_history.size());
  EXPECT_EQ(base::UTF8ToUTF16("test_commit_text"), insert_text_history[0]);

  // Test the input.ime.setComposition API.
  ui::CompositionText composition;
  composition.text = base::UTF8ToUTF16("test_set_composition");
  composition.ime_text_spans.push_back(ui::ImeTextSpan(
      ui::ImeTextSpan::Type::kComposition, 0, composition.text.length(),
      ui::ImeTextSpan::Thickness::kThin, SK_ColorTRANSPARENT));
  composition.selection = gfx::Range(2, 2);
  const std::vector<ui::CompositionText>& composition_history =
      client->composition_history();
  ASSERT_EQ(2UL, composition_history.size());
  EXPECT_EQ(base::UTF8ToUTF16("test_set_composition"),
            composition_history[0].text);
  EXPECT_EQ(base::UTF8ToUTF16(""), composition_history[1].text);

  // Tests input.ime.onBlur API should get event when focusing to another
  // text input client.
  std::unique_ptr<ui::DummyTextInputClient> client2(
      new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
  input_method->SetFocusedTextInputClient(client2.get());
  ASSERT_TRUE(blur_listener.WaitUntilSatisfied()) << message_;

  input_method->DetachTextInputClient(client2.get());
}

// TODO(crbug.com/1004628) Flakes on Windows and Linux
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_SendKeyEventsOnNormalPage DISABLED_SendKeyEventsOnNormalPage
#else
#define MAYBE_SendKeyEventsOnNormalPage SendKeyEventsOnNormalPage
#endif
IN_PROC_BROWSER_TEST_F(InputImeApiTest, MAYBE_SendKeyEventsOnNormalPage) {
  // Navigates to special page that sendKeyEvents API has limition with.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  // Manipulates the focused text input client because the follow cursor
  // window requires the text input focus.
  ui::InputMethod* input_method =
      browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod();
  std::unique_ptr<ui::DummyTextInputClient> client(
      new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
  input_method->SetFocusedTextInputClient(client.get());
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  base::AutoReset<bool> auto_reset_disable_bubble(
      &InputImeActivateFunction::disable_bubble_for_testing_, true);
  SetTrackKeyEvents(input_method, true);

  ASSERT_TRUE(RunExtensionTest("input_ime_nonchromeos")) << message_;

  std::vector<std::unique_ptr<ui::KeyEvent>> key_events;
  key_events.push_back(std::unique_ptr<ui::KeyEvent>(
      new ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE)));
  key_events.push_back(std::unique_ptr<ui::KeyEvent>(
      new ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_A, ui::EF_NONE)));
  key_events.push_back(std::unique_ptr<ui::KeyEvent>(
      new ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_CONTROL_DOWN)));
  key_events.push_back(std::unique_ptr<ui::KeyEvent>(
      new ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_A, ui::EF_CONTROL_DOWN)));
  key_events.push_back(std::unique_ptr<ui::KeyEvent>(
      new ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_TAB, ui::EF_NONE)));

  EXPECT_TRUE(CompareKeyEvents(key_events, input_method));

  input_method->DetachTextInputClient(client.get());
}

// TODO(https://crbug.com/795631): This test is failing on the Linux bot.
#if defined(OS_LINUX)
IN_PROC_BROWSER_TEST_F(InputImeApiTest, DISABLED_SendKeyEventsOnSpecialPage) {
#else
IN_PROC_BROWSER_TEST_F(InputImeApiTest, SendKeyEventsOnSpecialPage) {
#endif
  // Navigates to special page that sendKeyEvents API has limition with.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://flags"));

  ui::InputMethod* input_method =
      browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod();
  std::unique_ptr<ui::DummyTextInputClient> client(
      new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
  input_method->SetFocusedTextInputClient(client.get());
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  base::AutoReset<bool> auto_reset_disable_bubble(
      &InputImeActivateFunction::disable_bubble_for_testing_, true);
  SetTrackKeyEvents(input_method, true);

  ASSERT_TRUE(RunExtensionTest("input_ime_nonchromeos")) << message_;

  std::vector<std::unique_ptr<ui::KeyEvent>> key_events;
  key_events.push_back(std::unique_ptr<ui::KeyEvent>(
      new ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE)));
  key_events.push_back(std::unique_ptr<ui::KeyEvent>(
      new ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_A, ui::EF_NONE)));

  EXPECT_TRUE(CompareKeyEvents(key_events, input_method));
  input_method->DetachTextInputClient(client.get());
}

}  // namespace extensions
