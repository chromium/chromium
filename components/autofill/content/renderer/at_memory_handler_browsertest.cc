// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/at_memory_handler.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace autofill {

namespace {

using ::blink::WebFormControlElement;
using ::blink::WebString;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Ne;
using ::testing::Values;
using ::testing::WithParamInterface;

class AtMemoryHandlerTest : public test::AutofillRendererTest {
 public:
  AtMemoryHandlerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kAutofillKeydownEditableElement,
                              features::kAutofillAtMemoryDoubleCtrl,
                              features::kAutofillAtMemoryTriggerShortcut,
                              features::kAutofillAtMemoryTriggerString,
                              features::kAutofillAtMemory},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    test::AutofillRendererTest::SetUp();
    SetTrigger(u"@@");
    run_loop_.emplace();
    ON_CALL(autofill_driver(), AskForValuesToFill)
        .WillByDefault([this](const FormData& form, FieldRendererId field_id,
                              const gfx::Rect& caret_bounds,
                              AutofillSuggestionTriggerSource trigger_source,
                              const std::optional<PasswordSuggestionRequest>&
                                  password_request) {
          if (IsAtMemoryTriggerSource(trigger_source)) {
            ApplyFieldActionAsync(field_id, fill_value_to_respond_,
                                  action_persistence_to_respond_);
          }
        });
  }

  // Calls ApplyFieldAction() asynchronously.
  // To be called in response to AskForValuesToFill().
  void ApplyFieldActionAsync(
      FieldRendererId field_id,
      std::u16string value = u"result",
      mojom::ActionPersistence persistence = mojom::ActionPersistence::kFill) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&AutofillAgent::ApplyFieldAction,
                       test_api(autofill_agent()).GetWeakPtr(),
                       mojom::FieldActionType::kReplaceSelectionForAtMemory,
                       persistence, field_id, std::move(value))
            .Then(run_loop_->QuitClosure()));
  }

  void WaitForApplyFieldAction() {
    run_loop_->Run();
    run_loop_.emplace();
  }

  void set_fill_value_to_respond(std::u16string value) {
    fill_value_to_respond_ = std::move(value);
  }

  void set_action_persistence_to_respond(mojom::ActionPersistence persistence) {
    action_persistence_to_respond_ = persistence;
  }

  void SetTrigger(std::u16string trigger_string) {
    blink::RendererPreferences prefs =
        GetMainRenderFrame()->GetWebView()->GetRendererPreferences();
    prefs.autofill_trigger_string = std::move(trigger_string);
    prefs.autofill_shortcut_key_code = ui::VKEY_UNKNOWN;
    prefs.autofill_shortcut_modifiers = ui::EF_NONE;
    GetMainRenderFrame()->GetWebView()->SetRendererPreferences(prefs);
  }

  void SetTrigger(ui::KeyboardCode key_code, int modifiers) {
    blink::RendererPreferences prefs =
        GetMainRenderFrame()->GetWebView()->GetRendererPreferences();
    prefs.autofill_trigger_string = u"";
    prefs.autofill_shortcut_key_code = key_code;
    prefs.autofill_shortcut_modifiers = modifiers;
    GetMainRenderFrame()->GetWebView()->SetRendererPreferences(prefs);
  }

  // Simulates the user typing slow enough to let AutofillAgent trigger
  // AskForValuesToFill() after each character.
  //
  // For faster typing, AutofillAgent's event throttling may swallow
  // AskForValuesToFill().
  void SimulateSlowTyping(std::string_view text) {
    for (char c : text) {
      SimulateUserTypingAsciiCharacter(c, /*flush_message_loop=*/true);
      task_environment_.FastForwardBy(base::Milliseconds(100));
    }
  }

  enum class CtrlKey { kLeft, kRight };

  // On Mac, we treat the Command (aka Windows aka Meta aka Super) key as the
  // Ctrl key.
  void SendCtrlKeyDown(CtrlKey key = CtrlKey::kLeft,
                       bool is_auto_repeat = false) {
    int modifiers = [] {
      if constexpr (BUILDFLAG(IS_MAC)) {
        return blink::WebInputEvent::kMetaKey;
      } else {
        return blink::WebInputEvent::kControlKey;
      }
    }();
    if (is_auto_repeat) {
      modifiers |= blink::WebInputEvent::kIsAutoRepeat;
    }

    blink::WebKeyboardEvent event(blink::WebInputEvent::Type::kRawKeyDown,
                                  modifiers, base::TimeTicks::Now());

    event.windows_key_code = [](CtrlKey key) {
      if constexpr (BUILDFLAG(IS_MAC)) {
        switch (key) {
          case CtrlKey::kLeft:
            return ui::VKEY_COMMAND;
          case CtrlKey::kRight:
            return ui::VKEY_RIGHT_COMMAND;
        }
      } else {
        // The WebKeyboardEvent::windows_key_code does not distinguish between
        // left and right Ctrl buttons.
        return ui::VKEY_CONTROL;
      }
    }(key);

    event.dom_code = static_cast<int>([&key] {
      if constexpr (BUILDFLAG(IS_MAC)) {
        switch (key) {
          case CtrlKey::kLeft:
            return ui::DomCode::META_LEFT;
          case CtrlKey::kRight:
            return ui::DomCode::META_RIGHT;
        }
      } else {
        switch (key) {
          case CtrlKey::kLeft:
            return ui::DomCode::CONTROL_LEFT;
          case CtrlKey::kRight:
            return ui::DomCode::CONTROL_RIGHT;
        }
      }
      NOTREACHED();
    }());

    SendWebKeyboardEvent(event);
  }

 private:
  std::optional<base::RunLoop> run_loop_;
  std::u16string fill_value_to_respond_ = u"result";
  mojom::ActionPersistence action_persistence_to_respond_ =
      mojom::ActionPersistence::kFill;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/550313683): Parametrize tests from
// `AtMemoryHandlerTest` and `AtMemoryHandlerContentEditableTest`.
class AtMemoryHandlerTest_SingleField
    : public AtMemoryHandlerTest,
      public WithParamInterface<FormControlType> {
 public:
  void SetUp() override {
    AtMemoryHandlerTest::SetUp();
    switch (form_control_type()) {
      case FormControlType::kContentEditable:
        LoadHTML(R"(<div id="f" contenteditable="true"
                     style="width:100px; height:100px;"></div>)");
        break;
      case FormControlType::kInputText:
        LoadHTML(R"(<input id="f">)");
        break;
      case FormControlType::kTextArea:
        LoadHTML(R"(<textarea id="f"></textarea>)");
        break;
      case FormControlType::kInputDate:
      case FormControlType::kInputEmail:
      case FormControlType::kInputMonth:
      case FormControlType::kInputNumber:
      case FormControlType::kInputPassword:
      case FormControlType::kInputSearch:
      case FormControlType::kInputTelephone:
      case FormControlType::kInputUrl:
      case FormControlType::kSelectOne:
        NOTREACHED();
    }
    WaitForFormsSeen();
    Focus("f");
  }

  FormControlType form_control_type() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(AtMemoryHandlerTest,
                         AtMemoryHandlerTest_SingleField,
                         Values(FormControlType::kContentEditable,
                                FormControlType::kInputText,
                                FormControlType::kTextArea));

TEST_P(AtMemoryHandlerTest_SingleField, AtMemorySearchTrigger) {
  testing::MockFunction<void(int)> check_point;
  {
    testing::InSequence s;
    // 1. "a" -> No AtMemory trigger.
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, _, _,
            Eq(AutofillSuggestionTriggerSource::kAtMemoryTriggerString), _))
        .Times(0);
    EXPECT_CALL(check_point, Call(1));

    // 2. "a@" -> No AtMemory trigger.
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, _, _,
            Eq(AutofillSuggestionTriggerSource::kAtMemoryTriggerString), _))
        .Times(0);
    EXPECT_CALL(check_point, Call(2));

    // 3. "a@@" -> AtMemory has triggered.
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, _, _,
            Eq(AutofillSuggestionTriggerSource::kAtMemoryTriggerString), _))
        .Times(1);
    EXPECT_CALL(check_point, Call(3));

    // 4. "a@@b" -> No AtMemory trigger.
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, _, _,
            Eq(AutofillSuggestionTriggerSource::kAtMemoryTriggerString), _))
        .Times(0);
    EXPECT_CALL(check_point, Call(4));
  }

  // Ignore standard Autofill calls for this test.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(AnyNumber());

  // Typing sequence: "a", "a@", "a@@", "a@@b"
  SimulateSlowTyping("a");
  check_point.Call(1);
  SimulateSlowTyping("@");
  check_point.Call(2);
  SimulateSlowTyping("@");
  check_point.Call(3);
  SimulateSlowTyping("b");
  check_point.Call(4);
}

// Tests that the keyboard shortcut triggers AtMemory.
TEST_P(AtMemoryHandlerTest_SingleField, AtMemoryShortcutTrigger) {
  SetTrigger(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _,
          Eq(AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut), _));

  blink::WebKeyboardEvent event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kControlKey | blink::WebInputEvent::kShiftKey,
      base::TimeTicks::Now());
  event.windows_key_code = ui::VKEY_Y;
  SendWebKeyboardEvent(event);

  task_environment_.RunUntilIdle();
}

// Tests that the keyboard shortcut triggers AtMemory even with CapsLock and
// NumLock.
TEST_P(AtMemoryHandlerTest_SingleField,
       AtMemoryShortcutTriggerWithCapsLockAndNumLock) {
  SetTrigger(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _,
          Eq(AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut), _));

  blink::WebKeyboardEvent event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kControlKey | blink::WebInputEvent::kShiftKey |
          blink::WebInputEvent::kCapsLockOn | blink::WebInputEvent::kNumLockOn,
      base::TimeTicks::Now());
  event.windows_key_code = ui::VKEY_Y;
  SendWebKeyboardEvent(event);

  task_environment_.RunUntilIdle();
}

// Tests that the keyboard shortcut does not trigger AtMemory if it's an
// auto-repeat event.
TEST_P(AtMemoryHandlerTest_SingleField, AtMemoryShortcutTriggerRepeatBlocked) {
  SetTrigger(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _,
          Eq(AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut), _))
      .Times(0);

  blink::WebKeyboardEvent event(blink::WebInputEvent::Type::kRawKeyDown,
                                blink::WebInputEvent::kControlKey |
                                    blink::WebInputEvent::kShiftKey |
                                    blink::WebInputEvent::kIsAutoRepeat,
                                base::TimeTicks::Now());
  event.windows_key_code = ui::VKEY_Y;
  SendWebKeyboardEvent(event);

  task_environment_.RunUntilIdle();
}

// Tests that setting a keyboard shortcut disables other triggers (trigger
// string and double Ctrl).
TEST_P(AtMemoryHandlerTest_SingleField, ShortcutDisablesOtherTriggers) {
  SetTrigger(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(0);
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(0);
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _,
          AllOf(Ne(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
                Ne(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl)),
          _))
      .Times(AnyNumber());

  SimulateSlowTyping("@@");
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  task_environment_.RunUntilIdle();
}

TEST_F(AtMemoryHandlerTest, AtMemorySearchTrigger_NumberInput) {
  LoadHTML(R"(<input type="number" id="f">)");
  WaitForFormsSeen();
  Focus("f");

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, AutofillSuggestionTriggerSource::kAtMemoryTriggerString, _));
  // No other AskForValuesToFill() events are expected. In particular,
  // TextFieldValueChanged() doesn't fire any because of throttling.

  SimulateSlowTyping("@@");
}

TEST_F(AtMemoryHandlerTest, AtMemorySearchTrigger_NoTriggerOnBackspace) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(0);
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(AnyNumber());

  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  Focus("f");

  SimulateUserTypingAsciiCharacter('@', true);
  SimulateUserTypingKeyCode(ui::VKEY_BACK, true);
  SimulateUserTypingAsciiCharacter('@', true);
  task_environment_.RunUntilIdle();
}

TEST_F(AtMemoryHandlerTest, AtMemorySearchTrigger_NoTriggerOnAutoRepeat) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(0);
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(AnyNumber());

  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  Focus("f");

  SimulateUserTypingAsciiCharacter('@', true);

  blink::WebKeyboardEvent repeat_event(blink::WebInputEvent::Type::kRawKeyDown,
                                       blink::WebInputEvent::kIsAutoRepeat,
                                       base::TimeTicks::Now());
  repeat_event.windows_key_code = ui::VKEY_2;
  repeat_event.text[0] = '@';
  SendWebKeyboardEvent(repeat_event);
  task_environment_.RunUntilIdle();
}

TEST_F(AtMemoryHandlerTest, AtMemorySearchTrigger_Constraints) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  Focus("f");

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(0);
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(AnyNumber());

  // Scenario 1: Timeout constraint.
  // Type "@", wait 600ms, type "@".
  SimulateUserTypingAsciiCharacter('@', true);
  task_environment_.FastForwardBy(base::Milliseconds(600));
  SimulateUserTypingAsciiCharacter('@', true);
  task_environment_.RunUntilIdle();

  // Scenario 2: Navigation constraint (arrow key).
  ExecuteJavaScriptForTests("document.getElementById('f').value = '';");
  SimulateUserTypingAsciiCharacter('@', true);
  SimulateUserTypingKeyCode(ui::VKEY_LEFT, true);
  SimulateUserTypingAsciiCharacter('@', true);
  task_environment_.RunUntilIdle();
}

// Tests that typing "@@" into an empty field triggers the AtMemory search
// popup.
TEST_F(AtMemoryHandlerTest, MemorySearchTriggerTypedIntoEmptyField) {
  // 1. Setup Expectations:
  // Ignore standard Autofill noise during setup.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(AnyNumber());
  // Expect the specific AtMemory trigger.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, AutofillSuggestionTriggerSource::kAtMemoryTriggerString, _));

  // 2. Act:
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  Focus("f");
  SimulateSlowTyping("@@");
}

// Tests that typing "@@" in the middle of a string also triggers AtMemory.
TEST_F(AtMemoryHandlerTest, MemorySearchTriggerInMiddle) {
  // 1. Setup Expectations:
  // Ignore standard Autofill noise during setup.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(AnyNumber());
  // Expect the specific AtMemory trigger.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, AutofillSuggestionTriggerSource::kAtMemoryTriggerString, _));

  // 2. Act:
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  Focus("f");
  SimulateSlowTyping("a@@");
}

// Tests that prefix matching is not too greedy: even though the user input
// "aaaa" is not a prefix of the trigger string "aaab", AtMemoryHandler detects
// typing one more "b" completes the trigger.
TEST_F(AtMemoryHandlerTest, MemorySearchTriggerOverlappingPrefix) {
  SetTrigger(u"aaab");

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(AnyNumber());
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, AutofillSuggestionTriggerSource::kAtMemoryTriggerString, _));

  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  Focus("f");
  SimulateSlowTyping("aaaab");
}

// Tests that typing "@@" in the password field doesn't trigger AtMemory.
TEST_F(AtMemoryHandlerTest, MemorySearchNotTriggeredOnPasswordField) {
  // 1. Setup Expectations:
  // Ignore standard Autofill noise during setup.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(AnyNumber());
  // Expect no AtMemory trigger.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, AutofillSuggestionTriggerSource::kAtMemoryTriggerString, _))
      .Times(0);

  // 2. Act:
  LoadHTML(R"(<input id="f" type="password">)");
  WaitForFormsSeen();
  Focus("f");
  SimulateSlowTyping("a@@");
}

// Tests that typing "@@" in a disabled field doesn't trigger AtMemory.
TEST_F(AtMemoryHandlerTest, MemorySearchNotTriggeredOnDisabledField) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(AnyNumber());
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, AutofillSuggestionTriggerSource::kAtMemoryTriggerString, _))
      .Times(0);

  LoadHTML(R"(<input id="f" disabled>)");
  WaitForFormsSeen();
  Focus("f");
  SimulateSlowTyping("a@@");
}

// Tests that typing "@@" in a read-only field doesn't trigger AtMemory.
TEST_F(AtMemoryHandlerTest, MemorySearchNotTriggeredOnReadOnlyField) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(AnyNumber());
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, AutofillSuggestionTriggerSource::kAtMemoryTriggerString, _))
      .Times(0);

  LoadHTML(R"(<input id="f" readonly>)");
  WaitForFormsSeen();
  Focus("f");
  SimulateSlowTyping("a@@");
}

// Tests that ApplyFieldAction correctly handles targeted replacement of "@@"
// in standard text inputs during the filling phase.
TEST_F(AtMemoryHandlerTest,
       AtMemorySearchResult_ApplyFieldAction_StandardInput_Fill) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  blink::WebInputElement input = GetInputElementById("f");
  FieldRendererId field_id = form_util::GetFieldRendererId(input);
  Focus("f");

  // 1. Targeted replacement of the "@@" trigger: "hello @@" -> "hello result"
  SimulateSlowTyping("hello @@");
  WaitForApplyFieldAction();
  EXPECT_EQ(input.Value().Utf16(), u"hello result");
  EXPECT_EQ(input.SelectionStart(), 12u);

  // 2. Replacement of a non-empty selection: "hello [selection] world"
  task_environment_.FastForwardBy(base::Milliseconds(100));
  input.SetValue(blink::WebString::FromUtf16(u"hello selection world"));
  input.SetSelectionRange(6, 15);
  autofill_agent().TriggerSuggestions(
      field_id, AutofillSuggestionTriggerSource::kAtMemoryContextMenu);
  WaitForApplyFieldAction();
  EXPECT_EQ(input.Value().Utf16(), u"hello result world");
  EXPECT_EQ(input.SelectionStart(), 12u);

  // 3. Fallback insertion (no @@, no selection): "hello result" -> "hello
  // result extra"
  task_environment_.FastForwardBy(base::Milliseconds(100));
  input.SetValue(blink::WebString::FromUtf16(u"hello result"));
  input.SetSelectionRange(12, 12);
  set_fill_value_to_respond(u"extra");
  autofill_agent().TriggerSuggestions(
      field_id, AutofillSuggestionTriggerSource::kAtMemoryContextMenu);
  WaitForApplyFieldAction();
  EXPECT_EQ(input.Value().Utf16(), u"hello resultextra");
  EXPECT_EQ(input.SelectionStart(), 17u);
}

// Tests that trigger string removal does NOT happen when triggered by keyboard
// shortcut.
TEST_F(AtMemoryHandlerTest,
       AtMemoryTriggerSource_KeyboardShortcut_PreservesTriggerString) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  blink::WebInputElement input = GetInputElementById("f");
  FieldRendererId field_id = form_util::GetFieldRendererId(input);
  Focus("f");

  input.SetValue(blink::WebString::FromUtf16(u"hello @@"));
  input.SetSelectionRange(8, 8);
  autofill_agent().TriggerSuggestions(
      field_id, AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut);
  WaitForApplyFieldAction();
  EXPECT_EQ(input.Value().Utf16(), u"hello @@result");
}

// Tests that trigger string removal DOES happen when triggered by trigger
// string.
TEST_F(AtMemoryHandlerTest,
       AtMemoryTriggerSource_TriggerString_ReplacesTriggerString) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  blink::WebInputElement input = GetInputElementById("f");
  Focus("f");

  SimulateSlowTyping("hello @@");
  WaitForApplyFieldAction();
  EXPECT_EQ(input.Value().Utf16(), u"hello result");
}

// Tests that ApplyFieldAction() with kReplaceSelectionForAtMemory aborts if no
// matching entry is found in last_at_memory_ask_for_values_to_fills_.
TEST_F(AtMemoryHandlerTest, AtMemoryReplaceTriggerAbortsIfNoHistoryEntryFound) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  blink::WebInputElement input = GetInputElementById("f");
  FieldRendererId field_id = form_util::GetFieldRendererId(input);
  Focus("f");

  input.SetValue(blink::WebString::FromUtf16(u"hello"));
  input.SetSelectionRange(5, 5);
  autofill_agent().ApplyFieldAction(
      mojom::FieldActionType::kReplaceSelectionForAtMemory,
      mojom::ActionPersistence::kFill, field_id, u"result");
  // Filling should be aborted; value remains unchanged.
  EXPECT_EQ(input.Value().Utf16(), u"hello");
}

// Tests that ApplyFieldAction() with kReplaceSelectionForAtMemory aborts if the
// value changed after AskForValuesToFill().
TEST_F(AtMemoryHandlerTest, AtMemoryReplaceTriggerAbortsIfValueChanged) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  blink::WebInputElement input = GetInputElementById("f");
  Focus("f");

  SimulateSlowTyping("hello ");
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  input.SetValue(blink::WebString::FromUtf16(u"hello changed"));
  WaitForApplyFieldAction();
  // Filling should be aborted; value remains unchanged.
  EXPECT_EQ(input.Value().Utf16(), u"hello changed");
}

// Tests that ApplyFieldAction() with kReplaceSelectionForAtMemory refocuses the
// element and restores the caret if the element lost focus.
TEST_F(AtMemoryHandlerTest, RefocusesAndRestoresCaretIfUnfocused) {
  LoadHTML(R"(<input id="f"><input id="g">)");
  WaitForFormsSeen();
  blink::WebInputElement input = GetInputElementById("f");
  blink::WebInputElement other = GetInputElementById("g");
  Focus("f");

  // Ignore standard Autofill noise during setup.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(AnyNumber());
  // Expect the specific AtMemory trigger.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .WillOnce([this, &other](const FormData& form, FieldRendererId field_id,
                               const gfx::Rect& caret_bounds,
                               AutofillSuggestionTriggerSource trigger_source,
                               const std::optional<PasswordSuggestionRequest>&
                                   password_request) {
        other.Focus();
        EXPECT_EQ(other.GetDocument().FocusedElement(), other);
        ApplyFieldActionAsync(field_id);
      });

  SimulateSlowTyping("hello ");
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  WaitForApplyFieldAction();
  EXPECT_EQ(input.Value().Utf16(), u"hello result");
  EXPECT_EQ(input.GetDocument().FocusedElement(), input);
}

// Tests that ApplyFieldAction() with kReplaceSelectionForAtMemory waits for
// window-level focus to return before filling if the window lost focus to a
// popup.
TEST_F(AtMemoryHandlerTest, WaitsForWindowFocusBeforeFilling) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  GetWebFrameWidget()->SetFocus(true);
  blink::WebInputElement input = GetInputElementById("f");
  Focus("f");

  // Ignore standard Autofill noise during setup.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(AnyNumber());
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .WillOnce([this](const FormData& form, FieldRendererId field_id,
                       const gfx::Rect& caret_bounds,
                       AutofillSuggestionTriggerSource trigger_source,
                       const std::optional<PasswordSuggestionRequest>&
                           password_request) {
        // Simulate the window losing focus to the popup.
        GetWebFrameWidget()->SetFocus(false);
        ApplyFieldActionAsync(field_id, u"result");
      });

  SimulateSlowTyping("hello ");
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  // The first attempt (num_try = 0) fails because the window lacks focus and
  // schedules a retry in 20 ms.
  WaitForApplyFieldAction();
  EXPECT_EQ(input.Value().Utf16(), u"hello ");

  // After 20 ms, the first retry runs and finds the window is still unfocused.
  task_environment_.FastForwardBy(base::Milliseconds(20));
  EXPECT_EQ(input.Value().Utf16(), u"hello ");

  // Restore window focus. The next retry in 20 ms will see the focused state.
  GetWebFrameWidget()->SetFocus(true);
  task_environment_.FastForwardBy(base::Milliseconds(20));

  EXPECT_EQ(input.Value().Utf16(), u"hello result");
  EXPECT_EQ(input.GetDocument().FocusedElement(), input);
}

// Tests that ApplyFieldAction() with kReplaceSelectionForAtMemory falls back to
// filling the field without window focus once the retry limit is exceeded.
TEST_F(AtMemoryHandlerTest, FillsAfterMaxRetriesIfWindowNeverGainsFocus) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  GetWebFrameWidget()->SetFocus(true);
  blink::WebInputElement input = GetInputElementById("f");
  Focus("f");

  // Ignore standard Autofill noise during setup.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(AnyNumber());
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .WillOnce([this](const FormData& form, FieldRendererId field_id,
                       const gfx::Rect& caret_bounds,
                       AutofillSuggestionTriggerSource trigger_source,
                       const std::optional<PasswordSuggestionRequest>&
                           password_request) {
        // Simulate the window losing focus and never regaining it.
        GetWebFrameWidget()->SetFocus(false);
        ApplyFieldActionAsync(field_id, u"result");
      });

  SimulateSlowTyping("hello ");
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  WaitForApplyFieldAction();

  // Fast forward through 4 retries (4 * 20 ms = 80 ms). The field is not filled
  // yet.
  task_environment_.FastForwardBy(base::Milliseconds(80));
  EXPECT_EQ(input.Value().Utf16(), u"hello ");

  // The 5th retry (at 100 ms) hits kMaxRetries and fills as a fallback.
  task_environment_.FastForwardBy(base::Milliseconds(20));
  EXPECT_EQ(input.Value().Utf16(), u"hello result");
}

// Tests that a non-standard trigger string works in <input> fields.
TEST_F(AtMemoryHandlerTest, NonStandardTriggerString) {
  // Ignore standard Autofill noise during setup.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(AnyNumber());
  // Expect the specific AtMemory trigger.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, AutofillSuggestionTriggerSource::kAtMemoryTriggerString, _));

  SetTrigger(u"Foo");
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  Focus("f");
  SimulateSlowTyping("Foobar");
}

// TODO(crbug.com/550313683): Make a parameterized test with a parameter to test
// an <input>, <textarea>, or contenteditable.
class AtMemoryHandlerContentEditableTest : public AtMemoryHandlerTest {
 public:
  void SetUp() override {
    AtMemoryHandlerTest::SetUp();
    LoadHTML(R"(<div id="ce" contenteditable="true"
                     style="width:100px; height:100px;"></div>)");
    WaitForFormsSeen();
    Focus("ce");
  }
};

// Tests that AtMemory popup is triggered if we type just the "@@".
TEST_F(AtMemoryHandlerContentEditableTest, TriggerViaTyping) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, AutofillSuggestionTriggerSource::kAtMemoryTriggerString, _))
      .Times(1);

  SimulateSlowTyping("@@");
}

// Tests that AtMemory popup triggers in the presence of non-trivial symbols.
TEST_F(AtMemoryHandlerContentEditableTest, TriggerWithComplexPrecedingText) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(1);
  SimulateSlowTyping("Memory log #123 (Feb 2026): @@");
}

// Tests that AtMemory popup doesn't trigger on a single "@".
TEST_F(AtMemoryHandlerContentEditableTest, NoTriggerOnSingleAt) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(0);
  SimulateSlowTyping("@");
}

// Tests that AtMemory popup doesn't trigger on selection.
TEST_F(AtMemoryHandlerContentEditableTest, NoTriggerOnSelection) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(0);

  // Manually set text and select it all.
  ExecuteJavaScriptForTests(R"(
    const el = document.getElementById('ce');
    el.innerText = '@@';
    const range = document.createRange();
    range.selectNodeContents(el);
    const sel = window.getSelection();
    sel.removeAllRanges();
    sel.addRange(range);
  )");
  test_api(autofill_agent()).ContentEditableDidChange(GetWebElementById("ce"));
}

// Tests that AtMemory popup triggers each time the new trigger is typed.
TEST_F(AtMemoryHandlerContentEditableTest, MultipleTriggers) {
  // Verify that it triggers every time @@ is completed.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryTriggerString),
          _))
      .Times(2);

  SimulateSlowTyping("@@");
  SimulateSlowTyping("abc@@");
}

// Tests that kReplaceSelectionForAtMemory correctly replaces the "@@" trigger
// in a contenteditable element and places the cursor after the filled value.
TEST_F(AtMemoryHandlerContentEditableTest,
       ReplaceAtMemoryTriggerInContentEditable) {
  blink::WebElement ce = GetWebElementById("ce");

  // 1. Set response value to "Suffix" and simulate typing the trigger.
  set_fill_value_to_respond(u"Suffix");
  SimulateSlowTyping("Prefix @@");
  WaitForApplyFieldAction();

  // 2. Verify the trigger was replaced.
  EXPECT_EQ(ce.TextContent().Utf16(), u"Prefix Suffix");

  // 3. Verify the cursor position (at the end of "Prefix Suffix").
  blink::WebRange selection =
      GetMainFrame()->GetInputMethodController()->GetSelectionOffsets();
  EXPECT_EQ(selection.StartOffset(), 13);
  EXPECT_EQ(selection.EndOffset(), 13);
}

// Tests that kReplaceSelectionForAtMemory inserts a value at the current cursor
// position if the trigger string ("@@") is not found immediately before the
// cursor (for example, during context menu invocation).
TEST_F(AtMemoryHandlerContentEditableTest,
       ReplaceAtMemoryTriggerForContextMenu) {
  blink::WebElement ce = GetWebElementById("ce");

  // 1. Set initial text without the trigger and position cursor at the end.
  SimulateSlowTyping("PrefixSuffix");

  // 2. Put cursor position between "Prefix" and "Suffix".
  GetMainFrame()->SetEditableSelectionOffsets(6, 6);
  test_api(autofill_agent()).ContentEditableDidChange(ce);

  // Verify the cursor position before triggering the fill action.
  EXPECT_EQ(GetMainFrame()
                ->GetInputMethodController()
                ->GetSelectionOffsets()
                .StartOffset(),
            6);

  // 3. Trigger suggestions via context menu and wait for fill action.
  autofill_agent().TriggerSuggestions(
      form_util::GetFieldRendererId(ce),
      AutofillSuggestionTriggerSource::kAtMemoryContextMenu);
  WaitForApplyFieldAction();

  // 4. Verify the text was inserted.
  EXPECT_EQ(ce.TextContent().Utf16(), u"PrefixresultSuffix");

  // 5. Verify the cursor position (at the end of "result").
  // "Prefix" (6) + "result " (6) = 12.
  blink::WebRange selection =
      GetMainFrame()->GetInputMethodController()->GetSelectionOffsets();
  EXPECT_EQ(selection.StartOffset(), 12);
}

// Tests that kReplaceSelectionForAtMemory replaces a pre-existing selection.
TEST_F(AtMemoryHandlerContentEditableTest,
       ReplaceAtMemoryTriggerWithSelection) {
  blink::WebElement ce = GetWebElementById("ce");

  // 1. Set initial text and select a middle portion.
  ExecuteJavaScriptForTests(R"(
    const el = document.getElementById('ce');
    el.focus();
    el.innerText = 'PrefixSelectedSuffix';
    const range = document.createRange();
    // Select "Selected" (offsets 6 to 14).
    range.setStart(el.childNodes[0], 6);
    range.setEnd(el.childNodes[0], 14);
    const sel = window.getSelection();
    sel.removeAllRanges();
    sel.addRange(range);
  )");
  test_api(autofill_agent()).ContentEditableDidChange(ce);

  // 2. Trigger suggestions via context menu and wait for fill action.
  autofill_agent().TriggerSuggestions(
      form_util::GetFieldRendererId(ce),
      AutofillSuggestionTriggerSource::kAtMemoryContextMenu);
  WaitForApplyFieldAction();

  // 3. Verify "Selected" was replaced by "result".
  EXPECT_EQ(ce.TextContent().Utf16(), u"PrefixresultSuffix");

  // 4. Verify the cursor position (at the end of "Result").
  // "Prefix " (6) + "result" (6) = 12.
  blink::WebRange selection =
      GetMainFrame()->GetInputMethodController()->GetSelectionOffsets();
  EXPECT_EQ(selection.StartOffset(), 12);
}

// Tests that ApplyFieldAction() with kReplaceSelectionForAtMemory aborts if the
// value changed after AskForValuesToFill().
TEST_F(AtMemoryHandlerContentEditableTest,
       AtMemoryReplaceTriggerAbortsIfValueChanged) {
  blink::WebElement ce = GetWebElementById("ce");

  SimulateSlowTyping("hello ");
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  ExecuteJavaScriptForTests(R"(
    document.getElementById('ce').innerText = 'hello changed';
  )");
  test_api(autofill_agent()).ContentEditableDidChange(ce);
  WaitForApplyFieldAction();
  // Filling should be aborted; value remains unchanged.
  EXPECT_EQ(ce.TextContent().Utf16(), u"hello changed");
}

// Tests that ApplyFieldAction() with kReplaceSelectionForAtMemory refocuses the
// element and restores the caret if the element lost focus.
TEST_F(AtMemoryHandlerContentEditableTest,
       RefocusesAndRestoresCaretIfUnfocused) {
  blink::WebElement ce = GetWebElementById("ce");
  Focus("ce");

  ExecuteJavaScriptForTests(R"(
    const input = document.createElement('input');
    input.id = 'other';
    document.body.appendChild(input);
  )");
  blink::WebElement other = GetWebElementById("other");

  // Ignore standard Autofill noise during setup.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(AnyNumber());
  // Expect the specific AtMemory trigger.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .WillOnce([this, &other](const FormData& form, FieldRendererId field_id,
                               const gfx::Rect& caret_bounds,
                               AutofillSuggestionTriggerSource trigger_source,
                               const std::optional<PasswordSuggestionRequest>&
                                   password_request) {
        other.Focus();
        ApplyFieldActionAsync(field_id);
        EXPECT_EQ(other.GetDocument().FocusedElement(), other);
      });

  SimulateSlowTyping("hello ");
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  WaitForApplyFieldAction();
  EXPECT_EQ(ce.TextContent().Utf16(), u"hello result");
  EXPECT_EQ(ce.GetDocument().FocusedElement(), ce);
}

// Tests that a non-standard trigger string works in <div contenteditable>
// fields.
TEST_F(AtMemoryHandlerContentEditableTest, NonStandardTriggerString) {
  // Expect the specific AtMemory trigger.
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, AutofillSuggestionTriggerSource::kAtMemoryTriggerString, _));
  // No other AskForValuesToFill() events are expected. In particular,
  // TextFieldValueChanged() doesn't fire any because of throttling.

  SetTrigger(u"Foo");
  LoadHTML(R"(<div contenteditable id="f">)");
  WaitForFormsSeen();
  Focus("f");
  SimulateSlowTyping("Foobar");
}

// Tests that pressing Ctrl twice triggers AtMemory in an <input>.
TEST_F(AtMemoryHandlerTest, DoubleCtrlTriggersAtMemoryInInput) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  Focus("f");

  EXPECT_CALL(autofill_driver(),
              AskForValuesToFill(
                  _, _, _,
                  Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _));

  SendCtrlKeyDown();
  SendCtrlKeyDown();
  WaitForApplyFieldAction();
  blink::WebInputElement input = GetInputElementById("f");
  EXPECT_EQ(input.Value().Utf16(), u"result");
}

// Tests that pressing Ctrl twice triggers AtMemory in a <textarea>.
TEST_F(AtMemoryHandlerTest, DoubleCtrlTriggersAtMemoryInTextArea) {
  LoadHTML(R"(<textarea id="f"></textarea>)");
  WaitForFormsSeen();
  Focus("f");

  EXPECT_CALL(autofill_driver(),
              AskForValuesToFill(
                  _, _, _,
                  Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _));

  SendCtrlKeyDown();
  SendCtrlKeyDown();
  WaitForApplyFieldAction();
  blink::WebFormControlElement textarea = GetFormControlElementById("f");
  EXPECT_EQ(textarea.Value().Utf16(), u"result");
}

// Tests that pressing Ctrl twice triggers AtMemory in a contenteditable.
TEST_F(AtMemoryHandlerTest, DoubleCtrlTriggersAtMemoryInContentEditable) {
  LoadHTML(R"(<div contenteditable id="f"></div>)");
  WaitForFormsSeen();
  Focus("f");

  EXPECT_CALL(autofill_driver(),
              AskForValuesToFill(
                  _, _, _,
                  Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _));

  SendCtrlKeyDown();
  SendCtrlKeyDown();
  WaitForApplyFieldAction();
  blink::WebElement f = GetWebElementById("f");
  EXPECT_EQ(f.TextContent().Utf16(), u"result");
}

// Tests that pressing Ctrl twice triggers AtMemory even when a non-empty
// selection has been made, and replaces the selection with the filled value.
TEST_F(AtMemoryHandlerTest, DoubleCtrlTriggersAtMemoryWithSelection) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  blink::WebInputElement input = GetInputElementById("f");
  Focus("f");

  input.SetValue(blink::WebString::FromUtf16(u"hello selection world"));
  input.SetSelectionRange(6, 15);

  EXPECT_CALL(autofill_driver(),
              AskForValuesToFill(
                  _, _, _,
                  Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _));

  SendCtrlKeyDown();
  SendCtrlKeyDown();
  WaitForApplyFieldAction();

  EXPECT_EQ(input.Value().Utf16(), u"hello result world");
  EXPECT_EQ(input.SelectionStart(), 12u);
}

// Tests that typing an intervening character cancels the double Ctrl sequence.
TEST_F(AtMemoryHandlerTest, InterveningKeyCancelsDoubleCtrl) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  Focus("f");

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(0);
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(AnyNumber());

  SendCtrlKeyDown();
  SimulateUserTypingAsciiCharacter('a', /*flush_message_loop=*/true);
  SendCtrlKeyDown();
  task_environment_.RunUntilIdle();
}

// Tests that exceeding the timeout cancels the double Ctrl sequence.
TEST_F(AtMemoryHandlerTest, TimeoutCancelsDoubleCtrl) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  Focus("f");

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(0);
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(AnyNumber());

  SendCtrlKeyDown();
  task_environment_.FastForwardBy(base::Milliseconds(600));
  SendCtrlKeyDown();
  task_environment_.RunUntilIdle();
}

// Tests that an auto-repeat Ctrl keydown event does not trigger AtMemory.
TEST_F(AtMemoryHandlerTest, AutoRepeatDoesNotTrigger) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  Focus("f");

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(0);
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(AnyNumber());

  SendCtrlKeyDown();
  SendCtrlKeyDown(CtrlKey::kLeft, /*is_auto_repeat=*/true);
  task_environment_.RunUntilIdle();
}

// Tests that changing focus cancels the double Ctrl sequence.
TEST_F(AtMemoryHandlerTest, FocusChangeCancelsDoubleCtrl) {
  LoadHTML(R"(<input id="f1"><input id="f2">)");
  WaitForFormsSeen();
  Focus("f1");

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(0);
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(AnyNumber());

  SendCtrlKeyDown();
  Focus("f2");
  SendCtrlKeyDown();
  task_environment_.RunUntilIdle();
}

// Tests that Left Ctrl followed by Right Ctrl does not trigger AtMemory, but
// Left Ctrl followed by two Right Ctrls does.
TEST_F(AtMemoryHandlerTest, LeftCtrlFollowedByRightCtrl) {
  LoadHTML(R"(<input id="f">)");
  WaitForFormsSeen();
  Focus("f");

  testing::MockFunction<void(int)> check_point;
  {
    testing::InSequence s;
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl),
            _))
        .Times(0);
    EXPECT_CALL(check_point, Call(1));
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl),
            _))
        .Times(1);
    EXPECT_CALL(check_point, Call(2));
  }

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Ne(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(AnyNumber());

  // 1. Left Ctrl followed by Right Ctrl does not trigger.
  SendCtrlKeyDown(CtrlKey::kLeft);
  SendCtrlKeyDown(CtrlKey::kRight);
  task_environment_.RunUntilIdle();
  check_point.Call(1);

  // 2. A second Right Ctrl completes the Right Ctrl pair and triggers.
  SendCtrlKeyDown(CtrlKey::kRight);
  task_environment_.RunUntilIdle();
  check_point.Call(2);
}

class AtMemoryHandlerInactivityNudgeTest : public AtMemoryHandlerTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillAtMemoryInactivityNudge};
};

TEST_F(AtMemoryHandlerInactivityNudgeTest, InactivityTriggersNudge) {
  EXPECT_CALL(autofill_driver(), FormsSeen);
  LoadHTML(R"(<body><input id="input"></body>)");
  WaitForFormsSeen();

  SimulateElementClickAndWait("input");

  blink::WebFormControlElement element = GetFormControlElementById("input");
  element.SetValue(blink::WebString::FromUtf16(u"Elvis"));
  test_api(autofill_agent()).TextFieldValueChanged(element);

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, AutofillSuggestionTriggerSource::kAtMemoryInactivityNudge,
          _));

  task_environment_.FastForwardBy(base::Seconds(5));
}

}  // namespace

}  // namespace autofill
