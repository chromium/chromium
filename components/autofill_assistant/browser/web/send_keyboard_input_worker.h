// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SEND_KEYBOARD_INPUT_WORKER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SEND_KEYBOARD_INPUT_WORKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"
#include "third_party/icu/source/common/unicode/umachine.h"

namespace autofill_assistant {

// Worker class for sending keypress events.
class SendKeyboardInputWorker : public WebControllerWorker {
 public:
  // |devtools_client| must be valid for the lifetime of the instance.
  SendKeyboardInputWorker(DevtoolsClient* devtools_client);
  ~SendKeyboardInputWorker() override;

  // Callback called when the worker is done.
  using Callback = base::OnceCallback<void(const ClientStatus&)>;

  // Create a |KeyEvent| from the |codepoint|. This has the potential to fill
  // |text|, |key| and |commands|.
  static KeyEvent KeyEventFromCodepoint(UChar32 codepoint);

  // Send |key_events| to |frame_id|, waiting for
  // |key_press_delay_in_millisecond| between key presses.
  void Start(const std::string& frame_id,
             const std::vector<KeyEvent>& key_events,
             int key_press_delay_in_millisecond,
             Callback callback);

 private:
  // Send key events in sequence, waiting for |delay_in_millisecond| between
  // keys.
  void DispatchKeyboardTextDownEvent(size_t index);
  void DispatchKeyboardTextUpEvent(
      size_t index,
      const MessageDispatcher::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchKeyEventResult> result);
  void WaitBeforeNextKey(size_t index,
                         const MessageDispatcher::ReplyStatus& reply_status,
                         std::unique_ptr<input::DispatchKeyEventResult> result);

  // Calls |callback_| once |pending_key_events_| is 0. This is only used when
  // sending key events is parallel.
  void OnKeyEventDone(const MessageDispatcher::ReplyStatus& reply_status,
                      std::unique_ptr<input::DispatchKeyEventResult> result);

  const raw_ptr<DevtoolsClient> devtools_client_;
  Callback callback_;
  base::TimeDelta key_press_delay_;
  std::string frame_id_;
  std::vector<KeyEvent> key_events_;

  // When sending key events in parallel, this contains the number of
  // events for which we're still waiting for an answer.
  size_t pending_key_events_ = 0;

  base::WeakPtrFactory<SendKeyboardInputWorker> weak_ptr_factory_{this};
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SEND_KEYBOARD_INPUT_WORKER_H_
