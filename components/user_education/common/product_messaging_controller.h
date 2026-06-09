// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_PRODUCT_MESSAGING_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_PRODUCT_MESSAGING_CONTROLLER_H_

#include <map>
#include <set>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/user_education/common/session/user_education_session_manager.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/identifier/unique_identifier.h"

namespace user_education {

class ProductMessagingController;

namespace internal {

class MessagingCoordinator;

// Opaque ID for required messages. Do not use directly.
DECLARE_UNIQUE_IDENTIFIER_TYPE(ProductMessageUniqueId);

}  // namespace internal

// An enumeration of all known product message types, in general order of
// priority from lowest to highest.
enum class ProductMessageType {
  // This value is only for filtering and must be first on the list.
  kNone = 0,
  kLowPriorityIph,
  kHighPriorityIph,
  kLegalOrComplianceNotice,
  kMaxValue = kLegalOrComplianceNotice
};

// Used to uniquely identify a message and define its type.
// Opaque, but the type can be retrieved.
// Declare with the macros below.
class ProductMessageKey {
 public:
  consteval ProductMessageKey() = default;
  consteval ProductMessageKey(internal::ProductMessageUniqueId id,
                              ProductMessageType type)
      : id_(id), type_(type) {}

  explicit constexpr operator bool() const { return !!id_; }
  bool operator<(ProductMessageKey other) const;
  friend constexpr bool operator==(ProductMessageKey lhs,
                                   ProductMessageKey rhs) = default;

  std::string GetName() const;
  std::string ToString() const;

  ProductMessageType type() const { return type_; }

 private:
  internal::ProductMessageUniqueId id_;
  ProductMessageType type_ = ProductMessageType::kNone;
};

// Use these macros to declare keys.

// Place this in a .h file:
#define DECLARE_PRODUCT_MESSAGE_KEY(name, type)                             \
  DECLARE_UNIQUE_IDENTIFIER_VALUE(                                          \
      ::user_education::internal::ProductMessageUniqueId, name##UniqueId);  \
  inline constexpr ::user_education::ProductMessageKey name(name##UniqueId, \
                                                            type)

// Place this in a .cc file:
#define DEFINE_PRODUCT_MESSAGE_KEY(name) \
  DEFINE_UNIQUE_IDENTIFIER_VALUE(        \
      ::user_education::internal::ProductMessageUniqueId, name##UniqueId)

// This can be used in tests to avoid name conflicts.
#define DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(name, type)                          \
  DEFINE_MACRO_LOCAL_UNIQUE_IDENTIFIER_VALUE(                                 \
      __FILE__, __LINE__, ::user_education::internal::ProductMessageUniqueId, \
      name##UniqueId);                                                        \
  static constexpr ::user_education::ProductMessageKey name(name##UniqueId,   \
                                                            type)

// This can be used to scope an identifier to a class; use this in the public
// part of the class definition.
#define DECLARE_CLASS_PRODUCT_MESSAGE_KEY(name, type)                      \
  DECLARE_CLASS_UNIQUE_IDENTIFIER_VALUE(                                   \
      ::user_education::internal::ProductMessageUniqueId, name##UniqueId); \
  static constexpr ::user_education::ProductMessageKey name {              \
    name##UniqueId, type                                                   \
  }

// Use this in the .cc file to define an identifier scoped to a class, this must
// be paired with the DECLARE macro above.
#define DEFINE_CLASS_PRODUCT_MESSAGE_KEY(Class, Name)            \
  DEFINE_CLASS_UNIQUE_IDENTIFIER_VALUE(                          \
      Class, ::user_education::internal::ProductMessageUniqueId, \
      Name##UniqueId)

// The status of a message.
enum class ProductMessageStatus { kNone, kQueued, kEligible, kShowing };

using ProductMessageStatusCallback =
    base::RepeatingCallback<void(ProductMessageKey, ProductMessageStatus)>;

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
            UserEducationStorageService& storage_service);

  // Checks whether the given `message_id` is queued.
  bool IsMessageQueued(ProductMessageKey message_key) const;

  // Requests that `message_key` be queued to show. When it is allowed (which
  // might be as soon as the current message queue empties),
  // `ready_to_start_callback` will be called.
  //
  // If `always_show_after` is provided, then this message is guaranteed to show
  // after the specified messages; otherwise the order of messages is not
  // defined.
  //
  // The `blocked_by` list is similar to `always_show_after`, but if one of the
  // listed messages is successfully shown, this message will not be shown this
  // session. Be aware that specifying one or more messages on the `blocked_by`
  // list may mean `ready_to_start_callback` is never called.
  //
  // Similarly, re-queueing a message that is already showing or has been
  // successfully shown will have no effect, and `ready_to_start_callback` will
  // not be called.
  //
  // The expectation is that all of the messages will be queued during browser
  // startup, so that even if A must show after B, but B requests to show just
  // before A, then they will still show in the correct order starting a frame
  // or two later.
  void QueueMessage(
      ProductMessageKey message_key,
      ProductMessageReadyCallback ready_to_start_callback,
      std::initializer_list<ProductMessageKey> always_show_after = {},
      std::initializer_list<ProductMessageKey> blocked_by = {});

  // Removes `message_id` from the queue, if it is queued.
  // Has no effect if the message has already started to show.
  void UnqueueMessage(ProductMessageKey message_key);

  // Returns the status of `message`.
  ProductMessageStatus GetProductMessageStatus(ProductMessageKey message) const;

  // Returns queued or showing messages. Can be filtered by priority and by
  // status.
  base::flat_map<ProductMessageKey, ProductMessageStatus> GetAllMessages(
      std::initializer_list<ProductMessageStatus> statuses_to_retrieve =
          {ProductMessageStatus::kQueued, ProductMessageStatus::kEligible,
           ProductMessageStatus::kShowing},
      ProductMessageType priority_higher_than =
          ProductMessageType::kNone) const;

  // Adds a callback that will be called whenever the status of a message
  // changes.
  base::CallbackListSubscription AddStatusUpdateCallbackForTesting(
      ProductMessageStatusCallback callback);

  // Returns if any messages queued or showing.
  bool HasPendingMessagesForTesting() const;

 private:
  friend class ProductMessagingHandleImpl;
  friend class internal::MessagingCoordinator;
  struct ProductMessageData;

  bool ready_to_show() const {
    CHECK(storage_service_) << "Must call Init() before queueing messages.";
    return !current_message_ && !pending_messages_.empty();
  }

  // Called by ProductMessagePriorityHandle when it is released. Clears the
  // current message and maybe tries to start the next.
  void ReleaseHandle(ProductMessageKey message_key, bool message_shown);

  // Shows the next message, if one is eligible, by calling
  // `MaybeShowNextProductMessageImpl()` on a fresh call stack.
  void MaybeShowNextMessage();

  // Remove any queued message that should not show.
  //
  // A message is blocked if another message in its `blocked_by` list has been
  // shown, or if the same message has already been shown this session.
  void PurgeBlockedMessages();

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

  ProductMessageKey current_message_;
  bool current_message_shown_ = false;
  raw_ptr<UserEducationStorageService> storage_service_ = nullptr;
  std::map<ProductMessageKey, ProductMessageData> pending_messages_;
  base::CallbackListSubscription session_subscription_;
  base::RepeatingCallbackList<ProductMessageStatusCallback::RunType>
      status_update_callbacks_;
  base::WeakPtrFactory<ProductMessagingController> weak_ptr_factory_{this};
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_PRODUCT_MESSAGING_CONTROLLER_H_
