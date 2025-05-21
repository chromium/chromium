// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/type_tool.h"

#include <string>

#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "chrome/common/actor.mojom-shared.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/renderer/actor/click_tool.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_element.h"
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
using ::blink::WebInputEvent;
using ::blink::WebInputEventResult;
using ::blink::WebKeyboardEvent;
using ::blink::WebLocalFrame;
using ::blink::WebMouseEvent;
using ::blink::WebNode;
using ::blink::WebString;

namespace {

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

bool PrepareTargetForMode(WebLocalFrame& frame,
                          mojom::TypeAction::Mode mode,
                          bool is_target_editable) {
  // Skip prepration if target is not editable.
  if (is_target_editable) {
    // TODO(crbug.com/409570203): Use DELETE_EXISTING regardless of `mode` but
    // we'll have to implement the different insertion modes.
    frame.ExecuteCommand(WebString::FromUTF8("SelectAll"));
  }
  return true;
}

}  // namespace

TypeTool::TypeTool(mojom::TypeActionPtr action, content::RenderFrame& frame)
    : frame_(frame), action_(std::move(action)) {}

TypeTool::~TypeTool() = default;

TypeTool::KeyParams::KeyParams() = default;
TypeTool::KeyParams::~KeyParams() = default;
TypeTool::KeyParams::KeyParams(const KeyParams& other) = default;

TypeTool::KeyParams TypeTool::GetEnterKeyParams() {
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

std::optional<TypeTool::KeyParams> TypeTool::GetKeyParamsForChar(char c) {
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

  return result;
}

bool TypeTool::SimulateKeyPress(TypeTool::KeyParams params) {
  // TODO(crbug.com/402082693): Maybe add slight delay between events?
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
    ACTOR_LOG() << "KeyDown event for key " << params.dom_key << " suppressed.";
    return false;
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
  return true;
}

void TypeTool::Execute(ToolFinishedCallback callback) {
  if (!frame_->GetWebFrame() || !frame_->GetWebFrame()->FrameWidget()) {
    ACTOR_LOG() << "RenderFrame or FrameWidget is invalid.";
    std::move(callback).Run(MakeErrorResult());
    return;
  }

  mojom::ToolTargetPtr& target = action_->target;
  CHECK(target);

  bool is_target_editable;

  if (target->is_coordinate()) {
    // Injecting a click first at the coordinate.
    gfx::PointF click_point(target->get_coordinate());
    if (!IsPointWithinViewport(click_point, frame_.get())) {
      std::move(callback).Run(
          MakeResult(mojom::ActionResultCode::kCoordinatesOutOfBounds));
      return;
    }
    mojom::ActionResultPtr result = CreateAndDispatchClick(
        blink::WebMouseEvent::Button::kLeft, 1, click_point,
        frame_->GetWebFrame()->FrameWidget());
    // Cancel rest of typing if initial click failed.
    if (!IsOk(*result)) {
      std::move(callback).Run(std::move(result));
      return;
    }
    blink::WebHitTestResult htresult =
        frame_->GetWebFrame()->FrameWidget()->HitTestResultAt(click_point);
    // Only prepare target if the hit test result indicates the node is
    // editable.
    is_target_editable = htresult.IsContentEditable();
  } else {
    int32_t dom_node_id = target->get_dom_node_id();
    WebNode node = GetNodeFromId(frame_.get(), dom_node_id);
    if (node.IsNull()) {
      ACTOR_LOG() << "Cannot find dom node with id " << dom_node_id;
      std::move(callback).Run(MakeErrorResult());
      return;
    }

    // Validate Node is an editable element
    // TODO(crbug.com/414398425): This seems too restrictive for non-input
    // cases.
    if (!node.IsElementNode()) {
      ACTOR_LOG() << "Target node " << node << " is not an element.";
      std::move(callback).Run(MakeErrorResult());
      return;
    }
    WebElement element = node.To<WebElement>();
    if (!element.IsEditable()) {
      ACTOR_LOG() << "Target element " << element << " is not editable.";
      std::move(callback).Run(MakeErrorResult());
      return;
    }

    // Check and set focus if needed.
    if (!IsNodeFocused(frame_.get(), node)) {
      if (element.IsFocusable()) {
        element.Focus();
      } else {
        ACTOR_LOG() << "Target element " << element
                    << " is not focusable for typing.";
        std::move(callback).Run(MakeErrorResult());
        return;
      }
    }
    is_target_editable = true;
  }

  if (!PrepareTargetForMode(*frame_->GetWebFrame(), action_->mode,
                            is_target_editable)) {
    ACTOR_LOG() << "Failed to prepare target element based on mode: "
                << action_->mode;
    std::move(callback).Run(MakeErrorResult());
    return;
  }

  // Note: Focus and preparing the target performs actions which lead to
  // script execution so `node` may no longer be focused (it or its frame
  // could be disconnected). However, sites sometimes do unexpected things to
  // work around issues so to keep those working we proceed to key dispatch
  // without checking this.

  if (!base::IsStringASCII(action_->text)) {
    // TODO(crbug.com/409032824): Add support beyond ASCII.
    ACTOR_LOG() << "Characters beyond ASCII not supported" << action_->text;
    std::move(callback).Run(MakeErrorResult());
    return;
  }

  std::vector<KeyParams> key_sequence;
  key_sequence.reserve(action_->text.length() +
                       (action_->follow_by_enter ? 1 : 0));
  // Validate all characters in text before simulating key presses.
  for (char c : action_->text) {
    std::optional<KeyParams> params = GetKeyParamsForChar(c);
    if (!params.has_value()) {
      ACTOR_LOG() << "Failed to map char to key " << c;
      std::move(callback).Run(MakeErrorResult());
      return;
    }
    key_sequence.push_back(params.value());
  }
  if (action_->follow_by_enter) {
    key_sequence.push_back(GetEnterKeyParams());
  }

  for (const auto& param : key_sequence) {
    if (!SimulateKeyPress(param)) {
      ACTOR_LOG() << "Failed to simulate key press for " << param.dom_key;
      std::move(callback).Run(MakeErrorResult());
      return;
    }
  }

  std::move(callback).Run(MakeOkResult());
}

std::string TypeTool::DebugString() const {
  return absl::StrFormat("TypeTool[%s;text(%s);mode(%s);FollowByEnter(%v)]",
                         ToDebugString(action_->target), action_->text,
                         base::ToString(action_->mode),
                         action_->follow_by_enter);
}

}  // namespace actor
