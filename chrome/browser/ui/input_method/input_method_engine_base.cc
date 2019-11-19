// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/input_method/input_method_engine_base.h"

#include <algorithm>
#include <map>
#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/chromeos/ime_keymap.h"
#elif defined(OS_WIN)
#include "ui/events/keycodes/keyboard_codes_win.h"
#elif defined(OS_LINUX)
#include "ui/events/keycodes/keyboard_codes_posix.h"
#endif

namespace input_method {

namespace {

const char kErrorNotActive[] = "IME is not active";
const char kErrorWrongContext[] = "Context is not active";
const char kErrorInvalidValue[] = "Argument '%s' with value '%d' is not valid";

#if defined(OS_CHROMEOS)
std::string GetKeyFromEvent(const ui::KeyEvent& event) {
  const std::string code = event.GetCodeString();
  if (base::StartsWith(code, "Control", base::CompareCase::SENSITIVE))
    return "Ctrl";
  if (base::StartsWith(code, "Shift", base::CompareCase::SENSITIVE))
    return "Shift";
  if (base::StartsWith(code, "Alt", base::CompareCase::SENSITIVE))
    return "Alt";
  if (base::StartsWith(code, "Arrow", base::CompareCase::SENSITIVE))
    return code.substr(5);
  if (code == "Escape")
    return "Esc";
  if (code == "Backspace" || code == "Tab" || code == "Enter" ||
      code == "CapsLock" || code == "Power")
    return code;
  // Cases for media keys.
  switch (event.key_code()) {
    case ui::VKEY_BROWSER_BACK:
    case ui::VKEY_F1:
      return "HistoryBack";
    case ui::VKEY_BROWSER_FORWARD:
    case ui::VKEY_F2:
      return "HistoryForward";
    case ui::VKEY_BROWSER_REFRESH:
    case ui::VKEY_F3:
      return "BrowserRefresh";
    case ui::VKEY_MEDIA_LAUNCH_APP2:
    case ui::VKEY_F4:
      return "ChromeOSFullscreen";
    case ui::VKEY_MEDIA_LAUNCH_APP1:
    case ui::VKEY_F5:
      return "ChromeOSSwitchWindow";
    case ui::VKEY_BRIGHTNESS_DOWN:
    case ui::VKEY_F6:
      return "BrightnessDown";
    case ui::VKEY_BRIGHTNESS_UP:
    case ui::VKEY_F7:
      return "BrightnessUp";
    case ui::VKEY_VOLUME_MUTE:
    case ui::VKEY_F8:
      return "AudioVolumeMute";
    case ui::VKEY_VOLUME_DOWN:
    case ui::VKEY_F9:
      return "AudioVolumeDown";
    case ui::VKEY_VOLUME_UP:
    case ui::VKEY_F10:
      return "AudioVolumeUp";
    default:
      break;
  }
  uint16_t ch = 0;
  // Ctrl+? cases, gets key value for Ctrl is not down.
  if (event.flags() & ui::EF_CONTROL_DOWN) {
    ui::KeyEvent event_no_ctrl(event.type(), event.key_code(),
                               event.flags() ^ ui::EF_CONTROL_DOWN);
    ch = event_no_ctrl.GetCharacter();
  } else {
    ch = event.GetCharacter();
  }
  return base::UTF16ToUTF8(base::string16(1, ch));
}
#endif  // defined(OS_CHROMEOS)

void GetExtensionKeyboardEventFromKeyEvent(
    const ui::KeyEvent& event,
    InputMethodEngineBase::KeyboardEvent* ext_event) {
  DCHECK(event.type() == ui::ET_KEY_RELEASED ||
         event.type() == ui::ET_KEY_PRESSED);
  DCHECK(ext_event);
  ext_event->type = (event.type() == ui::ET_KEY_RELEASED) ? "keyup" : "keydown";

  if (event.code() == ui::DomCode::NONE) {
// TODO(azurewei): Use KeycodeConverter::DomCodeToCodeString on all platforms
#if defined(OS_CHROMEOS)
    ext_event->code = ui::KeyboardCodeToDomKeycode(event.key_code());
#else
    ext_event->code =
        std::string(ui::KeycodeConverter::DomCodeToCodeString(event.code()));
#endif
  } else {
    ext_event->code = event.GetCodeString();
  }
  ext_event->key_code = static_cast<int>(event.key_code());
  ext_event->alt_key = event.IsAltDown();
  ext_event->altgr_key = event.IsAltGrDown();
  ext_event->ctrl_key = event.IsControlDown();
  ext_event->shift_key = event.IsShiftDown();
  ext_event->caps_lock = event.IsCapsLockOn();
#if defined(OS_CHROMEOS)
  ext_event->key = GetKeyFromEvent(event);
#else
  ext_event->key = ui::KeycodeConverter::DomKeyToKeyString(event.GetDomKey());
#endif  // defined(OS_CHROMEOS)
}

bool IsUint32Value(int i) {
  return 0 <= i && i <= std::numeric_limits<uint32_t>::max();
}

}  // namespace

InputMethodEngineBase::KeyboardEvent::KeyboardEvent() = default;

InputMethodEngineBase::KeyboardEvent::KeyboardEvent(
    const KeyboardEvent& other) = default;

InputMethodEngineBase::KeyboardEvent::~KeyboardEvent() {}

InputMethodEngineBase::InputMethodEngineBase()
    : current_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      context_id_(0),
      next_context_id_(1),
      profile_(nullptr),
      composition_changed_(false),
      text_(""),
      commit_text_changed_(false),
      handling_key_event_(false) {}

InputMethodEngineBase::~InputMethodEngineBase() {}

void InputMethodEngineBase::Initialize(
    std::unique_ptr<InputMethodEngineBase::Observer> observer,
    const char* extension_id,
    Profile* profile) {
  DCHECK(observer) << "Observer must not be null.";

  // TODO(komatsu): It is probably better to set observer out of Initialize.
  observer_ = std::move(observer);
  extension_id_ = extension_id;
  profile_ = profile;
}

void InputMethodEngineBase::FocusIn(
    const ui::IMEEngineHandlerInterface::InputContext& input_context) {
  current_input_type_ = input_context.type;

  if (!IsActive() || current_input_type_ == ui::TEXT_INPUT_TYPE_NONE)
    return;

  context_id_ = next_context_id_;
  ++next_context_id_;

  observer_->OnFocus(ui::IMEEngineHandlerInterface::InputContext(
      context_id_, input_context.type, input_context.mode, input_context.flags,
      input_context.focus_reason, input_context.should_do_learning));
}

void InputMethodEngineBase::FocusOut() {
  if (!IsActive() || current_input_type_ == ui::TEXT_INPUT_TYPE_NONE)
    return;

  current_input_type_ = ui::TEXT_INPUT_TYPE_NONE;

  int context_id = context_id_;
  context_id_ = -1;
  observer_->OnBlur(context_id);
}

void InputMethodEngineBase::Enable(const std::string& component_id) {
  active_component_id_ = component_id;
  observer_->OnActivate(component_id);
  const ui::IMEEngineHandlerInterface::InputContext& input_context =
      ui::IMEBridge::Get()->GetCurrentInputContext();
  current_input_type_ = input_context.type;
  FocusIn(input_context);
}

void InputMethodEngineBase::Disable() {
  std::string last_component_id{active_component_id_};
  active_component_id_.clear();
  ConfirmCompositionText(/* reset_engine */ true, /* keep_selection */ false);
  observer_->OnDeactivated(last_component_id);
}

void InputMethodEngineBase::Reset() {
  observer_->OnReset(active_component_id_);
}

void InputMethodEngineBase::ProcessKeyEvent(const ui::KeyEvent& key_event,
                                            KeyEventDoneCallback callback) {
  // Make true that we don't handle IME API calling of setComposition and
  // commitText while the extension is handling key event.
  handling_key_event_ = true;

  if (key_event.IsCommandDown()) {
    std::move(callback).Run(false);
    return;
  }

  KeyboardEvent ext_event;
  GetExtensionKeyboardEventFromKeyEvent(key_event, &ext_event);

  // If the given key event is from VK, it means the key event was simulated.
  // Sets the |extension_id| value so that the IME extension can ignore it.
  auto* properties = key_event.properties();
  if (properties && properties->find(ui::kPropertyFromVK) != properties->end())
    ext_event.extension_id = extension_id_;

  // Should not pass key event in password field.
  if (current_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD) {
    // Bind the start time to the callback so that we can calculate the latency
    // when the callback is called.
    observer_->OnKeyEvent(
        active_component_id_, ext_event,
        base::BindOnce(
            [](base::Time start, KeyEventDoneCallback callback, bool handled) {
              std::move(callback).Run(handled);
              UMA_HISTOGRAM_TIMES("InputMethod.KeyEventLatency",
                                  base::Time::Now() - start);
            },
            base::Time::Now(), std::move(callback)));
  }
}

void InputMethodEngineBase::SetSurroundingText(const std::string& text,
                                               uint32_t cursor_pos,
                                               uint32_t anchor_pos,
                                               uint32_t offset_pos) {
  observer_->OnSurroundingTextChanged(
      active_component_id_, text, static_cast<int>(cursor_pos),
      static_cast<int>(anchor_pos), static_cast<int>(offset_pos));
}

void InputMethodEngineBase::SetCompositionBounds(
    const std::vector<gfx::Rect>& bounds) {
  composition_bounds_ = bounds;
  observer_->OnCompositionBoundsChanged(bounds);
}

const std::string& InputMethodEngineBase::GetActiveComponentId() const {
  return active_component_id_;
}

bool InputMethodEngineBase::ClearComposition(int context_id,
                                             std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  UpdateComposition(ui::CompositionText(), 0, false);
  return true;
}

bool InputMethodEngineBase::CommitText(int context_id,
                                       const char* text,
                                       std::string* error) {
  if (!IsActive()) {
    // TODO: Commit the text anyways.
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  CommitTextToInputContext(context_id, std::string(text));
  return true;
}

bool InputMethodEngineBase::FinishComposingText(int context_id,
                                                std::string* error) {
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }
  ConfirmCompositionText(/* reset_engine */ false, /* keep_selection */ true);
  return true;
}

bool InputMethodEngineBase::DeleteSurroundingText(int context_id,
                                                  int offset,
                                                  size_t number_of_chars,
                                                  std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  // TODO(nona): Return false if there is ongoing composition.

  DeleteSurroundingTextToInputContext(offset, number_of_chars);

  return true;
}

bool InputMethodEngineBase::SendKeyEvents(
    int context_id,
    const std::vector<KeyboardEvent>& events) {
  // context_id  ==  0, means sending key events to non-input field.
  // context_id_ == -1, means the focus is not in an input field.
  if (!IsActive() ||
      (context_id != 0 && (context_id != context_id_ || context_id_ == -1)))
    return false;

  for (size_t i = 0; i < events.size(); ++i) {
    const KeyboardEvent& event = events[i];
    const ui::EventType type =
        (event.type == "keyup") ? ui::ET_KEY_RELEASED : ui::ET_KEY_PRESSED;
    ui::KeyboardCode key_code = static_cast<ui::KeyboardCode>(event.key_code);

    int flags = ui::EF_NONE;
    flags |= event.alt_key ? ui::EF_ALT_DOWN : ui::EF_NONE;
    flags |= event.altgr_key ? ui::EF_ALTGR_DOWN : ui::EF_NONE;
    flags |= event.ctrl_key ? ui::EF_CONTROL_DOWN : ui::EF_NONE;
    flags |= event.shift_key ? ui::EF_SHIFT_DOWN : ui::EF_NONE;
    flags |= event.caps_lock ? ui::EF_CAPS_LOCK_ON : ui::EF_NONE;

    ui::KeyEvent ui_event(
        type, key_code, ui::KeycodeConverter::CodeStringToDomCode(event.code),
        flags, ui::KeycodeConverter::KeyStringToDomKey(event.key),
        ui::EventTimeForNow());
    if (!SendKeyEvent(&ui_event, event.code))
      return false;
  }
  return true;
}

bool InputMethodEngineBase::SetComposition(
    int context_id,
    const char* text,
    int selection_start,
    int selection_end,
    int cursor,
    const std::vector<SegmentInfo>& segments,
    std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  ui::CompositionText composition_text;
  composition_text.text = base::UTF8ToUTF16(text);
  composition_text.selection.set_start(selection_start);
  composition_text.selection.set_end(selection_end);

  // TODO: Add support for displaying selected text in the composition string.
  for (auto segment = segments.begin(); segment != segments.end(); ++segment) {
    ui::ImeTextSpan ime_text_span;

    ime_text_span.underline_color = SK_ColorTRANSPARENT;
    switch (segment->style) {
      case SEGMENT_STYLE_UNDERLINE:
        ime_text_span.thickness = ui::ImeTextSpan::Thickness::kThin;
        break;
      case SEGMENT_STYLE_DOUBLE_UNDERLINE:
        ime_text_span.thickness = ui::ImeTextSpan::Thickness::kThick;
        break;
      case SEGMENT_STYLE_NO_UNDERLINE:
        ime_text_span.thickness = ui::ImeTextSpan::Thickness::kNone;
        break;
      default:
        continue;
    }

    ime_text_span.start_offset = segment->start;
    ime_text_span.end_offset = segment->end;
    composition_text.ime_text_spans.push_back(ime_text_span);
  }

  // TODO(nona): Makes focus out mode configuable, if necessary.
  UpdateComposition(composition_text, cursor, true);
  return true;
}

bool InputMethodEngineBase::SetCompositionRange(
    int context_id,
    int selection_before,
    int selection_after,
    const std::vector<SegmentInfo>& segments,
    std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  // When there is composition text, commit it to the text field first before
  // changing the composition range.
  ConfirmCompositionText(/* reset_engine */ false, /* keep_selection */ false);

  std::vector<ui::ImeTextSpan> text_spans;
  for (const auto& segment : segments) {
    ui::ImeTextSpan text_span;

    text_span.underline_color = SK_ColorTRANSPARENT;
    switch (segment.style) {
      case SEGMENT_STYLE_UNDERLINE:
        text_span.thickness = ui::ImeTextSpan::Thickness::kThin;
        break;
      case SEGMENT_STYLE_DOUBLE_UNDERLINE:
        text_span.thickness = ui::ImeTextSpan::Thickness::kThick;
        break;
      case SEGMENT_STYLE_NO_UNDERLINE:
        text_span.thickness = ui::ImeTextSpan::Thickness::kNone;
        break;
    }

    text_span.start_offset = segment.start;
    text_span.end_offset = segment.end;
    text_spans.push_back(text_span);
  }
  if (!IsUint32Value(selection_before)) {
    *error = base::StringPrintf(kErrorInvalidValue, "selection_before",
                                selection_before);
    return false;
  }
  if (!IsUint32Value(selection_after)) {
    *error = base::StringPrintf(kErrorInvalidValue, "selection_after",
                                selection_after);
    return false;
  }
  return SetCompositionRange(static_cast<uint32_t>(selection_before),
                             static_cast<uint32_t>(selection_after),
                             text_spans);
}

bool InputMethodEngineBase::SetSelectionRange(int context_id,
                                              int start,
                                              int end,
                                              std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }
  if (!IsUint32Value(start)) {
    *error = base::StringPrintf(kErrorInvalidValue, "start", start);
    return false;
  }
  if (!IsUint32Value(end)) {
    *error = base::StringPrintf(kErrorInvalidValue, "end", end);
    return false;
  }

  return SetSelectionRange(static_cast<uint32_t>(start),
                           static_cast<uint32_t>(end));
}

void InputMethodEngineBase::KeyEventHandled(const std::string& extension_id,
                                            const std::string& request_id,
                                            bool handled) {
  handling_key_event_ = false;
  // When finish handling key event, take care of the unprocessed commitText
  // and setComposition calls.
  if (commit_text_changed_) {
    CommitTextToInputContext(context_id_, text_);
    text_ = "";
    commit_text_changed_ = false;
  }

  if (composition_changed_) {
    UpdateComposition(composition_, composition_.selection.start(), true);
    composition_ = ui::CompositionText();
    composition_changed_ = false;
  }

  const auto it = pending_key_events_.find(request_id);
  if (it == pending_key_events_.end()) {
    LOG(ERROR) << "Request ID not found: " << request_id;
    return;
  }

  std::move(it->second.callback).Run(handled);
  pending_key_events_.erase(it);
}

std::string InputMethodEngineBase::AddPendingKeyEvent(
    const std::string& component_id,
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) {
  std::string request_id = base::NumberToString(next_request_id_);
  ++next_request_id_;

  pending_key_events_.emplace(
      request_id, PendingKeyEvent(component_id, std::move(callback)));

  return request_id;
}

InputMethodEngineBase::PendingKeyEvent::PendingKeyEvent(
    const std::string& component_id,
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback)
    : component_id(component_id), callback(std::move(callback)) {}

InputMethodEngineBase::PendingKeyEvent::PendingKeyEvent(
    PendingKeyEvent&& other) = default;

InputMethodEngineBase::PendingKeyEvent::~PendingKeyEvent() = default;

void InputMethodEngineBase::DeleteSurroundingTextToInputContext(
    int offset,
    size_t number_of_chars) {
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (input_context)
    input_context->DeleteSurroundingText(offset, number_of_chars);
}

void InputMethodEngineBase::ConfirmCompositionText(bool reset_engine,
                                                   bool keep_selection) {
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (input_context)
    input_context->ConfirmCompositionText(reset_engine, keep_selection);
}

}  // namespace input_method
