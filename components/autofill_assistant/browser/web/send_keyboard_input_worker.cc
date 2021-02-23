// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/send_keyboard_input_worker.h"

#include "base/logging.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace autofill_assistant {

namespace {
std::unique_ptr<input::DispatchKeyEventParams> CreateKeyEventParamsForCharacter(
    input::DispatchKeyEventType type,
    base::Optional<base::Time> timestamp,
    UChar32 codepoint) {
  auto params = input::DispatchKeyEventParams::Builder().SetType(type).Build();
  if (timestamp) {
    params->SetTimestamp(timestamp->ToDoubleT());
  }

  std::string text;
  if (AppendUnicodeToUTF8(codepoint, &text)) {
    params->SetText(text);
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
    params->SetKey(ui::KeycodeConverter::DomKeyToKeyString(dom_key));
  } else {
#ifdef NDEBUG
    VLOG(1) << __func__ << ": Failed to set DomKey for codepoint";
#else
    DVLOG(1) << __func__
             << ": Failed to set DomKey for codepoint: " << codepoint;
#endif
  }

  return params;
}
}  // namespace

SendKeyboardInputWorker::SendKeyboardInputWorker(
    DevtoolsClient* devtools_client)
    : devtools_client_(devtools_client) {}

SendKeyboardInputWorker::~SendKeyboardInputWorker() = default;

void SendKeyboardInputWorker::Start(const std::string& frame_id,
                                    const std::vector<UChar32>& codepoints,
                                    int key_press_delay_in_millisecond,
                                    Callback callback) {
  DCHECK(!callback_);

  if (codepoints.empty()) {
    std::move(callback).Run(OkClientStatus());
    return;
  }

  callback_ = std::move(callback);
  key_press_delay_ =
      base::TimeDelta::FromMilliseconds(key_press_delay_in_millisecond);
  frame_id_ = frame_id;
  codepoints_ = codepoints;

  if (VLOG_IS_ON(3)) {
    std::string input_str;
    if (!UnicodeToUTF8(codepoints_, &input_str)) {
      input_str.assign("<invalid input>");
    }
#ifdef NDEBUG
    VLOG(3) << __func__ << " input=(redacted)"
            << " delay=" << key_press_delay_;
#else
    DVLOG(3) << __func__ << " input=" << input_str
             << " delay=" << key_press_delay_;
#endif
  }

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
  pending_key_events_ = 2 * codepoints.size();
  base::Time base_ts = base::Time::Now() -
                       base::TimeDelta::FromMilliseconds(2 * codepoints.size());

  auto* devtools_input = devtools_client_->GetInput();
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  // Don't access fields after this point, as callbacks might destroy this at
  // any time.

  for (size_t i = 0; i < codepoints.size(); i++) {
    base::Time keydown_ts = base_ts + base::TimeDelta::FromMilliseconds(2 * i);
    base::Time keyup_ts =
        base_ts + base::TimeDelta::FromMilliseconds(2 * i + 1);
    devtools_input->DispatchKeyEvent(
        CreateKeyEventParamsForCharacter(input::DispatchKeyEventType::KEY_DOWN,
                                         keydown_ts, codepoints[i]),
        frame_id,
        base::BindOnce(&SendKeyboardInputWorker::OnKeyEventDone, weak_ptr));
    devtools_input->DispatchKeyEvent(
        CreateKeyEventParamsForCharacter(input::DispatchKeyEventType::KEY_UP,
                                         keyup_ts, codepoints[i]),
        frame_id,
        base::BindOnce(&SendKeyboardInputWorker::OnKeyEventDone, weak_ptr));
  }
}

void SendKeyboardInputWorker::DispatchKeyboardTextDownEvent(size_t index) {
  DCHECK_LT(index, codepoints_.size());
  devtools_client_->GetInput()->DispatchKeyEvent(
      CreateKeyEventParamsForCharacter(input::DispatchKeyEventType::KEY_DOWN,
                                       base::nullopt, codepoints_[index]),
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

  DCHECK_LT(index, codepoints_.size());
  devtools_client_->GetInput()->DispatchKeyEvent(
      CreateKeyEventParamsForCharacter(input::DispatchKeyEventType::KEY_UP,
                                       base::nullopt, codepoints_[index]),
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

  if (index >= codepoints_.size()) {
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
