// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/cast_crash_storage_impl.h"

#include <string_view>

#include "chromecast/crash/cast_crash_keys.h"
#include "components/crash/core/common/crash_key.h"

namespace chromecast {
namespace {

crash_reporter::CrashKeyString<64> last_app(crash_keys::kLastApp);
crash_reporter::CrashKeyString<64> current_app(crash_keys::kCurrentApp);
crash_reporter::CrashKeyString<64> previous_app(crash_keys::kPreviousApp);
crash_reporter::CrashKeyString<64> stadia_session_id(
    crash_keys::kStadiaSessionId);

}  // namespace

CastCrashStorageImpl::CastCrashStorageImpl() = default;
CastCrashStorageImpl::~CastCrashStorageImpl() = default;

void CastCrashStorageImpl::SetLastLaunchedApp(std::string_view app_id) {
  last_app.Set(app_id);
}

void CastCrashStorageImpl::ClearLastLaunchedApp() {
  last_app.Clear();
}

void CastCrashStorageImpl::SetCurrentApp(std::string_view app_id) {
  current_app.Set(app_id);
}

void CastCrashStorageImpl::ClearCurrentApp() {
  current_app.Clear();
}

void CastCrashStorageImpl::SetPreviousApp(std::string_view app_id) {
  previous_app.Set(app_id);
}

void CastCrashStorageImpl::ClearPreviousApp() {
  previous_app.Clear();
}

void CastCrashStorageImpl::SetStadiaSessionId(std::string_view session_id) {
  stadia_session_id.Set(session_id);
}

void CastCrashStorageImpl::ClearStadiaSessionId() {
  stadia_session_id.Clear();
}

}  // namespace chromecast
