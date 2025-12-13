// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/key_dispatcher.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/type_tool.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/latency/latency_info.h"

namespace actor {

using ::blink::WebCoalescedInputEvent;
using ::blink::WebInputEvent;
using ::blink::WebInputEventResult;
using ::blink::WebKeyboardEvent;
using ::blink::WebWidget;

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

KeyDispatcher::KeyParams::KeyParams() = default;
KeyDispatcher::KeyParams::~KeyParams() = default;
KeyDispatcher::KeyParams::KeyParams(const KeyParams& other) = default;

KeyDispatcher::KeyDispatcher(std::vector<KeyParams> key_sequence,
                             mojom::TypeActionPtr action,
                             const ResolvedTarget& resolved_target,
                             const TypeTool& type_tool,
                             ToolBase::ToolFinishedCallback on_complete,
                             TaskId task_id,
                             Journal& journal)
    : key_sequence_(std::move(key_sequence)),
      action_(std::move(action)),
      resolved_target_(resolved_target),
      type_tool_(type_tool),
      on_complete_(std::move(on_complete)),
      task_id_(task_id),
      journal_(journal) {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&KeyDispatcher::ContinueIncrementalTyping,
                     weak_ptr_factory_.GetWeakPtr()),
      features::kGlicActorKeyUpDuration.Get());
}

KeyDispatcher::~KeyDispatcher() = default;

void KeyDispatcher::Cancel() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  on_complete_.Reset();

  if (is_key_down_) {
    // A key is currently held down. We need to release it to avoid a stuck key.
    WebWidget* widget = resolved_target_->GetWidget(*type_tool_);
    if (widget) {
      const KeyDispatcher::KeyParams& params = key_sequence_[current_key_];
      WebInputEventResult up_result = CreateAndDispatchKeyEvent(
          *widget, WebInputEvent::Type::kKeyUp, params);
      if (up_result == WebInputEventResult::kHandledSuppressed) {
        ACTOR_LOG() << "Warning: KeyUp event for key " << params.dom_key
                    << " suppressed during cancelation.";
      }
    }
  }
  is_key_down_ = false;

  Finish(MakeResult(mojom::ActionResultCode::kToolTimeout));
}

void KeyDispatcher::ContinueIncrementalTyping() {
  CHECK(on_complete_);
  const KeyParams& params = key_sequence_[current_key_];

  WebWidget* widget = resolved_target_->GetWidget(*type_tool_);
  if (!widget) {
    Finish(MakeResult(mojom::ActionResultCode::kFrameWentAway,
                      /*requires_page_stabilization=*/true,
                      "No widget during incremental typing"));
    return;
  }

  if (!is_key_down_) {
    WebInputEventResult down_result = CreateAndDispatchKeyEvent(
        *widget, WebInputEvent::Type::kRawKeyDown, params);

    // Only the KeyDown event will check for and report failure. The reason the
    // other events don't is that if the KeyDown event was dispatched to the
    // page, the key input was observable to the page and it may mutate itself
    // in a way that subsequent Char and KeyUp events are suppressed (e.g.
    // mutating the DOM tree, removing frames, etc). These "failure" cases can
    // be considered successful in terms that the tool has acted on the page. In
    // particular, a preventDefault()'ed KeyDown event will force suppressing
    // the following Char event but this is expected and common.
    if (down_result == WebInputEventResult::kHandledSuppressed) {
      Finish(
          MakeResult(mojom::ActionResultCode::kTypeKeyDownSuppressed,
                     /*requires_page_stabilization=*/true,
                     absl::StrFormat("Suppressed char[%s]", params.dom_key)));
      return;
    }

    // Input handling could destroy the widget so it needs to be re-read.
    widget = resolved_target_->GetWidget(*type_tool_);
    if (!widget) {
      Finish(MakeResult(mojom::ActionResultCode::kFrameWentAway,
                        /*requires_page_stabilization=*/true,
                        "No widget during incremental typing"));
      return;
    }
    if (params.dom_key != "Dead") {
      WebInputEventResult char_result = CreateAndDispatchKeyEvent(
          *widget, WebInputEvent::Type::kChar, params);
      if (char_result == WebInputEventResult::kHandledSuppressed) {
        ACTOR_LOG() << "Warning: Char event for key " << params.dom_key
                    << " suppressed.";
      }
    }

    is_key_down_ = true;
  } else {
    WebInputEventResult up_result =
        CreateAndDispatchKeyEvent(*widget, WebInputEvent::Type::kKeyUp, params);
    if (up_result == WebInputEventResult::kHandledSuppressed) {
      ACTOR_LOG() << "Warning: KeyUp event for key " << params.dom_key
                  << " suppressed.";
    }

    is_key_down_ = false;
    current_key_++;
  }

  if (current_key_ >= key_sequence_.size()) {
    Finish(MakeOkResult());
  } else {
    bool is_final_enter_key_down = action_->follow_by_enter &&
                                   current_key_ == key_sequence_.size() - 1 &&
                                   !is_key_down_;
    DCHECK(!is_final_enter_key_down || key_sequence_[current_key_].dom_code ==
                                           GetEnterKeyParams().dom_code);

    base::TimeDelta delay;

    if (is_final_enter_key_down) {
      // If the next key is the final enter key, it has a specific delay to
      // ensure a user-like input and to allow the page to process the typed
      // text. Only down is delayed to avoid doubling this longer delay and
      // since most inputs take action on the down event.
      delay = features::kGlicActorTypeToolEnterDelay.Get();
    } else {
      delay = (is_key_down_ ? features::kGlicActorKeyDownDuration
                            : features::kGlicActorKeyUpDuration)
                  .Get();

      // Apply a speed boost when typing a long string.
      if (action_->text.length() >
          features::kGlicActorIncrementalTypingLongTextThreshold.Get()) {
        delay *= features::kGlicActorIncrementalTypingLongMultiplier.Get();
      }
    }

    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&KeyDispatcher::ContinueIncrementalTyping,
                       weak_ptr_factory_.GetWeakPtr()),
        delay);
  }
}

void KeyDispatcher::Finish(mojom::ActionResultPtr result) {
  if (!on_complete_) {
    return;
  }
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_complete_), std::move(result)));
  // This instance may now be deleted once the callback runs.
}

WebInputEventResult KeyDispatcher::CreateAndDispatchKeyEvent(
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

KeyDispatcher::KeyParams KeyDispatcher::GetEnterKeyParams() const {
  KeyParams params;
  params.windows_key_code = ui::VKEY_RETURN;
  params.native_key_code =
      ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::ENTER);
  params.dom_code = "Enter";
  params.dom_key = "Enter";
  params.text = ui::VKEY_RETURN;
  params.unmodified_text = ui::VKEY_RETURN;
  return params;
}

}  // namespace actor
