// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/navigation_capturing_navigation_handle_user_data.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "ui/base/window_open_disposition.h"

namespace web_app {

NavigationCapturingRedirectionInfo::NavigationCapturingRedirectionInfo() =
    default;
NavigationCapturingRedirectionInfo::~NavigationCapturingRedirectionInfo() =
    default;
NavigationCapturingRedirectionInfo::NavigationCapturingRedirectionInfo(
    const NavigationCapturingRedirectionInfo& navigation_info) = default;

NavigationCapturingNavigationHandleUserData::
    ~NavigationCapturingNavigationHandleUserData() = default;

NavigationCapturingNavigationHandleUserData::
    NavigationCapturingNavigationHandleUserData(
        content::NavigationHandle& navigation_handle,
        NavigationCapturingRedirectionInfo redirection_info)
    : redirection_info_(std::move(redirection_info)) {}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(
    NavigationCapturingNavigationHandleUserData);

}  // namespace web_app
