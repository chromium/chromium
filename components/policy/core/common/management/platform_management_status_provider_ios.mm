// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/policy/core/common/management/platform_management_status_provider_ios.h"

#import <Foundation/Foundation.h>

#import "base/enterprise_util.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/core/common/policy_pref_names.h"

namespace policy {

DeviceManagementStatusProvider::DeviceManagementStatusProvider() = default;

DeviceManagementStatusProvider::~DeviceManagementStatusProvider() = default;

EnterpriseManagementAuthority DeviceManagementStatusProvider::FetchAuthority() {
  BOOL isManagedDevice =
      [[NSUserDefaults standardUserDefaults]
          dictionaryForKey:kPolicyLoaderIOSConfigurationKey] != nil;
  return isManagedDevice ? EnterpriseManagementAuthority::CLOUD
                         : EnterpriseManagementAuthority::NONE;
}

}  // namespace policy
