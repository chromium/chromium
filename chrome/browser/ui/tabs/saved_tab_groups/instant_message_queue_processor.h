// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_INSTANT_MESSAGE_QUEUE_PROCESSOR_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_INSTANT_MESSAGE_QUEUE_PROCESSOR_H_

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"

class Browser;

namespace tab_groups {

using collaboration::messaging::InstantMessage;
using collaboration::messaging::MessagingBackendService;

using SuccessCallback =
    MessagingBackendService::InstantMessageDelegate::SuccessCallback;

// Struct used to capture the instant message and its associated callback.
struct QueuedInstantMessage {
  QueuedInstantMessage(InstantMessage message_,
                       SuccessCallback success_callback_);
  QueuedInstantMessage(QueuedInstantMessage&& other);
  ~QueuedInstantMessage();

  InstantMessage message;
  SuccessCallback success_callback;
};

// This class acts as a buffer for InstantMessages from the
// MessagingBackendService. These messages are delivered through
// a delegate to be shown in the ToastController. Toasts currently cannot
// be queued, so this queue processor maintains the list of messages. As
// soon as a message is queued, the processor enters the processing loop
// where a single message is shown at a time, waiting for some interval
// before attempting to show the next message.
//
// A single instance of this class is meant to be owned by the
// InstantMessageDelegate for the profile.
//
// A drawback to this class is that the toast controller does not provide
// an observer, so the processor cannot know if a message was dismissed
// early without polling the toast state.
class InstantMessageQueueProcessor {
 public:
  explicit InstantMessageQueueProcessor(Profile* profile);
  virtual ~InstantMessageQueueProcessor();

  // Queue a message to be shown in a toast.
  void Enqueue(InstantMessage message, SuccessCallback success_callback);

  // Returns a reference to the head of the message queue. Must check
  // that the queue is non-empty before calling.
  const InstantMessage& GetCurrentMessage();

  // Returns whether a message is currently being shown.
  bool IsMessageShowing();

  // Returns the size of the queue, including the currently showing item.
  int GetQueueSize();

  // Returns the time to wait between showing messages.
  base::TimeDelta GetMessageInterval();

 protected:
  // Triggers showing a toast for the head of the message queue, as long
  // as the browser is in a state sufficient to show the given message.
  void MaybeShowInstantMessage();

  // Responsible for resetting the |is_showing_instant_message_| state.
  // Called following a timeout after a message has been successfully shown.
  void ProcessQueueAfterMessageShown();

  // Removes the head of the queue and triggers MaybeShowInstantMessage
  // for the new message queue head.
  void ProceedToNextQueueMessage();

  // Finds the toast controller needed to show a message for the group
  // contained in this message.
  Browser* GetBrowser(const InstantMessage& message);

  // Finds the toast controller needed to show a message for the group
  // contained in this message. Virtual for testing purposes only.
  virtual bool MaybeShowToastInBrowser(Browser* browser,
                                       std::optional<ToastParams> params);

  // Returns the toast params populated for this message. Returns
  // std::nullopt if this message cannot be converted into a valid
  // toast. Virtual for tests.
  std::optional<ToastParams> GetParamsForMessage(const InstantMessage& message);

  raw_ptr<Profile> profile_;

  // Used to track if an instant message is being shown in a toast.
  bool is_showing_instant_message_ = false;

  // Queue of instant messages remaining to be shown.
  base::queue<QueuedInstantMessage> instant_message_queue_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_INSTANT_MESSAGE_QUEUE_PROCESSOR_H_
