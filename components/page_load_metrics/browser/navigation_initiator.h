// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_INITIATOR_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_INITIATOR_H_

#include <compare>
#include <string_view>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace page_load_metrics {

// Represents who/what triggered a navigation.
//
// This is an open enum: the integer portion is used for UKM logging and UMA
// bucketing, the string portion is used to dynamically compose UMA histogram
// names, and embedders are allowed to define their own initiators. See
// `//chrome/browser/page_load_metrics/chrome_navigation_initiator.h` for
// examples.
//
// This complements `ui::PageTransition`. An initiator that
// `ui::PageTransition` can tell (e.g. a link click or a reload) must not be
// defined nor attached by embedders, as it breaks the single source of truth.
// Such initiators are derived in `GetNavigationInitiator()`.
//
// Both the value and the name must be unique across all the layers. The value
// space is split by layer: 0-99 is the historical range that predates the
// split, `//components/page_load_metrics` takes 100-199 and embedders take
// 200-255 for new ones. Of the historical range,
// `//components/page_load_metrics` holds 0, 5-8 and 11, and embedders hold 1-4
// and 9-10.
class NavigationInitiator final {
 public:
  // Exclusive upper bound of `id()` over all the layers. `NavigationInitiator`
  // is an open enum, so it has no `kMaxValue` that
  // `base::UmaHistogramEnumeration()` requires; record `id()` with
  // `base::UmaHistogramExactLinear()` and this bound instead.
  //
  // This is deliberately a fixed number rather than one derived from the
  // defined `NavigationInitiator`s. Deriving it would shift the bucket layout
  // of the histograms every time an initiator is added, and the visible set
  // differs per translation unit because each embedder defines its own.
  static constexpr int kIdExclusiveMax = 256;

  // `consteval` so that every `NavigationInitiator` is built at compile time.
  // That makes the checks below unconditional, and guarantees that `name`
  // outlives the `std::string_view` referring to it.
  consteval NavigationInitiator(int id, std::string_view name)
      : id_(id), name_(name) {
    CHECK(0 <= id && id < kIdExclusiveMax);
    CHECK(!name.empty());
  }

  constexpr int id() const { return id_; }
  constexpr std::string_view name() const { return name_; }

  constexpr bool operator==(const NavigationInitiator& other) const {
    CHECK((id_ == other.id_) == (name_ == other.name_));
    return id_ == other.id_;
  }

  constexpr std::strong_ordering operator<=>(
      const NavigationInitiator& other) const {
    return id_ <=> other.id_;
  }

 private:
  int id_;
  // Points to a static string, guaranteed by the `consteval` constructor.
  std::string_view name_;
};

// `NavigationInitiator`s that are not specific to an embedder.
//
// LINT.IfChange(PageLoadMetricsNavigationInitiator)
namespace navigation_initiator {

// The fallback initiator.
//
// Used when the trigger of the navigation is unknown, or is not classified into
// any dedicated `NavigationInitiator`. Also used when a navigation has a
// relevant page transition (like link click or form submission) but lacks
// renderer initiation or user gesture, or when a history navigation has an
// offset of 0.
inline constexpr NavigationInitiator kOther{0, "Other"};

// The following are derived from `ui::PageTransition` in
// `GetNavigationInitiator()`, not attached by a trigger. See the comment of
// `NavigationInitiator`.

// Navigation triggered by clicking a link on a web page.
//
// Classified in `GetNavigationInitiator()` when
// `ui::PageTransitionCoreTypeIs(PAGE_TRANSITION_LINK)` holds, and the
// navigation is renderer-initiated (`navigation_handle.IsRendererInitiated()`)
// with a user gesture (`navigation_handle.HasUserGesture()`). If without a user
// gesture (e.g. script-driven link clicks), falls back to `kOther`.
inline constexpr NavigationInitiator kLinkClick{5, "LinkClick"};

// Navigation to a forward history entry or a BFCache restore.
//
// Classified in `GetNavigationInitiator()` when
// `(navigation_handle.GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK)`
// holds or `navigation_handle.IsServedFromBackForwardCache()` is true, and
// `navigation_handle.GetNavigationEntryOffset() > 0`.
inline constexpr NavigationInitiator kForward{6, "Forward"};

// Navigation to a backward history entry or a BFCache restore.
//
// Classified in `GetNavigationInitiator()` when
// `(navigation_handle.GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK)`
// holds or `navigation_handle.IsServedFromBackForwardCache()` is true, and
// `navigation_handle.GetNavigationEntryOffset() < 0`.
inline constexpr NavigationInitiator kBackward{7, "Backward"};

// Navigation triggered by reloading the page.
//
// Classified in `GetNavigationInitiator()` when
// `ui::PageTransitionCoreTypeIs(PAGE_TRANSITION_RELOAD)` holds, provided that
// it is not classified as a forward/backward navigation (since pages restored
// from BFCache preserve the previous transition type).
inline constexpr NavigationInitiator kReload{8, "Reload"};

// Navigation triggered by submitting an HTML form.
//
// Classified in `GetNavigationInitiator()` when
// `ui::PageTransitionCoreTypeIs(PAGE_TRANSITION_FORM_SUBMIT)` holds, and the
// navigation is renderer-initiated (`navigation_handle.IsRendererInitiated()`)
// with a user gesture (`navigation_handle.HasUserGesture()`). If without a user
// gesture, falls back to `kOther`.
inline constexpr NavigationInitiator kFormSubmission{11, "FormSubmission"};

}  // namespace navigation_initiator
// LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:NavigationInitiatorType)

// Carries a `NavigationInitiator` that a navigation trigger attached.
//
// Triggers attach this when they create a navigation, typically via the
// `navigation_handle_callback` of `PageNavigator::OpenURL()`. Note that
// `content::NavigationHandleUserData` is create-once: the first attachment
// wins.
//
// Use `GetNavigationInitiator()` instead of using this class directly.
class NavigationInitiatorHolder
    : public content::NavigationHandleUserData<NavigationInitiatorHolder> {
 public:
  ~NavigationInitiatorHolder() override;

  // Returns a callback that attaches `initiator` to the navigation it is run
  // with, for the `navigation_handle_callback` of `PageNavigator::OpenURL()`.
  static base::RepeatingCallback<void(content::NavigationHandle&)>
  AttacherCallback(NavigationInitiator initiator);

  const NavigationInitiator& initiator() const { return initiator_; }

 private:
  NavigationInitiatorHolder(content::NavigationHandle& navigation_handle,
                            NavigationInitiator initiator);

  const NavigationInitiator initiator_;

  friend content::NavigationHandleUserData<NavigationInitiatorHolder>;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

// Returns the `NavigationInitiator` for `navigation_handle`, derived from
// `ui::PageTransition` or an attached `NavigationInitiatorHolder`, falling back
// to `kOther`.
//
// Timing of availability: The attachment is not guaranteed to be done at
// `PageLoadMetricsObserver::OnStart()`. `PageLoadMetricsObserver::OnCommit()`
// (or `DidActivatePrerenderedPage()` for prerender activation) is a reliable
// timing.
NavigationInitiator GetNavigationInitiator(
    content::NavigationHandle& navigation_handle);

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_INITIATOR_H_
