// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_PRODUCT_MESSAGING_PRODUCT_MESSAGING_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_PRODUCT_MESSAGING_PRODUCT_MESSAGING_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/user_education/common/session/user_education_session_manager.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/product_messaging/product_messaging_types.h"
#include "ui/base/identifier/unique_identifier.h"

namespace user_education {

class ProductMessagingController;
class ProductMessagingPolicy;

// The owner of this object currently has priority to show a product message
// It must be held while the message is showing and released immediately after
// the message is dismissed.
class ProductMessagingHandleImpl final {
 public:
  ProductMessagingHandleImpl(const ProductMessagingHandleImpl&) = delete;
  ProductMessagingHandleImpl& operator=(const ProductMessagingHandleImpl&) =
      delete;
  ~ProductMessagingHandleImpl();

  ProductMessageKey message_key() const { return message_key_; }

  // Set that the message was actually shown. Cannot be called on a null handle
  // or after releasing. Call to specify that the given message was actually
  // shown; if you discard or release the handle without calling this function,
  // it is assumed that the message was not shown.
  void SetShown();

  // Set the callback that will be called if another message of strictly higher
  // priority becomes eligible or shown. This should be set right away when the
  // handle is received; it has no effect if the handle is not valid, and the
  // callback will be unregistered if this object is released.
  void SetSupersededCallback(ProductMessageStatusCallback callback);

 private:
  friend class ProductMessagingController;
  ProductMessagingHandleImpl(
      ProductMessageKey message_key,
      base::WeakPtr<ProductMessagingController> controller);

  void OnStatusChange(ProductMessageKey key, ProductMessageStatus status);

  bool shown_ = false;
  ProductMessageKey message_key_;
  ProductMessageStatusCallback superseded_callback_;
  base::CallbackListSubscription superseded_subscription_;
  base::WeakPtr<ProductMessagingController> controller_;
};

using ProductMessagingHandle = std::unique_ptr<ProductMessagingHandleImpl>;

// Callback when a required message is ready to show. The message should show
// immediately.
//
// `handle` should be moved to a semi-permanent location and released when the
// message is dismissed/closes. Failure to hold or release the handle can cause
// problems with User Education and other required messages.
using ProductMessageReadyCallback =
    base::OnceCallback<void(ProductMessagingHandle handle)>;

// Coordinates between different product messaging systems.
class ProductMessagingController final {
 public:
  ProductMessagingController();
  ProductMessagingController(const ProductMessagingController&) = delete;
  void operator=(const ProductMessagingController&) = delete;
  ~ProductMessagingController();

  // Register the session provider which is used to clear the set of shown
  // messages and the storage service used to retrieve shown promos.
  void Init(UserEducationSessionProvider& session_provider,
            UserEducationStorageService& storage_service,
            std::unique_ptr<ProductMessagingPolicy> policy);

  // Requests that `message_key` be queued to show. When it is allowed (which
  // might be as soon as the current message queue empties),
  // `ready_to_start_callback` will be called.
  //
  // Similarly, re-queueing a message that is already showing or has been
  // successfully shown will have no effect, and `ready_to_start_callback` will
  // not be called (but see below).
  //
  // If `timeout` is specified, the message will fall out of the queue after
  // that time has elapsed. If a new message with the same key is queued, the
  // one with a timeout further in the future will be the one that ends up in
  // the queue.
  void QueueMessage(ProductMessageKey message_key,
                    ProductMessageReadyCallback ready_to_start_callback,
                    std::optional<base::TimeDelta> timeout = std::nullopt);

  // Removes `message_id` from the queue, if it is queued.
  // Has no effect if the message has already started to show.
  void UnqueueMessage(ProductMessageKey message_key);

  // Returns the status of `message`.
  ProductMessageStatus GetMessageStatus(ProductMessageKey message) const;

  // Returns waiting, ready, or showing messages. Can be filtered by priority
  // and by status.
  base::flat_map<ProductMessageKey, ProductMessageStatus> GetAllMessages(
      ProductMessageStatusSet statuses_to_retrieve =
          {ProductMessageStatus::kWaiting, ProductMessageStatus::kReady,
           ProductMessageStatus::kShowing},
      ProductMessageType priority_higher_than =
          ProductMessageType::kNone) const;

  // Adds a callback that will be called whenever the status of a message
  // changes.
  base::CallbackListSubscription AddStatusUpdateCallbackForTesting(
      ProductMessageStatusCallback callback);

  // Returns the remaining time for `message_key` in the queue, or `nullopt` if
  // no expiry. Fails if the message is not queued. May be negative if the entry
  // is expired but has not been purged, indicating it is overdue to expire.
  std::optional<base::TimeDelta> GetRemainingTimeForTesting(
      ProductMessageKey message_key) const;

 private:
  friend class ProductMessagingHandleImpl;

  struct QueueData;

  // Called by ProductMessagePriorityHandle when it is released. Clears the
  // current message and maybe tries to start the next.
  void ReleaseHandle(ProductMessageKey message_key, bool message_shown);

  // Shows the next message, if one is eligible, by calling
  // `MaybeShowNextProductMessageImpl()` on a fresh call stack.
  void MaybeShowNextMessage();

  // Remove any queued message that should not show.
  void PurgeBlockedAndExpiredMessages();

  // Actually shows the next message, if one is eligible. Must be called on a
  // fresh call stack, and should only be queued by
  // `MaybeShowNextProductMessage()`.
  void MaybeShowNextMessageImpl();

  // Do housekeeping associated with a new session.
  void OnNewSession();

  // Notify that the message was actually shown.
  void OnMessageShown(ProductMessageKey message_key);

  // Describes the current contents of `pending_messages_` for debugging/error
  // purposes.
  std::string DumpData() const;

  // Map from message key to whether the message is marked as "shown".
  std::map<ProductMessageKey, bool> active_messages_;

  raw_ptr<UserEducationStorageService> storage_service_ = nullptr;
  std::map<ProductMessageKey, QueueData> pending_messages_;
  std::unique_ptr<ProductMessagingPolicy> policy_;
  base::CallbackListSubscription session_subscription_;
  base::RepeatingCallbackList<ProductMessageStatusCallback::RunType>
      status_update_callbacks_;
  base::WeakPtrFactory<ProductMessagingController> weak_ptr_factory_{this};
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_PRODUCT_MESSAGING_PRODUCT_MESSAGING_CONTROLLER_H_
