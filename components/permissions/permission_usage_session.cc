// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_usage_session.h"

#include <tuple>

namespace permissions {

bool PermissionUsageSession::IsValid() const {
  return !(origin.opaque() || usage_start.is_null() || usage_end.is_null() ||
           usage_end < usage_start);
}

}  // namespace permissions
