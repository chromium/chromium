// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_user_data.h"

#include <memory>

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

#include "net/url_request/url_fetcher.h"

namespace data_use_measurement {

DataUseUserData::DataUseUserData(AppState app_state)
    : app_state_(app_state), content_type_(DataUseContentType::OTHER) {}

DataUseUserData::~DataUseUserData() {}

// static
const void* const DataUseUserData::kUserDataKey =
    &DataUseUserData::kUserDataKey;

}  // namespace data_use_measurement
