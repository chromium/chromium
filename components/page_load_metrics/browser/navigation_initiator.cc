// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/navigation_initiator.h"

#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "content/public/browser/navigation_handle.h"

namespace page_load_metrics {

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(NavigationInitiatorHolder);

NavigationInitiatorHolder::NavigationInitiatorHolder(
    content::NavigationHandle& navigation_handle,
    NavigationInitiator initiator)
    : initiator_(initiator) {}

NavigationInitiatorHolder::~NavigationInitiatorHolder() = default;

std::optional<NavigationInitiator> GetNavigationInitiator(
    content::NavigationHandle& navigation_handle) {
  NavigationInitiatorHolder* holder =
      NavigationInitiatorHolder::GetForNavigationHandle(navigation_handle);
  if (!holder) {
    return std::nullopt;
  }

  return holder->initiator();
}

int64_t GetAttachedNavigationInitiatorId(
    content::NavigationHandle& navigation_handle) {
  if (std::optional<NavigationInitiator> initiator =
          GetNavigationInitiator(navigation_handle)) {
    return initiator->id();
  }

  NavigationHandleUserData* user_data =
      NavigationHandleUserData::GetForNavigationHandle(navigation_handle);
  if (!user_data) {
    return navigation_initiator::kOther.id();
  }

  return user_data->navigation_type();
}

}  // namespace page_load_metrics
