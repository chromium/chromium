// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_PRODUCT_MESSAGING_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_PRODUCT_MESSAGING_CONTROLLER_H_

#include <map>
#include <set>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/user_education/common/session/user_education_session_manager.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/interaction/element_identifier.h"

namespace user_education {

class ProductMessagingController;

// Opaque ID for required notices.
//
// Use DECLARE/DEFINE_REQUIRED_NOTICE_IDENTIFIER() below to create these for
// your notices.
using RequiredNoticeId = ui::ElementIdentifier;

// Place this in a .h file:
#define DECLARE_REQUIRED_NOTICE_IDENTIFIER(name) \
  DECLARE_ELEMENT_IDENTIFIER_VALUE(name)

// Place this in a .cc file:
#define DEFINE_REQUIRED_NOTICE_IDENTIFIER(name) \
  DEFINE_ELEMENT_IDENTIFIER_VALUE(name)

// This can be used in tests to avoid name conflicts.
#define DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(name) \
  DEFINE_MACRO_ELEMENT_IDENTIFIER_VALUE(__FILE__, __LINE__, name)

namespace internal {
// Special value in the "show after" list that causes the notice to happen last.
DECLARE_REQUIRED_NOTICE_IDENTIFIER(kShowAfterAllNotices);
}  // namespace internal

// The owner of this object currently has priority to show a required product
// notice. It must be held while the notice is showing and released immediately
// after the notice is dismissed.
class [[nodiscard]] RequiredNoticePriorityHandle final {
 public:
  RequiredNoticePriorityHandle();
  RequiredNoticePriorityHandle(RequiredNoticePriorityHandle&&) noexcept;
  RequiredNoticePriorityHandle& operator=(
      RequiredNoticePriorityHandle&&) noexcept;
  ~RequiredNoticePriorityHandle();

  // Whether this handle is valid.
  explicit operator bool() const;
  bool operator!() const;

  RequiredNoticeId notice_id() const { return notice_id_; }

  // Set that the notice was actually shown. Cannot be called on a null handle
  // or after releasing. Call to specify that the given notice was actually
  // shown; if you discard or release the handle without calling this function,
  // it is assumed that the notice was not shown.
  void SetShown();

  // Release the handle, resetting to default (null/falsy) value.
  void Release();

 private:
  friend class ProductMessagingController;
  RequiredNoticePriorityHandle(
      RequiredNoticeId notice_id,
      base::WeakPtr<ProductMessagingController> controller);

  bool shown_ = false;
  RequiredNoticeId notice_id_;
  base::WeakPtr<ProductMessagingController> controller_;
};

// Callback when a required notice is ready to show. The notice should show
// immediately.
//
// `handle` should be moved to a semi-permanent location and released when the
// notice is dismissed/closes. Failure to hold or release the handle can cause
// problems with User Education and other required notices.
using RequiredNoticeShowCallback =
    base::OnceCallback<void(RequiredNoticePriorityHandle handle)>;

// Coordinates between critical product messaging (e.g. legal notices) that must
// show in Chrome, to ensure that (a) they do not show over each other and (b)
// no other spontaneous User Education experiences start at the same time.
class ProductMessagingController final {
 public:
  ProductMessagingController();
  ProductMessagingController(const ProductMessagingController&) = delete;
  void operator=(const ProductMessagingController&) = delete;
  ~ProductMessagingController();

  // Register the session provider which is used to clear the set of shown
  // notices and the storage service used to retrieve shown promos.
  void Init(UserEducationSessionProvider& session_provider,
            UserEducationStorageService& storage_service);

  // Returns whether there are any notices queued or showing. This can be used
  // to prevent other, lower-priority User Education experiences from showing.
  bool has_pending_notices() const {
    return current_notice_ || !pending_notices_.empty();
  }

  // Checks whether the given `notice_id` is queued.
  bool IsNoticeQueued(RequiredNoticeId notice_id) const;

  // Requests that `notice_id` be queued to show. When it is allowed (which
  // might be as soon as the current message queue empties),
  // `ready_to_start_callback` will be called.
  //
  // If `always_show_after` is provided, then this notice is guaranteed to show
  // after the specified notices; otherwise the order of notices is not defined.
  //
  // The `blocked_by` list is similar to `always_show_after`, but if one of the
  // listed notices is successfully shown, this notice will not be shown this
  // session. Be aware that specifying one or more notices on the `blocked_by`
  // list may mean `ready_to_start_callback` is never called.
  //
  // Similarly, re-queueing a notice that is already showing or has been
  // successfully shown will have no effect, and `ready_to_start_callback` will
  // not be called.
  //
  // The expectation is that all of the notices will be queued during browser
  // startup, so that even if A must show after B, but B requests to show just
  // before A, then they will still show in the correct order starting a frame
  // or two later.
  void QueueRequiredNotice(
      RequiredNoticeId notice_id,
      RequiredNoticeShowCallback ready_to_start_callback,
      std::initializer_list<RequiredNoticeId> always_show_after = {},
      std::initializer_list<RequiredNoticeId> blocked_by = {});

  // Removes `notice_id` from the queue, if it is queued.
  // Has no effect if the notice has already started to show.
  void UnqueueRequiredNotice(RequiredNoticeId notice_id);

  RequiredNoticeId current_notice_for_testing() const {
    return current_notice_;
  }

 private:
  friend class RequiredNoticePriorityHandle;
  struct RequiredNoticeData;

  bool ready_to_show() const {
    CHECK(storage_service_) << "Must call Init() before queueing notices.";
    return !current_notice_ && !pending_notices_.empty();
  }

  // Called by RequiredNoticePriorityHandle when it is released. Clears the
  // current notice and maybe tries to start the next.
  void ReleaseHandle(RequiredNoticeId notice_id, bool notice_shown);

  // Shows the next notice, if one is eligible, by calling
  // `MaybeShowNextRequiredNoticeImpl()` on a fresh call stack.
  void MaybeShowNextRequiredNotice();

  // Remove any queued notice that should not show.
  //
  // A notice is blocked if another notice in its `blocked_by` list has been
  // shown, or if the same notice has already been shown this session.
  void PurgeBlockedNotices();

  // Actually shows the next notice, if one is eligible. Must be called on a
  // fresh call stack, and should only be queued by
  // `MaybeShowNextRequiredNotice()`.
  void MaybeShowNextRequiredNoticeImpl();

  // Do housekeeping associated with a new session.
  void OnNewSession();

  // Describes the current contents of `pending_notices_` for debugging/error
  // purposes.
  std::string DumpData() const;

  RequiredNoticeId current_notice_;
  raw_ptr<UserEducationStorageService> storage_service_ = nullptr;
  std::map<RequiredNoticeId, RequiredNoticeData> pending_notices_;
  base::CallbackListSubscription session_subscription_;
  base::WeakPtrFactory<ProductMessagingController> weak_ptr_factory_{this};
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_PRODUCT_MESSAGING_CONTROLLER_H_
