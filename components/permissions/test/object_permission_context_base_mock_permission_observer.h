// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_OBJECT_PERMISSION_CONTEXT_BASE_MOCK_PERMISSION_OBSERVER_H_
#define COMPONENTS_PERMISSIONS_TEST_OBJECT_PERMISSION_CONTEXT_BASE_MOCK_PERMISSION_OBSERVER_H_

#include <optional>

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/object_permission_context_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace permissions {

class MockPermissionObserver
    : public ObjectPermissionContextBase::PermissionObserver {
 public:
  MockPermissionObserver();
  ~MockPermissionObserver() override;

  MOCK_METHOD2(
      OnObjectPermissionChanged,
      void(std::optional<ContentSettingsType> guard_content_settings_type,
           ContentSettingsType data_content_settings_type));
  MOCK_METHOD1(OnPermissionRevoked, void(const url::Origin& origin));
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_OBJECT_PERMISSION_CONTEXT_BASE_MOCK_PERMISSION_OBSERVER_H_
