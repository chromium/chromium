// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_APP_CAST_CRASH_UPLOADER_H_
#define CHROMECAST_APP_CAST_CRASH_UPLOADER_H_

#include <memory>

#include "components/prefs/pref_service.h"

namespace chromecast {
class CastCrashUploader {
 public:
  static std::unique_ptr<CastCrashUploader> Create(PrefService*);
  virtual ~CastCrashUploader() = default;

  virtual void UploadDumps(const std::string& uuid,
                           const std::string& application_feedback,
                           const bool can_send_usage_stats) = 0;
};
}  // namespace chromecast

#endif  // CHROMECAST_APP_CAST_CRASH_UPLOADER_H_
