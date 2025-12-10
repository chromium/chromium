// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/type_tool.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/char_iterator.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "chrome/common/actor.mojom-shared.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/click_dispatcher.h"
#include "chrome/renderer/actor/click_tool.h"
#include "chrome/renderer/actor/key_dispatcher.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/latency/latency_info.h"

namespace actor {

using ::blink::WebCoalescedInputEvent;
using ::blink::WebElement;
using ::blink::WebFormControlElement;
using ::blink::WebInputEvent;
using ::blink::WebInputEventResult;
using ::blink::WebKeyboardEvent;
using ::blink::WebLocalFrame;
using ::blink::WebMouseEvent;
using ::blink::WebNode;
using ::blink::WebString;
using ::blink::WebWidget;

namespace {

// Typing into input fields often causes custom made dropdowns to appear and
// update content. These are often updated via async tasks that try to detect
// when a user has finished typing. Delay observation to try to ensure the page
// stability monitor kicks in only after these tasks have invoked.
constexpr base::TimeDelta kObservationDelay = base::Seconds(1);

// Structure to hold the mapping
struct KeyInfo {
  char16_t key_code;
  const char* dom_code;
  // The base character if it requires shift, 0 otherwise
  char16_t unmodified_char = 0;
};

// Function to provide access to the key info map.
// Initialization happens thread-safely on the first call.
const absl::flat_hash_map<char, KeyInfo>& GetKeyInfoMap() {
  // TODO(crbug.com/402082693): This map is a temporary solution in converting
  // between dom code and key code. We should find a central solution to this
  // that aligns with ui/events/keycodes/ data and functions.
  static const base::NoDestructor<absl::flat_hash_map<char, KeyInfo>>
      key_info_map([] {
        absl::flat_hash_map<char, KeyInfo> map_data = {
            {' ', {ui::VKEY_SPACE, "Space"}},
            {')', {ui::VKEY_0, "Digit0", u'0'}},
            {'!', {ui::VKEY_1, "Digit1", u'1'}},
            {'@', {ui::VKEY_2, "Digit2", u'2'}},
            {'#', {ui::VKEY_3, "Digit3", u'3'}},
            {'$', {ui::VKEY_4, "Digit4", u'4'}},
            {'%', {ui::VKEY_5, "Digit5", u'5'}},
            {'^', {ui::VKEY_6, "Digit6", u'6'}},
            {'&', {ui::VKEY_7, "Digit7", u'7'}},
            {'*', {ui::VKEY_8, "Digit8", u'8'}},
            {'(', {ui::VKEY_9, "Digit9", u'9'}},
            {';', {ui::VKEY_OEM_1, "Semicolon"}},
            {':', {ui::VKEY_OEM_1, "Semicolon", u';'}},
            {'=', {ui::VKEY_OEM_PLUS, "Equal"}},
            {'+', {ui::VKEY_OEM_PLUS, "Equal", u'='}},
            {',', {ui::VKEY_OEM_COMMA, "Comma"}},
            {'<', {ui::VKEY_OEM_COMMA, "Comma", u','}},
            {'-', {ui::VKEY_OEM_MINUS, "Minus"}},
            {'_', {ui::VKEY_OEM_MINUS, "Minus", u'-'}},
            {'.', {ui::VKEY_OEM_PERIOD, "Period"}},
            {'>', {ui::VKEY_OEM_PERIOD, "Period", u'.'}},
            {'/', {ui::VKEY_OEM_2, "Slash"}},
            {'?', {ui::VKEY_OEM_2, "Slash", u'/'}},
            {'`', {ui::VKEY_OEM_3, "Backquote"}},
            {'~', {ui::VKEY_OEM_3, "Backquote", u'`'}},
            {'[', {ui::VKEY_OEM_4, "BracketLeft"}},
            {'{', {ui::VKEY_OEM_4, "BracketLeft", u'['}},
            {'\\', {ui::VKEY_OEM_5, "Backslash"}},
            {'|', {ui::VKEY_OEM_5, "Backslash", u'\\'}},
            {']', {ui::VKEY_OEM_6, "BracketRight"}},
            {'}', {ui::VKEY_OEM_6, "BracketRight", u']'}},
            {'\'', {ui::VKEY_OEM_7, "Quote"}},
            {'"', {ui::VKEY_OEM_7, "Quote", u'\''}},
        };
        return map_data;
      }());
  return *key_info_map;
}

// Structure to hold the mapping for dead key compositions.
struct Composition {
  char16_t dead_key;
  char16_t second_key;
};

// Function to provide access to the composition map.
const absl::flat_hash_map<char16_t, Composition>& GetCompositionMap() {
  static const base::NoDestructor<absl::flat_hash_map<char16_t, Composition>>
      composition_map([] {
        absl::flat_hash_map<char16_t, Composition> map_data = {
            // Acute Accent (')
            {u'á', {'\'', 'a'}},
            {u'é', {'\'', 'e'}},
            {u'í', {'\'', 'i'}},
            {u'ó', {'\'', 'o'}},
            {u'ú', {'\'', 'u'}},
            {u'ý', {'\'', 'y'}},
            {u'Á', {'\'', 'A'}},
            {u'É', {'\'', 'E'}},
            {u'Í', {'\'', 'I'}},
            {u'Ó', {'\'', 'O'}},
            {u'Ú', {'\'', 'U'}},
            {u'Ý', {'\'', 'Y'}},

            // Grave Accent (`)
            {u'à', {'`', 'a'}},
            {u'è', {'`', 'e'}},
            {u'ì', {'`', 'i'}},
            {u'ò', {'`', 'o'}},
            {u'ù', {'`', 'u'}},
            {u'À', {'`', 'A'}},
            {u'È', {'`', 'E'}},
            {u'Ì', {'`', 'I'}},
            {u'Ò', {'`', 'O'}},
            {u'Ù', {'`', 'U'}},

            // Diaeresis / Umlaut (")
            {u'ä', {'"', 'a'}},
            {u'ë', {'"', 'e'}},
            {u'ï', {'"', 'i'}},
            {u'ö', {'"', 'o'}},
            {u'ü', {'"', 'u'}},
            {u'ÿ', {'"', 'y'}},
            {u'Ä', {'"', 'A'}},
            {u'Ë', {'"', 'E'}},
            {u'Ï', {'"', 'I'}},
            {u'Ö', {'"', 'O'}},
            {u'Ü', {'"', 'U'}},
            {u'Ÿ', {'"', 'Y'}},

            // Tilde (~)
            {u'ã', {'~', 'a'}},
            {u'ñ', {'~', 'n'}},
            {u'õ', {'~', 'o'}},
            {u'Ã', {'~', 'A'}},
            {u'Ñ', {'~', 'N'}},
            {u'Õ', {'~', 'O'}},

            // Circumflex (^)
            {u'â', {'^', 'a'}},
            {u'ê', {'^', 'e'}},
            {u'î', {'^', 'i'}},
            {u'ô', {'^', 'o'}},
            {u'û', {'^', 'u'}},
            {u'Â', {'^', 'A'}},
            {u'Ê', {'^', 'E'}},
            {u'Î', {'^', 'I'}},
            {u'Ô', {'^', 'O'}},
            {u'Û', {'^', 'U'}},

            // Cedilla (')
            {u'ç', {'\'', 'c'}},
            {u'Ç', {'\'', 'C'}},
        };
        return map_data;
      }());
  return *composition_map;
}

// Function to provide access to the AltGr map.
const absl::flat_hash_map<char16_t, char16_t>& GetAltGrMap() {
  static const base::NoDestructor<absl::flat_hash_map<char16_t, char16_t>>
      altgr_map([] {
        absl::flat_hash_map<char16_t, char16_t> map_data = {
            // Non-shifted characters
            {u'¡', u'1'},
            {u'²', u'2'},
            {u'³', u'3'},
            {u'€', u'5'},
            {u'¶', u';'},
            {u'æ', u'z'},
            {u'ß', u's'},
            {u'ð', u'd'},
            {u'ƒ', u'f'},
            {u'ø', u'l'},
            {u'´', u'j'},
            {u'þ', u't'},
            {u'å', u'w'},
            {u'©', u'c'},
            {u'®', u'r'},
            {u'µ', u'm'},
            {u'«', u'['},
            {u'»', u']'},
            {u'¿', u'/'},
            {u'¥', u'-'},
            // Characters requiring Shift
            {u'¹', u'!'},
            {u'¢', u'C'},
            {u'£', u'$'},
            {u'§', u'S'},
            {u'°', u':'},
            {u'Æ', u'Z'},
            {u'Ð', u'D'},
            {u'Ø', u'L'},
            {u'Þ', u'T'},
            {u'Å', u'W'},
        };
        return map_data;
      }());
  return *altgr_map;
}

bool PrepareTargetForMode(WebLocalFrame& frame, mojom::TypeAction::Mode mode) {
  // TODO(crbug.com/409570203): Use DELETE_EXISTING regardless of `mode` but
  // we'll have to implement the different insertion modes.
  frame.ExecuteCommand(WebString::FromUTF8("SelectAll"));
  return true;
}

}  // namespace

TypeTool::TypeTool(content::RenderFrame& frame,
                   TaskId task_id,
                   Journal& journal,
                   mojom::TypeActionPtr action,
                   mojom::ToolTargetPtr target,
                   mojom::ObservedToolTargetPtr observed_target)
    : ToolBase(frame,
               task_id,
               journal,
               std::move(target),
               std::move(observed_target)),
      action_(std::move(action)) {}

TypeTool::~TypeTool() = default;

KeyDispatcher::KeyParams TypeTool::GetEnterKeyParams() const {
  KeyDispatcher::KeyParams params;
  params.windows_key_code = ui::VKEY_RETURN;
  params.native_key_code =
      ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::ENTER);
  params.dom_code = "Enter";
  params.dom_key = "Enter";
  params.text = ui::VKEY_RETURN;
  params.unmodified_text = ui::VKEY_RETURN;
  return params;
}

KeyDispatcher::KeyParams TypeTool::GetBackspaceKeyParams() const {
  KeyDispatcher::KeyParams params;
  params.windows_key_code = ui::VKEY_BACK;
  params.native_key_code =
      ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::BACKSPACE);
  params.dom_code = "Backspace";
  params.dom_key = "Backspace";
  params.text = ui::VKEY_BACK;
  params.unmodified_text = ui::VKEY_BACK;
  return params;
}

std::optional<KeyDispatcher::KeyParams> TypeTool::GetKeyParamsForChar(
    char16_t c) const {
  // This function only supports ASCII characters. Non-ASCII characters are
  // handled by composition in the Validate() function.
  if (c > 0x7F) {
    return std::nullopt;
  }

  KeyDispatcher::KeyParams params;
  // Basic conversion assuming simple case.
  params.text = c;
  params.unmodified_text = c;

  char ascii_char = static_cast<char>(c);
  params.dom_key = std::string(1, ascii_char);

  // ASCII Lowercase letters
  if (base::IsAsciiLower(ascii_char)) {
    params.windows_key_code = ui::VKEY_A + (ascii_char - 'a');
    // dom_key and unmodified_text already set correctly
    params.dom_code = base::StrCat({"Key", {base::ToUpperASCII(ascii_char)}});
  } else if (base::IsAsciiUpper(ascii_char)) {
    // ASCII Uppercase letters
    params.windows_key_code = ui::VKEY_A + (ascii_char - 'A');
    params.dom_code = base::StrCat({"Key", {ascii_char}});
    // dom_key is already set correctly (it's the uppercase char)
    // Unmodified is lowercase
    params.unmodified_text = base::ToLowerASCII(ascii_char);
    params.modifiers = WebInputEvent::kShiftKey;
  } else if (base::IsAsciiDigit(ascii_char)) {
    // ASCII Digits
    params.windows_key_code = ui::VKEY_0 + (ascii_char - '0');
    // dom_key and unmodified is already set correctly
    params.dom_code = base::StrCat({"Digit", {ascii_char}});
  } else {
    // Symbols and Punctuation (US QWERTY layout assumed)
    const absl::flat_hash_map<char, KeyInfo>& key_info_map = GetKeyInfoMap();
    auto it = key_info_map.find(ascii_char);
    if (it == key_info_map.end()) {
      ACTOR_LOG() << "Character cannot be mapped directly to key event: "
                  << ascii_char;
      return std::nullopt;
    }

    const KeyInfo& info = it->second;
    params.windows_key_code = info.key_code;
    params.dom_code = info.dom_code;

    // Check if this character requires shift
    if (info.unmodified_char != 0) {
      params.modifiers = WebInputEvent::kShiftKey;
      params.unmodified_text = info.unmodified_char;
    }
  }

  // Set native_key_code (often matches windows_key_code, platform dependent)
  params.native_key_code = ui::KeycodeConverter::DomCodeToNativeKeycode(
      ui::KeycodeConverter::CodeStringToDomCode(params.dom_code));

  return params;
}

namespace {

std::string_view WebInputEventResultToString(WebInputEventResult result) {
  switch (result) {
    case WebInputEventResult::kNotHandled:
      return "NotHandled";
    case WebInputEventResult::kHandledSuppressed:
      return "HandledSuppressed";
    case WebInputEventResult::kHandledApplication:
      return "HandledApplication";
    case WebInputEventResult::kHandledSystem:
      return "HandledSystem";
  }
}

}  // namespace

WebInputEventResult TypeTool::CreateAndDispatchKeyEvent(
    WebWidget& widget,
    WebInputEvent::Type type,
    KeyDispatcher::KeyParams key_params) {
  WebKeyboardEvent key_event(type, key_params.modifiers, ui::EventTimeForNow());
  key_event.windows_key_code = key_params.windows_key_code;
  key_event.native_key_code = key_params.native_key_code;
  key_event.dom_code = static_cast<int>(
      ui::KeycodeConverter::CodeStringToDomCode(key_params.dom_code));
  key_event.dom_key =
      ui::KeycodeConverter::KeyStringToDomKey(key_params.dom_key);
  key_event.text[0] = key_params.text;
  key_event.unmodified_text[0] = key_params.unmodified_text;

  WebInputEventResult result = widget.HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));
  journal_->Log(task_id_, WebInputEvent::GetName(type),
                JournalDetailsBuilder()
                    .Add("key", key_params.dom_key)
                    .Add("result", WebInputEventResultToString(result))
                    .Build());

  return result;
}
mojom::ActionResultPtr TypeTool::SimulateKeyPress(
    KeyDispatcher::KeyParams params) {
  CHECK(resolved_target_);

  WebWidget* widget = resolved_target_->GetWidget(*this);
  if (!widget) {
    return MakeResult(mojom::ActionResultCode::kFrameWentAway,
                      /*requires_page_stabilization=*/false,
                      "No widget during simulate key down");
  }

  WebInputEventResult down_result = CreateAndDispatchKeyEvent(
      *widget, WebInputEvent::Type::kRawKeyDown, params);

  // Only the KeyDown event will check for and report failure. The reason the
  // other events don't is that if the KeyDown event was dispatched to the page,
  // the key input was observable to the page and it may mutate itself in a way
  // that subsequent Char and KeyUp events are suppressed (e.g. mutating the DOM
  // tree, removing frames, etc). These "failure" cases can be considered
  // successful in terms that the tool has acted on the page. In particular, a
  // preventDefault()'ed KeyDown event will force suppressing the following Char
  // event but this is expected and common.
  if (down_result == WebInputEventResult::kHandledSuppressed) {
    return MakeResult(mojom::ActionResultCode::kTypeKeyDownSuppressed,
                      /*requires_page_stabilization=*/false,
                      absl::StrFormat("Suppressed char[%s]", params.dom_key));
  }

  // Input handling could cause the widget to be destroyed so it must be
  // re-read.
  widget = resolved_target_->GetWidget(*this);
  if (!widget) {
    return MakeResult(mojom::ActionResultCode::kFrameWentAway,
                      /*requires_page_stabilization=*/false,
                      "No widget after simulate key down");
  }

  if (params.dom_key != "Dead") {
    WebInputEventResult char_result =
        CreateAndDispatchKeyEvent(*widget, WebInputEvent::Type::kChar, params);
    if (char_result == WebInputEventResult::kHandledSuppressed) {
      ACTOR_LOG() << "Warning: Char event for key " << params.dom_key
                  << " suppressed.";
    }
  }

  // Input handling could cause the widget to be destroyed so it must be
  // re-read.
  widget = resolved_target_->GetWidget(*this);
  if (!widget) {
    return MakeResult(mojom::ActionResultCode::kFrameWentAway,
                      /*requires_page_stabilization=*/false,
                      "No widget after simulate key char");
  }

  WebInputEventResult up_result =
      CreateAndDispatchKeyEvent(*widget, WebInputEvent::Type::kKeyUp, params);
  if (up_result == WebInputEventResult::kHandledSuppressed) {
    ACTOR_LOG() << "Warning: KeyUp event for key " << params.dom_key
                << " suppressed.";
  }

  return MakeOkResult();
}

void TypeTool::Execute(ToolFinishedCallback callback) {
  ValidatedResult validated_target = Validate();
  if (!validated_target.has_value()) {
    std::move(callback).Run(std::move(validated_target.error()));
    return;
  }

  // Injecting a click to get focus.
  resolved_target_ = validated_target.value();
  journal_->Log(task_id_, "TypeTool::Execute::Focus",
                JournalDetailsBuilder()
                    .Add("coord", resolved_target_->widget_point)
                    .Build());
  CHECK(!click_dispatcher_);
  click_dispatcher_.emplace(
      blink::WebMouseEvent::Button::kLeft, 1, *resolved_target_, *this,
      base::BindOnce(&TypeTool::OnFocusingClickComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TypeTool::Cancel() {
  // Clicking is completed before key dispatching, so there shouldn't be both.
  CHECK(!(click_dispatcher_ && key_dispatcher_));

  if (click_dispatcher_) {
    click_dispatcher_->Cancel();
    click_dispatcher_.reset();
  }
  if (key_dispatcher_) {
    key_dispatcher_->Cancel();
    key_dispatcher_.reset();
  }
}

void TypeTool::OnFocusingClickComplete(ToolFinishedCallback callback,
                                       mojom::ActionResultPtr click_result) {
  CHECK(resolved_target_.has_value());
  click_dispatcher_.reset();

  // Cancel rest of typing if initial click failed.
  if (!IsOk(*click_result)) {
    journal_->Log(
        task_id_, "TypeTool::Execute::ClickFailed",
        JournalDetailsBuilder().AddError(click_result->message).Build());
    std::move(callback).Run(std::move(click_result));
    return;
  }

  WebWidget* widget = resolved_target_->GetWidget(*this);
  if (!widget) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kFrameWentAway,
                   /*requires_page_stabilization=*/false,
                   "Widget went away after focusing click."));
    return;
  }

  // Preparing the target editable and checks on focused element currently
  // assume we're using a FrameWidget. We'd have to rewrite those to be Widget
  // agonistic in order to support these for popups. Skip for now since this
  // isn't really a use case today.
  bool is_for_popup = resolved_target_->popup_handle.has_value();

  // Note: Focus and preparing the target performs actions which lead to
  // script execution so `node` may no longer be focused (it or its frame
  // could be disconnected). However, sites sometimes do unexpected things to
  // work around issues so to keep those working we proceed to key dispatch
  // without checking this.

  // Only prepare target if the click resulted in focusing an editable.
  // TODO(crbug.com/421133798): If the target isn't editable, the existing
  // TypeAction modes don't make sense.
  WebLocalFrame* focused_frame = is_for_popup
                                     ? nullptr
                                     : frame_->GetWebFrame()
                                           ->FrameWidget()
                                           ->FocusedWebLocalFrameInWidget();
  WebElement focused_element =
      focused_frame ? focused_frame->GetDocument().FocusedElement()
                    : WebElement();
  bool in_editing_context = focused_element && focused_element.IsEditable();
  if (in_editing_context) {
    journal_->Log(
        task_id_, "TypeTool::Execute::FocusElementEditable",
        JournalDetailsBuilder().Add("focus", focused_element).Build());
    PrepareTargetForMode(*focused_frame, action_->mode);
  } else {
    if (focused_element) {
      journal_->Log(
          task_id_, "TypeTool::Execute::FocusElementNotEditable",
          JournalDetailsBuilder().Add("focus", focused_element).Build());
      // TODO(crbug.com/421133798): If the target isn't editable, the existing
      // TypeAction modes don't make sense.
      ACTOR_LOG()
          << "Warning: TypeAction::Mode cannot be applied when targeting "
             "a non-editable ["
          << focused_element << "]. https://crbug.com/421133798.";
    } else {
      journal_->Log(task_id_, "TypeTool::Execute::NoFocusElement", {});
      ACTOR_LOG()
          << "Warning: TypeAction::Mode cannot be applied when there is no "
             "focused element in the widget. https://crbug.com/432551725.";
    }
  }

  bool can_simulate_typing = false;
  // Empty text is used to simulate clearing the existing text with Backspace.
  if (action_->text.empty()) {
    can_simulate_typing = true;
    key_sequence_.reserve(action_->follow_by_enter ? 2 : 1);
    key_sequence_.push_back(GetBackspaceKeyParams());
    if (action_->follow_by_enter) {
      key_sequence_.push_back(GetEnterKeyParams());
    }
  } else {
    // Reserve two space per letter in text in case of composition keys.
    key_sequence_.reserve(2 * action_->text.length() +
                          (action_->follow_by_enter ? 1 : 0));
    can_simulate_typing = ProcessInputText(key_sequence_);
  }

  if (can_simulate_typing) {
    if (!base::FeatureList::IsEnabled(features::kGlicActorIncrementalTyping)) {
      for (const auto& param : key_sequence_) {
        mojom::ActionResultPtr result = SimulateKeyPress(param);
        if (!IsOk(*result)) {
          // The initial click may have changed the page.
          result->requires_page_stabilization = true;

          std::move(callback).Run(std::move(result));
          return;
        }
      }
      std::move(callback).Run(MakeOkResult());
    } else {
      journal_->Log(task_id_, "TypeTool::Execute::TypeWithDelay",
                    JournalDetailsBuilder()
                        .Add("delay", features::kGlicActorKeyUpDuration.Get())
                        .Build());
      CHECK(resolved_target_);
      key_dispatcher_.emplace(std::move(key_sequence_), std::move(action_),
                              *resolved_target_, *this, std::move(callback),
                              task_id_, *journal_);
    }
    return;
  }
  // Fallback to using PasteText when we can't simulate typing.
  if (in_editing_context) {
    journal_->Log(task_id_, "TypeTool::Execute::PasteTextFallback",
                  JournalDetailsBuilder()
                      .Add("text", action_->text)
                      .Add("focus", focused_element)
                      .Build());
    focused_element.PasteText(WebString::FromUTF8(action_->text),
                              /*replace_all=*/false);
    std::move(callback).Run(MakeOkResult());
  } else {
    std::move(callback).Run(MakeResult(
        mojom::ActionResultCode::kTypeUnsupportedCharacters,
        /*requires_page_stabilization=*/false,
        "Cannot paste text with unsupported characters because no editable "
        "element is focused after click."));
  }
}

std::string TypeTool::DebugString() const {
  return absl::StrFormat("TypeTool[%s;text(%s);mode(%s);FollowByEnter(%v)]",
                         ToDebugString(target_), action_->text,
                         base::ToString(action_->mode),
                         action_->follow_by_enter);
}

base::TimeDelta TypeTool::ExecutionObservationDelay() const {
  return kObservationDelay;
}

bool TypeTool::SupportsPaintStability() const {
  return true;
}

TypeTool::ValidatedResult TypeTool::Validate() const {
  CHECK(frame_->GetWebFrame());
  CHECK(frame_->GetWebFrame()->FrameWidget());

  CHECK(target_);

  auto resolved_target = ValidateAndResolveTarget();
  if (!resolved_target.has_value()) {
    return base::unexpected(std::move(resolved_target.error()));
  }

  if (target_->is_dom_node_id()) {
    const WebNode& node = resolved_target->node;
    if (!node.IsElementNode()) {
      return base::unexpected(
          MakeResult(mojom::ActionResultCode::kTypeTargetNotElement));
    }

    WebElement element = node.To<WebElement>();
    if (WebFormControlElement form_control =
            element.DynamicTo<WebFormControlElement>()) {
      if (!form_control.IsEnabled()) {
        return base::unexpected(
            MakeResult(mojom::ActionResultCode::kElementDisabled));
      }
    }
  }
  return resolved_target;
}

bool TypeTool::ProcessInputText(
    std::vector<KeyDispatcher::KeyParams>& key_sequence) const {
  // Skip typing simulation for very long text.
  if (action_->text.length() >
      features::kGlicActorIncrementalTypingLongTextPasteThreshold.Get()) {
    return false;
  }

  const absl::flat_hash_map<char16_t, Composition>& composition_map =
      GetCompositionMap();

  for (base::i18n::UTF8CharIterator iter(action_->text); !iter.end();
       iter.Advance()) {
    int32_t code_point = iter.get();

    if (code_point > 0xFFFF) {
      // supplementary plane characters cannot be simulated.
      return false;
    }
    char16_t c = static_cast<char16_t>(code_point);

    // Handle simple ASCII character
    std::optional<KeyDispatcher::KeyParams> params = GetKeyParamsForChar(c);
    if (params.has_value()) {
      key_sequence.push_back(params.value());
      continue;
    }

    // Handle characters requiring composition (dead key)
    auto comp_it = composition_map.find(c);
    if (comp_it != composition_map.end()) {
      const Composition& composition = comp_it->second;
      std::optional<KeyDispatcher::KeyParams> dead_key_params =
          GetKeyParamsForChar(composition.dead_key);
      if (!dead_key_params.has_value()) {
        return false;
      }
      dead_key_params->unmodified_text = 0;
      dead_key_params->text = 0;
      dead_key_params->dom_key = "Dead";
      key_sequence.push_back(dead_key_params.value());

      std::optional<KeyDispatcher::KeyParams> base_key_params =
          GetKeyParamsForChar(composition.second_key);
      if (!base_key_params.has_value()) {
        return false;
      }
      base_key_params->text = c;
      base_key_params->unmodified_text = c;
      key_sequence.push_back(base_key_params.value());
      continue;
    }

    // Handle characters requiring AltGr combo key
    const absl::flat_hash_map<char16_t, char16_t>& altgr_map = GetAltGrMap();
    auto altgr_it = altgr_map.find(c);
    if (altgr_it != altgr_map.end()) {
      std::optional<KeyDispatcher::KeyParams> base_key_params =
          GetKeyParamsForChar(altgr_it->second);
      if (!base_key_params.has_value()) {
        return false;
      }
      base_key_params->modifiers |= WebInputEvent::kAltGrKey;
      base_key_params->text = c;
      base_key_params->unmodified_text = c;
      key_sequence.push_back(base_key_params.value());
      continue;
    }

    // The character is unsupported.
    return false;
  }

  if (action_->follow_by_enter) {
    key_sequence.push_back(GetEnterKeyParams());
  }

  return true;
}

}  // namespace actor
