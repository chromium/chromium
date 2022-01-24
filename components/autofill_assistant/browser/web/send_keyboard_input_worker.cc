// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/send_keyboard_input_worker.h"

#include "base/logging.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/web/keyboard_input_data.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace autofill_assistant {
namespace {

std::unique_ptr<input::DispatchKeyEventParams> CreateKeyEventParamsForKeyEvent(
    input::DispatchKeyEventType type,
    absl::optional<base::Time> timestamp,
    const KeyEvent& key_event) {
  auto params = input::DispatchKeyEventParams::Builder().SetType(type).Build();
  if (timestamp) {
    params->SetTimestamp(timestamp->ToDoubleT());
  }

  if (key_event.has_code()) {
    params->SetCode(key_event.code());
  }
  if (key_event.has_text()) {
    params->SetText(key_event.text());
  }
  if (key_event.has_key()) {
    params->SetKey(key_event.key());
  }
  if (!key_event.command().empty()) {
    params->SetCommands(std::vector<std::string>(key_event.command().begin(),
                                                 key_event.command().end()));
  }
  if (key_event.has_key_code()) {
    // Set legacy keyCode for the KeyEvent as described here:
    // https://w3c.github.io/uievents/#dom-keyboardevent-keycode
    params->SetWindowsVirtualKeyCode(key_event.key_code());
  }

  return params;
}

}  // namespace

SendKeyboardInputWorker::SendKeyboardInputWorker(
    DevtoolsClient* devtools_client)
    : devtools_client_(devtools_client) {}

SendKeyboardInputWorker::~SendKeyboardInputWorker() = default;

// static
KeyEvent SendKeyboardInputWorker::KeyEventFromCodepoint(UChar32 codepoint) {
  KeyEvent key_event;

  std::string text;
  if (AppendUnicodeToUTF8(codepoint, &text)) {
    key_event.set_text(text);
  } else {
#ifdef NDEBUG
    VLOG(1) << __func__ << ": Failed to convert codepoint to UTF-8";
#else
    DVLOG(1) << __func__
             << ": Failed to convert codepoint to UTF-8: " << codepoint;
#endif
  }

  auto dom_key = ui::DomKey::FromCharacter(codepoint);
  if (dom_key.IsValid()) {
    key_event.set_key(ui::KeycodeConverter::DomKeyToKeyString(dom_key));
  } else {
#ifdef NDEBUG
    VLOG(1) << __func__ << ": Failed to set DomKey for codepoint";
#else
    DVLOG(1) << __func__
             << ": Failed to set DomKey for codepoint: " << codepoint;
#endif
  }

  auto key_info =
      keyboard_input_data::GetDevToolsDispatchKeyEventParamsForCodepoint(
          codepoint);
  key_event.set_key_code(static_cast<int32_t>(key_info.key_code));
  if (!key_info.command.empty()) {
    key_event.add_command(key_info.command);
  }

  return key_event;
}

void SendKeyboardInputWorker::Start(const std::string& frame_id,
                                    const std::vector<KeyEvent>& key_events,
                                    int key_press_delay_in_millisecond,
                                    Callback callback) {
  DCHECK(!callback_);

  if (key_events.empty()) {
    std::move(callback).Run(OkClientStatus());
    return;
  }

  callback_ = std::move(callback);
  key_press_delay_ = base::Milliseconds(key_press_delay_in_millisecond);
  frame_id_ = frame_id;
  key_events_ = key_events;

  if (key_press_delay_in_millisecond > 0) {
    // Send events one after the others, waiting for
    // key_press_delay_in_millisecond in between.
    DispatchKeyboardTextDownEvent(0);
    return;
  }

  // Since all events are sent at the same time, devtools might assign them all
  // the same timestamp. To avoid running into trouble if some intermediate code
  // sort events by timestamp, we assign a unique,increasing timestamp to each
  // key events.
  pending_key_events_ = 2 * key_events.size();
  base::Time base_ts =
      base::Time::Now() - base::Milliseconds(2 * key_events.size());

  auto* devtools_input = devtools_client_->GetInput();
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  // Don't access fields after this point, as callbacks might destroy this at
  // any time.

  for (size_t i = 0; i < key_events.size(); i++) {
    base::Time keydown_ts = base_ts + base::Milliseconds(2 * i);
    base::Time keyup_ts = base_ts + base::Milliseconds(2 * i + 1);
    devtools_input->DispatchKeyEvent(
        CreateKeyEventParamsForKeyEvent(input::DispatchKeyEventType::KEY_DOWN,
                                        keydown_ts, key_events[i]),
        frame_id,
        base::BindOnce(&SendKeyboardInputWorker::OnKeyEventDone, weak_ptr));
    devtools_input->DispatchKeyEvent(
        CreateKeyEventParamsForKeyEvent(input::DispatchKeyEventType::KEY_UP,
                                        keyup_ts, key_events[i]),
        frame_id,
        base::BindOnce(&SendKeyboardInputWorker::OnKeyEventDone, weak_ptr));
  }
}

void SendKeyboardInputWorker::DispatchKeyboardTextDownEvent(size_t index) {
  DCHECK_LT(index, key_events_.size());
  devtools_client_->GetInput()->DispatchKeyEvent(
      CreateKeyEventParamsForKeyEvent(input::DispatchKeyEventType::KEY_DOWN,
                                      absl::nullopt, key_events_[index]),
      frame_id_,
      base::BindOnce(&SendKeyboardInputWorker::DispatchKeyboardTextUpEvent,
                     weak_ptr_factory_.GetWeakPtr(), index));
}

void SendKeyboardInputWorker::DispatchKeyboardTextUpEvent(
    size_t index,
    const MessageDispatcher::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchKeyEventResult> result) {
  if (!callback_)
    return;

  if (!result) {
    VLOG(1) << __func__ << " Failed to dispatch key down event.";
    std::move(callback_).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  DCHECK_LT(index, key_events_.size());
  devtools_client_->GetInput()->DispatchKeyEvent(
      CreateKeyEventParamsForKeyEvent(input::DispatchKeyEventType::KEY_UP,
                                      absl::nullopt, key_events_[index]),
      frame_id_,
      base::BindOnce(&SendKeyboardInputWorker::WaitBeforeNextKey,
                     weak_ptr_factory_.GetWeakPtr(), index + 1));
}

void SendKeyboardInputWorker::WaitBeforeNextKey(
    size_t index,
    const MessageDispatcher::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchKeyEventResult> result) {
  if (!callback_)
    return;

  if (!result) {
    VLOG(1) << __func__ << " Failed to dispatch key up event.";
    std::move(callback_).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  if (index >= key_events_.size()) {
    std::move(callback_).Run(OkClientStatus());
    return;
  }

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SendKeyboardInputWorker::DispatchKeyboardTextDownEvent,
                     weak_ptr_factory_.GetWeakPtr(), index),
      key_press_delay_);
}

void SendKeyboardInputWorker::OnKeyEventDone(
    const MessageDispatcher::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchKeyEventResult> result) {
  if (!callback_)
    return;

  if (!result) {
    VLOG(1) << __func__ << " Failed to dispatch key event.";
    std::move(callback_).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  DCHECK_GT(pending_key_events_, 0ul);
  --pending_key_events_;
  if (pending_key_events_ == 0) {
    std::move(callback_).Run(OkClientStatus());
  }
}

}  // namespace autofill_assistant
