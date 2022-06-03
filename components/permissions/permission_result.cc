// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_result.h"

namespace permissions {

PermissionResult::PermissionResult(ContentSetting cs,
                                   PermissionStatusSource pss)
    : content_setting(cs), source(pss) {}

PermissionResult::~PermissionResult() {}

}  // namespace permissions
