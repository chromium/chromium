// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_CUSTOM_HELP_BUBBLE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_CUSTOM_HELP_BUBBLE_H_

#include <concepts>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/user_education/common/help_bubble/help_bubble.h"
#include "ui/base/interaction/element_tracker.h"

namespace user_education {

class CustomHelpBubbleViews;

// Custom Help Bubbles

// A custom help bubble is a `HelpBubble` that wraps some UI that behaves like a
// help bubble, but is not one of the help bubbles defined here in
// `components/user_education`.
//
// The UI of a custom help bubble should implement `CustomHelpBubbleUi`,
// which allows it to send the same signals back to the promo controller as
// normal help bubble UIs do.
//
// This UI should be wrapped in a `HelpBubble` object that also implements
// `CustomHelpBubble`, which allows access to the special interface. Common
// implementations that wrap e.g. Views bubble dialogs and WebUI dialogs will
// be provided.
//
// All of these requirements are enforced via template requirements.

// This is an interface that must be implemented by custom help bubble UIs (or
// their controllers). Custom help bubble UI should not close themselves in
// response to user input but instead notify the system via `NotifyUserAction()`
// with one of the `UserAction` values.
//
// These user actions are observed by the promo controller, which uses them to
// record usage data, emit histograms, and properly mark the promo as dismissed
// or snoozed.
//
// Custom bubbles may be closed due to UI changes outside the bubble, via the
// promo being canceled, or through calls like `NotifyPromoFeatureUsed()`,
// `CloseBubbleAndContinuePromo()`, etc. These will be handled by the promo
// controller, not the bubble itself.
class CustomHelpBubbleUi {
 public:
  // Subset of `FeaturePromoClosedReason` that indicates the user engaged with
  // the promo and the custom help bubble should close.
  //
  // A custom help bubble should not close itself, but rather, should notify one
  // of these user actions and then wait to be closed.
  enum class UserAction {
    // The user pressed a button like "Got it" or "No thanks". The bubble will
    // not be able to be shown again.
    //
    // This is differentiated from `kCancel` for metrics reasons - we want to
    // know if the user interacted with an action button or just reflexively
    // closed the bubble.
    kDismiss,

    // The user pressed a button like "Maybe later". This enables the help
    // bubble to be shown again at a later time.
    kSnooze,

    // The user pressed a button or link that did some action in the browser,
    // turned on a setting, etc.
    kAction,

    // The user pressed a default close button (X) or pressed ESC. All custom
    // UI that are not mandatory legal or privacy messaging should have an (X)
    // button and respond to ESC.
    //
    // This is differentiated from `kDismiss` for metrics reasons - we want to
    // know if the user interacted with an action button or just reflexively
    // closed the bubble.
    kCancel,
  };
  using UserActionCallback = base::OnceCallback<void(UserAction)>;

  CustomHelpBubbleUi();
  CustomHelpBubbleUi(CustomHelpBubbleUi& other) = delete;
  void operator=(CustomHelpBubbleUi& other) = delete;
  virtual ~CustomHelpBubbleUi();

  // Registers a callback to be invoked by the bubble (via `NotifyUserAction()`)
  // when something causes the bubble to want to close itself (eg. a click on a
  // Snooze button).
  //
  // The promo controller adds a callback when the bubble is registered,
  // responds to the callback by closing the bubble.
  base::CallbackListSubscription AddUserActionCallback(
      UserActionCallback callback);

  // Get this object as a weak pointer.
  base::WeakPtr<CustomHelpBubbleUi> GetCustomUiAsWeakPtr();

 protected:
  // Notifies listeners that the user has done some significant input which
  // should close the help bubble (such as clicking on "Snooze" button or on a
  // link). The promo controller will record the result and close the bubble.
  //
  // Be aware that `this` may not be valid after calling this method.
  void NotifyUserAction(UserAction user_action);

 private:
  friend class CustomHelpBubbleViews;

  std::unique_ptr<base::OnceCallbackList<void(UserAction)>>
      user_action_callbacks_;
  base::WeakPtrFactory<CustomHelpBubbleUi> weak_ptr_factory_{this};
};

// Add-on interface for `HelpBubble`s that wrap `CustomHelpBubbleUi`.
class CustomHelpBubble {
 public:
  explicit CustomHelpBubble(CustomHelpBubbleUi& bubble);
  virtual ~CustomHelpBubble();

  CustomHelpBubbleUi* custom_bubble_ui() { return bubble_.get(); }
  const CustomHelpBubbleUi* custom_bubble_ui() const { return bubble_.get(); }

 private:
  base::WeakPtr<CustomHelpBubbleUi> bubble_;
};

// A custom help bubble is both a `CustomHelpBubble` and a `HelpBubble`.
template <typename T>
concept IsCustomHelpBubble =
    std::derived_from<T, HelpBubble> && std::derived_from<T, CustomHelpBubble>;

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_CUSTOM_HELP_BUBBLE_H_
