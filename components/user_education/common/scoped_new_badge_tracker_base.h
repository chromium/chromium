// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_SCOPED_NEW_BADGE_TRACKER_BASE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_SCOPED_NEW_BADGE_TRACKER_BASE_H_

#include <set>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"

namespace feature_engagement {
class Tracker;
}

namespace user_education {

// Works with the Feature Engagement system to determine when/how many times a
// New Badge is displayed to the user. Wraps a feature_engagement::Tracker so
// the correct calls are made to the Feature Engagement backend.
//
// Derive a subclass for a more convenient constructor (e.g. one that takes a
// Browser or Profile rather than a raw feature_engagement::Tracker).
//
// The lifespan of a ScopedNewBadgeTracker should match the time the dialog or
// menu containing the New Badge is visible to the user.
//
// You may use a single tracker for New Badges on multiple features in the same
// menu or dialog, but make sure the feature flags and event names are distinct.
//
// Usage:
//
// * Menus
//
// Below is an example of using a ScopedNewBadgeTracker to add a New Badge to a
// menu where the object implementing ui::SimpleMenuModel::Delegate is created
// each time the menu is shown (e.g. AppMenuModel, TabContextMenuContents,
// etc.) The case where the delegate object is persistent will be discussed
// later.
//
//   // In OnMenuWillShow(menu):
//   menu->SetIsNewFeatureAt(
//       menu->GetIndexOfCommandId(IDC_MY_FEATURE).value(),
//       new_badge_tracker_.TryShowNewBadge(
//           feature_engagement::kIPHMyFeatureNewBadge,
//           &ui_features::kMyFeature));
//
//   // In ExecuteCommand():
//   case IDC_MY_FEATURE:
//     new_badge_tracker_.EventPerformed("my_feature_activated");
//     ...
//     break;
//
// If the New Badge is in the top-level menu, you can move the call to
// SetIsNewFeatureAt() to immediately after the menu model is initialized
// (typically in the constructor or "Init" method) and you will not have to
// override OnMenuWillBeShown().
//
// If you are handling multiple New Badges for different features, you will
// want to check the result of GetIndexOfCommand() to make sure the menu being
// shown is the one that contains the item receiving the new badge.
//
// If the ui::SimpleMenuModel::Delegate is a persistent object and is not
// created each time the menu is displayed, you will need to move the tracker
// down into the ui::SimpleMenuModel for your menu, and move your code from
// OnMenuWillShow() to MenuWillShow() and from ExecuteCommand() to
// ActivatedAt(int, int). Be sure to invoke base class behavior when overriding
// these methods!
//
// * Dialogs
//
// Add a ScopedNewBadgeTracker member variable to your DialogDelegateView.
// Dialogs are typically not re-usable; we create a new DialogDelegateView for
// each time we show them. If you are following this pattern, include this in
// your constructor or "Init" function after creating the NewBadgeLabel (but
// before showing the dialog):
//
//   new_badge_label_->SetDisplayNewBadge(
//       new_badge_tracker_.TryShowNewBadge(
//           feature_engagement::kIPHMyFeatureNewBadge,
//           &ui_features::kMyFeature));
//
// Then in the callback for the button that activates the feature being
// promoted, call:
//
//   new_badge_tracker_.EventPerformed("my_feature_activated");
//
// If for some reason you are re-using a dialog delegate, you must dynamically
// create and destroy the tracker when the dialog is shown and hidden.
class ScopedNewBadgeTrackerBase {
 public:
  // Constructs a scoped tracker for browser with |profile|.
  explicit ScopedNewBadgeTrackerBase(feature_engagement::Tracker* tracker);

  // This object should be destructed when the New Badge is going away, such as
  // when a menu with a New Badge or a dialog with a NewBadgeLabel is closing.
  // If TryShowNewBadge() returned true, the tracker will be informed that the
  // promo has ended.
  ~ScopedNewBadgeTrackerBase();

  ScopedNewBadgeTrackerBase(const ScopedNewBadgeTrackerBase& other) = delete;
  void operator=(const ScopedNewBadgeTrackerBase& other) = delete;

  // Returns whether the New Badge should be shown.
  //
  // |badge_feature| is the feature flag for the New Badge itself.
  //
  // |promoted_feature|, if specified, is the flag for the feature the New Badge
  // is promoting. You generally want to specify this feature even if the two
  // flags are controlled by the same Finch study, because the user could
  // override one but not the other. This parameter is optional because a New
  // Badge promo could be shown for a feature without a flag, or for a feature
  // that has already rolled to 100% and whose flag has been removed.
  bool TryShowNewBadge(const base::Feature& badge_feature,
                       const base::Feature* promoted_feature = nullptr);

  // Indicates that the user performed a promoted action. |action_event_name|
  // should be the value referenced in the "event_used" parameter of your field
  // trial configuration.
  // Note: this is a wrapper around feature_engagement::Tracker::NotifyEvent().
  void ActionPerformed(const char* action_event_name);

 private:
  const raw_ptr<feature_engagement::Tracker> tracker_;
  std::set<const base::Feature*> active_badge_features_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_SCOPED_NEW_BADGE_TRACKER_BASE_H_
