// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/type_tool.h"

#include <string>

#include "base/feature_list.h"
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
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/click_tool.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
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
const std::unordered_map<char, KeyInfo>& GetKeyInfoMap() {
  // TODO(crbug.com/402082693): This map is a temporary solution in converting
  // between dom code and key code. We should find a central solution to this
  // that aligns with ui/events/keycodes/ data and functions.
  static const base::NoDestructor<std::unordered_map<char, KeyInfo>>
      key_info_map([] {
        std::unordered_map<char, KeyInfo> map_data = {
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

bool PrepareTargetForMode(WebLocalFrame& frame, mojom::TypeAction::Mode mode) {
  // TODO(crbug.com/409570203): Use DELETE_EXISTING regardless of `mode` but
  // we'll have to implement the different insertion modes.
  frame.ExecuteCommand(WebString::FromUTF8("SelectAll"));
  return true;
}

}  // namespace

TypeTool::TargetAndKeys::TargetAndKeys(const gfx::PointF& coordinate,
                                       std::vector<KeyParams> key_sequence)
    : target(coordinate), key_sequence(std::move(key_sequence)) {}

TypeTool::TargetAndKeys::~TargetAndKeys() = default;
TypeTool::TargetAndKeys::TargetAndKeys(const TargetAndKeys&) = default;
TypeTool::TargetAndKeys& TypeTool::TargetAndKeys::operator=(
    const TargetAndKeys&) = default;
TypeTool::TargetAndKeys::TargetAndKeys(TargetAndKeys&&) = default;
TypeTool::TargetAndKeys& TypeTool::TargetAndKeys::operator=(TargetAndKeys&&) =
    default;

TypeTool::KeyParams::KeyParams() = default;
TypeTool::KeyParams::~KeyParams() = default;
TypeTool::KeyParams::KeyParams(const KeyParams& other) = default;

TypeTool::TypeTool(content::RenderFrame& frame,
                   Journal::TaskId task_id,
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

TypeTool::KeyParams TypeTool::GetEnterKeyParams() const {
  TypeTool::KeyParams params;
  params.windows_key_code = ui::VKEY_RETURN;
  params.native_key_code =
      ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::ENTER);
  params.dom_code = "Enter";
  params.dom_key = "Enter";
  params.text = ui::VKEY_RETURN;
  params.unmodified_text = ui::VKEY_RETURN;
  return params;
}

std::optional<TypeTool::KeyParams> TypeTool::GetKeyParamsForChar(char c) const {
  TypeTool::KeyParams params;
  // Basic conversion assuming simple case.
  params.text = c;
  params.unmodified_text = c;
  params.dom_key = std::string(1, c);

  // ASCII Lowercase letters
  if (base::IsAsciiLower(c)) {
    params.windows_key_code = ui::VKEY_A + (c - 'a');
    // dom_key and unmodified_text already set correctly
    params.dom_code = base::StrCat({"Key", {base::ToUpperASCII(c)}});
  } else if (c >= 'A' && c <= 'Z') {
    // ASCII Uppercase letters
    params.windows_key_code = ui::VKEY_A + (c - 'A');
    params.dom_code = base::StrCat({"Key", {c}});
    // dom_key is already set correctly (it's the uppercase char)
    // Unmodified is lowercase
    params.unmodified_text = base::ToLowerASCII(c);
    params.modifiers = WebInputEvent::kShiftKey;
  } else if (c >= '0' && c <= '9') {
    // ASCII Digits
    params.windows_key_code = ui::VKEY_0 + (c - '0');
    // dom_key and unmodified is already set correctly
    params.dom_code = base::StrCat({"Digit", {c}});
  } else {
    // Symbols and Punctuation (US QWERTY layout assumed)
    const std::unordered_map<char, KeyInfo>& key_info_map = GetKeyInfoMap();
    auto it = key_info_map.find(c);
    if (it == key_info_map.end()) {
      ACTOR_LOG() << "Character cannot be mapped directly to key event: " << c;
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
    WebInputEvent::Type type,
    KeyParams key_params) {
  WebKeyboardEvent key_event(type, key_params.modifiers, ui::EventTimeForNow());
  key_event.windows_key_code = key_params.windows_key_code;
  key_event.native_key_code = key_params.native_key_code;
  key_event.dom_code = static_cast<int>(
      ui::KeycodeConverter::CodeStringToDomCode(key_params.dom_code));
  key_event.dom_key =
      ui::KeycodeConverter::KeyStringToDomKey(key_params.dom_key);
  key_event.text[0] = key_params.text;
  key_event.unmodified_text[0] = key_params.unmodified_text;

  WebInputEventResult result =
      frame_->GetWebFrame()->FrameWidget()->HandleInputEvent(
          WebCoalescedInputEvent(key_event, ui::LatencyInfo()));
  journal_->Log(
      task_id_, WebInputEvent::GetName(type),
      absl::StrFormat("%s[%s] -> %s", WebInputEvent::GetName(type),
                      key_params.dom_key, WebInputEventResultToString(result)));

  return result;
}
mojom::ActionResultPtr TypeTool::SimulateKeyPress(TypeTool::KeyParams params) {
  WebInputEventResult down_result =
      CreateAndDispatchKeyEvent(WebInputEvent::Type::kRawKeyDown, params);

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
                      absl::StrFormat("Suppressed char[%s]", params.dom_key));
  }

  WebInputEventResult char_result =
      CreateAndDispatchKeyEvent(WebInputEvent::Type::kChar, params);
  if (char_result == WebInputEventResult::kHandledSuppressed) {
    ACTOR_LOG() << "Warning: Char event for key " << params.dom_key
                << " suppressed.";
  }

  WebInputEventResult up_result =
      CreateAndDispatchKeyEvent(WebInputEvent::Type::kKeyUp, params);
  if (up_result == WebInputEventResult::kHandledSuppressed) {
    ACTOR_LOG() << "Warning: KeyUp event for key " << params.dom_key
                << " suppressed.";
  }

  return MakeOkResult();
}

void TypeTool::Execute(ToolFinishedCallback callback) {
  ValidatedResult validated_result = Validate();
  if (!validated_result.has_value()) {
    std::move(callback).Run(std::move(validated_result.error()));
    return;
  }

  // Injecting a click to get focus.
  gfx::PointF coordinate = validated_result->target;
  journal_->Log(
      task_id_, "TypeTool::Execute",
      absl::StrFormat("Click to focus on %s", base::ToString(coordinate)));
  mojom::ActionResultPtr click_result =
      CreateAndDispatchClick(blink::WebMouseEvent::Button::kLeft, 1, coordinate,
                             frame_->GetWebFrame()->FrameWidget());

  // Cancel rest of typing if initial click failed.
  if (!IsOk(*click_result)) {
    journal_->Log(
        task_id_, "TypeTool::Execute",
        absl::StrFormat("Initial click to focus target failed. Reason: %s",
                        click_result->message));
    std::move(callback).Run(std::move(click_result));
    return;
  }

  // Note: Focus and preparing the target performs actions which lead to
  // script execution so `node` may no longer be focused (it or its frame
  // could be disconnected). However, sites sometimes do unexpected things to
  // work around issues so to keep those working we proceed to key dispatch
  // without checking this.

  // Only prepare target if the click resulted in focusing an
  // editable.
  // TODO(crbug.com/421133798): If the target isn't editable, the existing
  // TypeAction modes don't make sense.
  WebElement focused = frame_->GetWebFrame()->GetDocument().FocusedElement();
  if (focused && focused.IsEditable()) {
    journal_->Log(
        task_id_, "TypeTool::Execute",
        absl::StrFormat("Focused element is now %s", base::ToString(focused)));
    PrepareTargetForMode(*frame_->GetWebFrame(), action_->mode);
  } else {
    journal_->Log(
        task_id_, "TypeTool::Execute",
        absl::StrFormat(
            "Target %s is not editable. Typing will proceed without clearing.",
            base::ToString(focused)));
    // TODO(crbug.com/421133798): If the target isn't editable, the existing
    // TypeAction modes don't make sense.
    ACTOR_LOG() << "Warning: TypeAction::Mode cannot be applied when targeting "
                   "a non-editable ["
                << focused << "]. https://crbug.com/421133798.";
  }

  if (!base::FeatureList::IsEnabled(features::kGlicActorIncrementalTyping)) {
    for (const auto& param : validated_result->key_sequence) {
      mojom::ActionResultPtr result = SimulateKeyPress(param);
      if (!IsOk(*result)) {
        std::move(callback).Run(std::move(result));
        return;
      }
    }

    std::move(callback).Run(MakeOkResult());
  } else {
    journal_->Log(task_id_, "TypeTool::Execute",
                  absl::StrFormat(
                      "Use incremental typing with %s delay",
                      base::ToString(features::kGlicActorKeyUpDuration.Get())));
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
    target_and_keys_ = std::move(validated_result).value();
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TypeTool::ContinueIncrementalTyping,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        features::kGlicActorKeyUpDuration.Get());
  }
}

void TypeTool::ContinueIncrementalTyping(ToolFinishedCallback callback) {
  const KeyParams& params = target_and_keys_->key_sequence[current_key_];

  if (!is_key_down_) {
    WebInputEventResult down_result =
        CreateAndDispatchKeyEvent(WebInputEvent::Type::kRawKeyDown, params);

    // Only the KeyDown event will check for and report failure. The reason the
    // other events don't is that if the KeyDown event was dispatched to the
    // page, the key input was observable to the page and it may mutate itself
    // in a way that subsequent Char and KeyUp events are suppressed (e.g.
    // mutating the DOM tree, removing frames, etc). These "failure" cases can
    // be considered successful in terms that the tool has acted on the page. In
    // particular, a preventDefault()'ed KeyDown event will force suppressing
    // the following Char event but this is expected and common.
    if (down_result == WebInputEventResult::kHandledSuppressed) {
      std::move(callback).Run(
          MakeResult(mojom::ActionResultCode::kTypeKeyDownSuppressed,
                     absl::StrFormat("Suppressed char[%s]", params.dom_key)));
      return;
    }

    CreateAndDispatchKeyEvent(WebInputEvent::Type::kChar, params);

    is_key_down_ = true;
  } else {
    CreateAndDispatchKeyEvent(WebInputEvent::Type::kKeyUp, params);
    is_key_down_ = false;
    current_key_++;
  }

  if (current_key_ >= target_and_keys_->key_sequence.size()) {
    std::move(callback).Run(MakeOkResult());
  } else {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TypeTool::ContinueIncrementalTyping,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        (is_key_down_ ? features::kGlicActorKeyDownDuration
                      : features::kGlicActorKeyUpDuration)
            .Get());
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

  // Perform typing specific validation.
  if (!base::IsStringASCII(action_->text)) {
    // TODO(crbug.com/409032824): Add support beyond ASCII.
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kTypeUnsupportedCharacters));
  }

  std::vector<KeyParams> key_sequence;
  key_sequence.reserve(action_->text.length() +
                       (action_->follow_by_enter ? 1 : 0));
  // Validate all characters in text.
  for (char c : action_->text) {
    std::optional<KeyParams> params = GetKeyParamsForChar(c);
    if (!params.has_value()) {
      journal_->Log(
          task_id_, "TypeTool::Validate",
          absl::StrFormat("Failed to map character '%c' to a key event.", c));
      return base::unexpected(
          MakeResult(mojom::ActionResultCode::kTypeFailedMappingCharToKey,
                     absl::StrFormat("Failed on char[%c]", c)));
    }
    key_sequence.push_back(params.value());
  }
  if (action_->follow_by_enter) {
    key_sequence.push_back(GetEnterKeyParams());
  }

  return TargetAndKeys{resolved_target->point, std::move(key_sequence)};
}

}  // namespace actor
