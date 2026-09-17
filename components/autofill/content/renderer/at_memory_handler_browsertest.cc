// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/at_memory_handler.h"

#include <algorithm>
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
                              features::kAutofillAtMemory},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    test::AutofillRendererTest::SetUp();
    GetWebFrameWidget()->SetFocus(true);
    SetDoubleCtrlTrigger(true);
    run_loop_.emplace();
    ON_CALL(autofill_driver(), AskForValuesToFill)
        .WillByDefault([this](const FormData& form, FieldRendererId field_id,
                              const gfx::Rect& caret_bounds,
                              AutofillSuggestionTriggerSource trigger_source,
                              const std::optional<PasswordSuggestionRequest>&
                                  password_request) {
          if (IsAtMemoryTriggerSource(trigger_source)) {
            ApplyFieldActionAsync(field_id, fill_value_to_respond_,
                                  mojom::ActionPersistence::kFill);
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

  void SetShortcutTrigger(ui::KeyboardCode key_code, int modifiers) {
    blink::RendererPreferences prefs =
        GetMainRenderFrame()->GetWebView()->GetRendererPreferences();
    prefs.autofill_trigger_string = u"";
    prefs.autofill_shortcut_key_code = key_code;
    prefs.autofill_shortcut_modifiers = modifiers;
    GetMainRenderFrame()->GetWebView()->SetRendererPreferences(prefs);
  }

  void SetDoubleCtrlTrigger(bool enabled) {
    blink::RendererPreferences prefs =
        GetMainRenderFrame()->GetWebView()->GetRendererPreferences();
    prefs.autofill_at_memory_double_ctrl_trigger_enabled = enabled;
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
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/550313683): Parametrize remaining tests from
// `AtMemoryHandlerTest`.
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

  blink::WebElement field() { return GetWebElementById("f"); }

  FieldRendererId field_id() { return form_util::GetFieldRendererId(field()); }

  std::u16string GetValue() {
    if (auto form_control = field().DynamicTo<blink::WebFormControlElement>()) {
      return form_control.Value().Utf16();
    }
    std::u16string value = field().TextContent().Utf16();
    // Blink inserts non-breaking spaces (\u00A0) for trailing spaces in
    // contenteditable elements to prevent HTML whitespace collapsing.
    std::ranges::replace(value, u'\xA0', u' ');
    return value;
  }

  void SetValue(std::u16string_view value) {
    field().PasteText(blink::WebString::FromUtf16(value),
                      /*replace_all=*/true, /*smart_replace=*/false);
  }

  void SetSelectionRange(int start, int end) {
    CHECK(field().ContainsFrameSelection());
    CHECK(GetMainFrame()->SetEditableSelectionOffsets(start, end));
  }

  blink::WebRange GetSelectionRange() {
    CHECK(field().ContainsFrameSelection());
    return GetMainFrame()->GetInputMethodController()->GetSelectionOffsets();
  }

  int SelectionStart() { return GetSelectionRange().StartOffset(); }

  int SelectionEnd() { return GetSelectionRange().EndOffset(); }
};

INSTANTIATE_TEST_SUITE_P(AtMemoryHandlerTest,
                         AtMemoryHandlerTest_SingleField,
                         Values(FormControlType::kContentEditable,
                                FormControlType::kInputText,
                                FormControlType::kTextArea));

// Tests that the keyboard shortcut triggers AtMemory.
TEST_P(AtMemoryHandlerTest_SingleField, AtMemoryShortcutTrigger) {
  SetShortcutTrigger(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

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
  SetShortcutTrigger(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

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
  SetShortcutTrigger(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

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

// Tests that pressing Ctrl twice does not trigger AtMemory when the preference
// is disabled.
TEST_P(AtMemoryHandlerTest_SingleField, DoubleCtrlDisabledByPreference) {
  SetDoubleCtrlTrigger(false);

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
  SendCtrlKeyDown();
  task_environment_.RunUntilIdle();
}

// Tests that both custom shortcut and double Ctrl can trigger AtMemory when
// both are enabled.
TEST_P(AtMemoryHandlerTest_SingleField, ShortcutAndDoubleCtrlBothTrigger) {
  SetShortcutTrigger(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  SetDoubleCtrlTrigger(true);

  {
    testing::InSequence s;
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, _, _,
            Eq(AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut), _));
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl),
            _));
  }

  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _,
          AllOf(Ne(AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut),
                Ne(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl)),
          _))
      .Times(AnyNumber());

  // 1. Send configured shortcut.
  blink::WebKeyboardEvent shortcut_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kControlKey | blink::WebInputEvent::kShiftKey,
      base::TimeTicks::Now());
  shortcut_event.windows_key_code = ui::VKEY_Y;
  SendWebKeyboardEvent(shortcut_event);
  WaitForApplyFieldAction();

  // 2. Send double Ctrl.
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  WaitForApplyFieldAction();
}

// Tests that ApplyFieldAction() with kReplaceSelectionForAtMemory aborts if no
// matching entry is found in last_at_memory_ask_for_values_to_fills_.
TEST_P(AtMemoryHandlerTest_SingleField,
       AtMemoryReplaceTriggerAbortsIfNoHistoryEntryFound) {
  SetValue(u"hello");
  SetSelectionRange(5, 5);
  autofill_agent().ApplyFieldAction(
      mojom::FieldActionType::kReplaceSelectionForAtMemory,
      mojom::ActionPersistence::kFill, field_id(), u"result");
  // Filling should be aborted; value remains unchanged.
  EXPECT_EQ(GetValue(), u"hello");
}

// Tests that ApplyFieldAction() with kReplaceSelectionForAtMemory aborts if the
// value changed after AskForValuesToFill().
TEST_P(AtMemoryHandlerTest_SingleField,
       AtMemoryReplaceTriggerAbortsIfValueChanged) {
  SimulateSlowTyping("hello ");
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  SetValue(u"hello changed");
  WaitForApplyFieldAction();
  // Filling should be aborted; value remains unchanged.
  EXPECT_EQ(GetValue(), u"hello changed");
}

// Tests that ApplyFieldAction() with kReplaceSelectionForAtMemory refocuses the
// element and restores the caret if the element lost focus.
TEST_P(AtMemoryHandlerTest_SingleField, RefocusesAndRestoresCaretIfUnfocused) {
  ExecuteJavaScriptForTests(R"(
    document.body.insertAdjacentHTML('beforeend', '<input id="other">');
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
        EXPECT_EQ(other.GetDocument().FocusedElement(), other);
        ApplyFieldActionAsync(field_id);
      });

  SimulateSlowTyping("hello ");
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  WaitForApplyFieldAction();
  EXPECT_EQ(GetValue(), u"hello result");
  EXPECT_EQ(field().GetDocument().FocusedElement(), field());
}

// Tests that ApplyFieldAction() with kReplaceSelectionForAtMemory waits for
// window-level focus to return before filling if the window lost focus to a
// popup.
TEST_P(AtMemoryHandlerTest_SingleField, WaitsForWindowFocusBeforeFilling) {
  GetWebFrameWidget()->SetFocus(true);
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
  EXPECT_EQ(GetValue(), u"hello ");

  // After 20 ms, the first retry runs and finds the window is still unfocused.
  task_environment_.FastForwardBy(base::Milliseconds(20));
  EXPECT_EQ(GetValue(), u"hello ");

  // Restore window focus. The next retry in 20 ms will see the focused state.
  GetWebFrameWidget()->SetFocus(true);
  task_environment_.FastForwardBy(base::Milliseconds(20));

  EXPECT_EQ(GetValue(), u"hello result");
  EXPECT_EQ(field().GetDocument().FocusedElement(), field());
}

// Tests that ApplyFieldAction() with kReplaceSelectionForAtMemory falls back to
// filling the field without window focus once the retry limit is exceeded.
TEST_P(AtMemoryHandlerTest_SingleField,
       FillsAfterMaxRetriesIfWindowNeverGainsFocus) {
  GetWebFrameWidget()->SetFocus(true);

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
  EXPECT_EQ(GetValue(), u"hello ");

  // The 5th retry (at 100 ms) hits kMaxRetries and fills as a fallback.
  task_environment_.FastForwardBy(base::Milliseconds(20));
  EXPECT_EQ(GetValue(), u"hello result");
}

// Tests that kReplaceSelectionForAtMemory invoked via context menu
// can replace a selection as well as insert at the current cursor
// position.
TEST_P(AtMemoryHandlerTest_SingleField, ContextMenuTriggersAtMemory) {
  // 1. Replacement of a non-empty selection.
  SetValue(u"hello selection world");
  SetSelectionRange(6, 15);
  autofill_agent().TriggerSuggestions(
      field_id(), AutofillSuggestionTriggerSource::kAtMemoryContextMenu);
  WaitForApplyFieldAction();
  EXPECT_EQ(GetValue(), u"hello result world");
  EXPECT_EQ(SelectionStart(), 12);
  EXPECT_EQ(SelectionEnd(), 12);

  // 2. Insertion at caret without selection in pre-existing text.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  SetValue(u"hello result");
  SetSelectionRange(12, 12);
  set_fill_value_to_respond(u"extra");
  autofill_agent().TriggerSuggestions(
      field_id(), AutofillSuggestionTriggerSource::kAtMemoryContextMenu);
  WaitForApplyFieldAction();
  EXPECT_EQ(GetValue(), u"hello result extra");
  EXPECT_EQ(SelectionStart(), 18);
  EXPECT_EQ(SelectionEnd(), 18);
}

// Tests that kReplaceSelectionForAtMemory invoked via context menu
// automatically inserts surrounding whitespace via smart paste when replacing
// or inserting inside a word without surrounding spaces.
TEST_P(AtMemoryHandlerTest_SingleField,
       ContextMenuTriggersAtMemoryWithSmartPaste) {
  // 1. Replacement of a non-empty selection without surrounding spaces.
  SetValue(u"PrefixSelectedSuffix");
  SetSelectionRange(6, 14);
  autofill_agent().TriggerSuggestions(
      field_id(), AutofillSuggestionTriggerSource::kAtMemoryContextMenu);
  WaitForApplyFieldAction();
  EXPECT_EQ(GetValue(), u"Prefix result Suffix");
  EXPECT_EQ(SelectionStart(), 14);
  EXPECT_EQ(SelectionEnd(), 14);

  // 2. Insertion at caret in the middle of a word without selection.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  SetValue(u"PrefixSuffix");
  SetSelectionRange(6, 6);
  set_fill_value_to_respond(u"extra");
  autofill_agent().TriggerSuggestions(
      field_id(), AutofillSuggestionTriggerSource::kAtMemoryContextMenu);
  WaitForApplyFieldAction();
  EXPECT_EQ(GetValue(), u"Prefix extra Suffix");
  EXPECT_EQ(SelectionStart(), 13);
  EXPECT_EQ(SelectionEnd(), 13);
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

// Tests that even a non-contenteditable element within a contenteditable does
// not break filling.
//
// WebLocalFrame::ExtendSelectionAndReplace() cannot handle such cases because
// the selection, even if it is empty and unchanged, is put into the
// non-editable <span>.
// WebElement::PasteText() handles the case fine.
//
// This test mimics Gmail's placeholder (crbug.com/555717699).
TEST_F(AtMemoryHandlerTest, DoubleCtrlWithNonContentEditable) {
  LoadHTML(
      "<div contenteditable id=f>"
      "<span contenteditable=false>Foo</span>"
      "</div>");
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
  EXPECT_EQ(f.TextContent().Utf16(), u"Fooresult");
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

// Tests that pressing Ctrl twice in a password field doesn't trigger AtMemory.
TEST_F(AtMemoryHandlerTest, DoubleCtrlNotTriggeredOnPasswordField) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(0);

  LoadHTML(R"(<input id="f" type="password">)");
  WaitForFormsSeen();
  Focus("f");
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  task_environment_.RunUntilIdle();
}

// Tests that pressing Ctrl twice in a disabled field doesn't trigger AtMemory.
TEST_F(AtMemoryHandlerTest, DoubleCtrlNotTriggeredOnDisabledField) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(0);

  LoadHTML(R"(<input id="f" disabled>)");
  WaitForFormsSeen();
  Focus("f");
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  task_environment_.RunUntilIdle();
}

// Tests that pressing Ctrl twice in a read-only field doesn't trigger AtMemory.
TEST_F(AtMemoryHandlerTest, DoubleCtrlNotTriggeredOnReadOnlyField) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          _, _, _, Eq(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl), _))
      .Times(0);

  LoadHTML(R"(<input id="f" readonly>)");
  WaitForFormsSeen();
  Focus("f");
  SendCtrlKeyDown();
  SendCtrlKeyDown();
  task_environment_.RunUntilIdle();
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
