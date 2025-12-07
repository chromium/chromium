// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/app/cast_crash_uploader_default.h"
#include <memory>

namespace chromecast {

// static
std::unique_ptr<CastCrashUploader> CastCrashUploader::Create(
    PrefService* pref_service) {
  return std::make_unique<CastCrashUploaderDefault>();
}
void CastCrashUploaderDefault::UploadDumps(
    const std::string& uuid,
    const std::string& application_feedback,
    const bool can_send_usage_stats) {
  NOTREACHED() << "TODO(b/258269114): Move non-android implementations of "
                  "crash reporting here.";
}
}  // namespace chromecast
