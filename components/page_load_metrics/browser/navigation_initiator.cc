// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/navigation_initiator.h"

#include "base/functional/bind.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/page_transition_types.h"

namespace page_load_metrics {

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(NavigationInitiatorHolder);

NavigationInitiatorHolder::NavigationInitiatorHolder(
    content::NavigationHandle& navigation_handle,
    NavigationInitiator initiator)
    : initiator_(initiator) {}

NavigationInitiatorHolder::~NavigationInitiatorHolder() = default;

// static
base::RepeatingCallback<void(content::NavigationHandle&)>
NavigationInitiatorHolder::AttacherCallback(NavigationInitiator initiator) {
  return base::BindRepeating(
      [](NavigationInitiator initiator,
         content::NavigationHandle& navigation_handle) {
        CreateForNavigationHandle(navigation_handle, initiator);
      },
      initiator);
}

NavigationInitiator GetNavigationInitiator(
    content::NavigationHandle& navigation_handle) {
  // Check forward/back and BFCache before reload, as reloaded entries preserve
  // `PAGE_TRANSITION_FORWARD_BACK` which `PageTransitionCoreTypeIs()` strips.
  if ((navigation_handle.GetPageTransition() &
       ui::PAGE_TRANSITION_FORWARD_BACK) ||
      navigation_handle.IsServedFromBackForwardCache()) {
    int history_offset = navigation_handle.GetNavigationEntryOffset();
    if (history_offset > 0) {
      return navigation_initiator::kForward;
    }

    if (history_offset < 0) {
      return navigation_initiator::kBackward;
    }

    // `history_offset` can be 0 when a reload navigation is served from BFCache
    // (crbug.com/420769973). Fall through to the subsequent checks so that it
    // is classified as `kReload`.
    // TODO(crbug.com/420769973): Fix this behavior, and avoid the fall-through.
  }

  if (ui::PageTransitionCoreTypeIs(navigation_handle.GetPageTransition(),
                                   ui::PAGE_TRANSITION_RELOAD)) {
    return navigation_initiator::kReload;
  }

  // The initiator that a trigger attached, e.g. `kBookmarkBar`. It is more
  // specific than the ones derived from `ui::PageTransition` below, so it
  // takes precedence over them.
  if (NavigationInitiatorHolder* holder =
          NavigationInitiatorHolder::GetForNavigationHandle(
              navigation_handle)) {
    return holder->initiator();
  }

  if (navigation_handle.IsRendererInitiated() &&
      navigation_handle.HasUserGesture()) {
    if (ui::PageTransitionCoreTypeIs(navigation_handle.GetPageTransition(),
                                     ui::PAGE_TRANSITION_LINK)) {
      return navigation_initiator::kLinkClick;
    }

    if (ui::PageTransitionCoreTypeIs(navigation_handle.GetPageTransition(),
                                     ui::PAGE_TRANSITION_FORM_SUBMIT)) {
      return navigation_initiator::kFormSubmission;
    }
  }

  return navigation_initiator::kOther;
}

}  // namespace page_load_metrics
