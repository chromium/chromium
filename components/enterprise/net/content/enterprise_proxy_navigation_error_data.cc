// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/content/enterprise_proxy_navigation_error_data.h"

#include <utility>

namespace enterprise_net {

EnterpriseProxyNavigationErrorData::~EnterpriseProxyNavigationErrorData() =
    default;

// static
bool EnterpriseProxyNavigationErrorData::HasDisguisedErrorData(
    content::NavigationHandle* navigation_handle) {
  return Get(navigation_handle) != nullptr;
}

// static
EnterpriseProxyNavigationErrorData* EnterpriseProxyNavigationErrorData::Get(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return nullptr;
  }
  return GetForNavigationHandle(*navigation_handle);
}

// `navigation_handle` is required by content::NavigationHandleUserData to
// instantiate the subclass.
EnterpriseProxyNavigationErrorData::EnterpriseProxyNavigationErrorData(
    content::NavigationHandle& /*navigation_handle*/,
    EnterpriseProxyErrorData error_data)
    : EnterpriseProxyErrorData(std::move(error_data)) {}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(EnterpriseProxyNavigationErrorData);

EnterpriseProxyErrorDataDelegate::EnterpriseProxyErrorDataDelegate(
    content::NavigationHandle* navigation_handle)
    : navigation_handle_(navigation_handle) {}

EnterpriseProxyErrorDataDelegate::~EnterpriseProxyErrorDataDelegate() = default;

const EnterpriseProxyErrorData*
EnterpriseProxyErrorDataDelegate::GetDisguisedErrorData() const {
  return EnterpriseProxyNavigationErrorData::Get(navigation_handle_);
}

void EnterpriseProxyErrorDataDelegate::AttachDisguisedErrorData(
    const EnterpriseProxyErrorData& error_data) {
  if (!navigation_handle_) {
    return;
  }
  EnterpriseProxyNavigationErrorData::CreateForNavigationHandle(
      *navigation_handle_, error_data);
}

}  // namespace enterprise_net
