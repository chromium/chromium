// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_H_

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/functional/callback.h"
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

  // Callback to be notified when the help bubble is closed. Note that the
  // pointer passed in is entirely for reference and should not be dereferenced
  // as another callback may have deleted the bubble itself.
  using ClosedCallback = base::OnceCallback<void(HelpBubble*, CloseReason)>;

  HelpBubble();
  ~HelpBubble() override;

  // Sets input focus on the bubble or on the bubble's anchor.
  virtual bool ToggleFocusForAccessibility() = 0;

  // Closes the bubble if it is not already closed. Returns whether the bubble
  // was open.
  bool Close(CloseReason close_reason = CloseReason::kProgrammaticallyClosed);

  // Notify that the element the help bubble is anchored to may have moved.
  // Default is no-op.
  virtual void OnAnchorBoundsChanged();

  // Get the bounds of the bubble in the screen. Default is gfx::Rect(), which
  // indicates that the bubble's screen position is not identifiable, or that
  // the bubble is not visible.
  virtual gfx::Rect GetBoundsInScreen() const;

  // Returns the context of this help bubble (if there is one).
  virtual ui::ElementContext GetContext() const = 0;

  // Add a callback to know when a bubble is going away.
  [[nodiscard]] base::CallbackListSubscription AddOnCloseCallback(
      ClosedCallback callback);

  bool is_open() const { return !is_closed(); }

 protected:
  // Actually close the bubble.
  virtual void CloseBubbleImpl() = 0;

 private:
  // Closed callbacks are cleared out on close, so this keeps us from having to
  // store extra data about closed status that could become out of sync.
  bool is_closed() const { return !on_close_callbacks_; }

  using CallbackList = base::OnceCallbackList<ClosedCallback::RunType>;
  std::unique_ptr<CallbackList> on_close_callbacks_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_H_
