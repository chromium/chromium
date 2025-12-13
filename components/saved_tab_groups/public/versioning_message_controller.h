// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_VERSIONING_MESSAGE_CONTROLLER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_VERSIONING_MESSAGE_CONTROLLER_H_

#include "base/functional/callback_forward.h"

namespace tab_groups {

// The central controller for versioning messages. Determines which versioning
// messages should be shown based on whether the current chrome client is
// able to support shared tab groups. The UI layer should query this class
// before showing any versioning messages. Owned by TabGroupSyncService.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.tab_group_sync
class VersioningMessageController {
 public:
  // Enum representing the different types of messages that needs to be
  // displayed to the user.
  enum class MessageType {
    // Instant message displayed to prompt the user to update chrome to be able
    // to continue using their shared tab groups. Shown one time only on
    // startup.
    // Invoke OnMessageUiShown after displaying the message.
    VERSION_OUT_OF_DATE_INSTANT_MESSAGE,
    // Persistent message displayed to prompt the user to update chrome to be
    // able to continue using their shared tab groups. Continues to be shown on
    // every session until user explicitly dismisses it.
    // Invoke OnMessageUiShown after displaying the message.
    // Invoke OnMessageUiDismissed if the user dismisses the message.
    VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE,
    // IPH displayed to notify the user that shared tab groups are available
    // again. Shown one time only.
    // Invoke OnMessageUiShown after displaying the message.
    VERSION_UPDATED_MESSAGE
  };

  virtual ~VersioningMessageController() = default;

  // Whether this VersioningMessageController is initialized.
  // Internally, this just returns whether the TabGroupSyncService is itself
  // initialized.
  virtual bool IsInitialized() = 0;

  // Invoke this method to query if the given message UI should be shown. This
  // should not be called if the TabGroupSyncService is not initialized.
  // See comments on MessageType for when the UI should inform this class about
  // display / dismissed events.
  virtual bool ShouldShowMessageUi(MessageType message_type) = 0;

  // Same as ShouldShowMessageUi but waits on the initialization before calling
  // the callback. If the TabGroupSyncService is already initialized, then the
  // callback is called synchronously.
  // See comments on MessageType for when the UI should inform this class about
  // display / dismissed events.
  //
  // NOTE: For MessageType::VERSION_UPDATED_MESSAGE, this method has a special
  // behavior. The callback may be deferred until a shared tab group has been
  // downloaded. If no shared tab group is ever added during the session, the
  // callback may not be invoked at all.
  virtual void ShouldShowMessageUiAsync(
      MessageType message_type,
      base::OnceCallback<void(bool)> callback) = 0;

  // Invoke this after a MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE or
  // MessageType::VERSION_UPDATED_MESSAGE has been displayed.
  virtual void OnMessageUiShown(MessageType message_type) = 0;

  // Invoke this after a user dismisses a
  // MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE.
  virtual void OnMessageUiDismissed(MessageType message_type) = 0;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_VERSIONING_MESSAGE_CONTROLLER_H_
