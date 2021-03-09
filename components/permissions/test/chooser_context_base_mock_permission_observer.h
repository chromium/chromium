// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_CHOOSER_CONTEXT_BASE_MOCK_PERMISSION_OBSERVER_H_
#define COMPONENTS_PERMISSIONS_TEST_CHOOSER_CONTEXT_BASE_MOCK_PERMISSION_OBSERVER_H_

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/chooser_context_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace permissions {

class MockPermissionObserver : public ChooserContextBase::PermissionObserver {
 public:
  MockPermissionObserver();
  ~MockPermissionObserver() override;

  MOCK_METHOD2(OnChooserObjectPermissionChanged,
               void(ContentSettingsType guard_content_settings_type,
                    ContentSettingsType data_content_settings_type));
  MOCK_METHOD1(OnPermissionRevoked, void(const url::Origin& origin));
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_CHOOSER_CONTEXT_BASE_MOCK_PERMISSION_OBSERVER_H_
