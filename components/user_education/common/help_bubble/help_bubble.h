// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_HELP_BUBBLE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_HELP_BUBBLE_H_

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/rect.h"

namespace user_education {

// HelpBubble is an interface for the lifecycle of an IPH or tutorial bubble.
// it is implemented by a framework's bubble. It is returned as the result of
// HelpBubbleFactory's CreateBubble...() method.
class HelpBubble : public ui::FrameworkSpecificImplementation {
 public:
  // Describes why a help bubble was closed.
  enum class CloseReason {
    // The bubble was closed expectedly; for example, because an action button
    // or the close button was clicked.
    kProgrammaticallyClosed,
    // The bubble was closed because its anchor disappeared.
    kAnchorHidden,
    // The bubble UI element was destroyed for some reason.
    kBubbleElementDestroyed,
    // The HelpBubble object went out of scope.
    kBubbleDestroyed,
  };

  HelpBubble();
  ~HelpBubble() override;

  // Sets input focus on the bubble or on the bubble's anchor.
  virtual bool ToggleFocusForAccessibility() = 0;

  // Closes the bubble if it is not already closed. Returns whether the bubble
  // was open. Usual pattern is to call `BeginClose()` at the top and return the
  // result of `ScopedNotifyOnClose::is_valid()`.
  //
  // `Close()` should return true at most once and return false (and be a no-op)
  // on any subsequent call.
  virtual bool Close(CloseReason close_reason) = 0;

  // Notify that the element the help bubble is anchored to may have moved.
  // Default is no-op.
  virtual void OnAnchorBoundsChanged();

  // Get the bounds of the bubble in the screen. Default is gfx::Rect(), which
  // indicates that the bubble's screen position is not identifiable, or that
  // the bubble is not visible.
  virtual gfx::Rect GetBoundsInScreen() const;

  // Returns the context of this help bubble (if there is one).
  virtual ui::ElementContext GetContext() const = 0;

  // Add a callback to know when a help bubble is about to close. The help
  // bubble will still be valid during this call.
  using ClosingCallback =
      base::OnceCallback<void(const HelpBubble*, CloseReason)>;
  [[nodiscard]] base::CallbackListSubscription AddOnClosingCallback(
      ClosingCallback callback);

  // Add a callback for when the help bubble has been fully torn down.
  //
  // The caller can release the help bubble and other resources at this point if
  // that hasn't already been done (but note that the help bubble may no longer
  // exist at this point).
  using ClosedCallback = base::OnceCallback<void(CloseReason)>;
  [[nodiscard]] base::CallbackListSubscription AddOnClosedCallback(
      ClosedCallback callback);

  bool is_open() const { return !is_closed(); }

  base::WeakPtr<HelpBubble> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  using ClosingCallbackList = base::OnceCallbackList<ClosingCallback::RunType>;
  using ClosedCallbackList = base::OnceCallbackList<ClosedCallback::RunType>;

  // Created from `this` with the contents of `on_close_callbacks_`, calls
  // the callbacks (if any) when it goes out of scope.
  class [[nodiscard]] ScopedNotifyOnClosed {
   public:
    ScopedNotifyOnClosed();
    ScopedNotifyOnClosed(const HelpBubble* bubble,
                         CloseReason reason,
                         std::unique_ptr<ClosedCallbackList> callbacks);
    ScopedNotifyOnClosed(ScopedNotifyOnClosed&&) noexcept;
    ScopedNotifyOnClosed& operator=(ScopedNotifyOnClosed&&) noexcept;
    ~ScopedNotifyOnClosed();

    bool is_valid() const { return callbacks_ != nullptr; }

   private:
    CloseReason reason_ = CloseReason::kProgrammaticallyClosed;
    std::unique_ptr<ClosedCallbackList> callbacks_;
  };

  // Call when you are tearing down the bubble; the resulting object will ensure
  // that - if the bubble was not previously closed - the proper close callbacks
  // will be triggered when the resulting object goes out of scope.
  //
  // If called a second time, `ScopedNotifyOnClose::is_valid()` will be false
  // and the call and destructor are a no-op.
  ScopedNotifyOnClosed BeginClose(CloseReason reason);

 private:
  // Closed callbacks are cleared out on close, so this keeps us from having to
  // store extra data about closed status that could become out of sync.
  bool is_closed() const { return !on_closed_callbacks_; }

  ClosingCallbackList on_closing_callbacks_;
  std::unique_ptr<ClosedCallbackList> on_closed_callbacks_;
  base::WeakPtrFactory<HelpBubble> weak_ptr_factory_{this};
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_HELP_BUBBLE_H_
