// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/public/cpp/android_app_info.h"

namespace chromeos {
namespace assistant {

AndroidAppInfo::AndroidAppInfo() = default;
AndroidAppInfo::AndroidAppInfo(const AndroidAppInfo& suggestion) = default;
AndroidAppInfo& AndroidAppInfo::operator=(const AndroidAppInfo&) = default;
AndroidAppInfo::AndroidAppInfo(AndroidAppInfo&& suggestion) = default;
AndroidAppInfo& AndroidAppInfo::operator=(AndroidAppInfo&&) = default;
AndroidAppInfo::~AndroidAppInfo() = default;

}  // namespace assistant
}  // namespace chromeos
