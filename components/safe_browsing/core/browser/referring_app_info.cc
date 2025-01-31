// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/referring_app_info.h"

namespace safe_browsing::internal {

ReferringAppInfo::ReferringAppInfo() = default;

ReferringAppInfo::~ReferringAppInfo() = default;

ReferringAppInfo::ReferringAppInfo(const ReferringAppInfo&) = default;

ReferringAppInfo::ReferringAppInfo(ReferringAppInfo&&) = default;

ReferringAppInfo& ReferringAppInfo::operator=(const ReferringAppInfo&) =
    default;

ReferringAppInfo& ReferringAppInfo::operator=(ReferringAppInfo&&) = default;

}  // namespace safe_browsing::internal
