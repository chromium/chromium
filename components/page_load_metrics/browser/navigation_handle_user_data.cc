// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/navigation_handle_user_data.h"

namespace page_load_metrics {

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(NavigationHandleUserData);

// static
void NavigationHandleUserData::AttachNewTabPageNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle,
      page_load_metrics::NavigationHandleUserData::InitiatorLocation::
      kNewTabPage);
}

}  // namespace page_load_metrics
