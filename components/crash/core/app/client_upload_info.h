// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_CLIENT_UPLOAD_INFO_H_
#define COMPONENTS_CRASH_CORE_APP_CLIENT_UPLOAD_INFO_H_

#include "components/crash/core/app/crash_reporter_client.h"

namespace crash_reporter {

// Returns whether the user has consented to collecting stats.
// This may block. Use a task with base::MayBlock() to call this function.
bool GetClientCollectStatsConsent();

// Returns a textual description of the product type, version and channel
// to include in crash reports.
void GetClientProductInfo(ProductInfo* product_info);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_APP_CLIENT_UPLOAD_INFO_H_
