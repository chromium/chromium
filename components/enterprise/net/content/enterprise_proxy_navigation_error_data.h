// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CONTENT_ENTERPRISE_PROXY_NAVIGATION_ERROR_DATA_H_
#define COMPONENTS_ENTERPRISE_NET_CONTENT_ENTERPRISE_PROXY_NAVIGATION_ERROR_DATA_H_

#include "base/memory/raw_ptr.h"
#include "components/enterprise/net/core/enterprise_proxy_error_data.h"
#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace enterprise_net {

// Attached to a NavigationHandle when an enterprise proxy disguised error
// is intercepted on non-iOS platforms.
class EnterpriseProxyNavigationErrorData
    : public content::NavigationHandleUserData<
          EnterpriseProxyNavigationErrorData>,
      public EnterpriseProxyErrorData {
 public:
  ~EnterpriseProxyNavigationErrorData() override;

  static bool HasDisguisedErrorData(
      content::NavigationHandle* navigation_handle);

  static EnterpriseProxyNavigationErrorData* Get(
      content::NavigationHandle* navigation_handle);

 private:
  EnterpriseProxyNavigationErrorData(
      content::NavigationHandle& navigation_handle,
      EnterpriseProxyErrorData error_data);

  friend content::NavigationHandleUserData<EnterpriseProxyNavigationErrorData>;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

// Non-iOS platform implementation of EnterpriseProxyErrorService::Delegate
// using content::NavigationHandle to attach EnterpriseProxyNavigationErrorData.
class EnterpriseProxyErrorDataDelegate
    : public EnterpriseProxyErrorService::Delegate {
 public:
  explicit EnterpriseProxyErrorDataDelegate(
      content::NavigationHandle* navigation_handle);
  EnterpriseProxyErrorDataDelegate(const EnterpriseProxyErrorDataDelegate&) =
      delete;
  EnterpriseProxyErrorDataDelegate& operator=(
      const EnterpriseProxyErrorDataDelegate&) = delete;
  ~EnterpriseProxyErrorDataDelegate() override;

  // EnterpriseProxyErrorService::Delegate:
  const EnterpriseProxyErrorData* GetDisguisedErrorData() const override;
  void AttachDisguisedErrorData(
      const EnterpriseProxyErrorData& error_data) override;

 private:
  raw_ptr<content::NavigationHandle> navigation_handle_ = nullptr;
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CONTENT_ENTERPRISE_PROXY_NAVIGATION_ERROR_DATA_H_
