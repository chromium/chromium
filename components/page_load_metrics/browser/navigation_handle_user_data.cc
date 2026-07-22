// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/navigation_handle_user_data.h"

namespace page_load_metrics {

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(NavigationHandleUserData);

NavigationHandleUserData::NavigationHandleUserData(
    content::NavigationHandle& navigation,
    InitiatorLocation navigation_type,
    std::string navigation_type_string)
    : navigation_type_(navigation_type),
      navigation_type_string_(std::move(navigation_type_string)) {}

NavigationHandleUserData::~NavigationHandleUserData() = default;

}  // namespace page_load_metrics
