// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_APP_CAST_CRASH_UPLOADER_DEFAULT_H_
#define CHROMECAST_APP_CAST_CRASH_UPLOADER_DEFAULT_H_

#include "chromecast/app/cast_crash_uploader.h"
#include "components/prefs/pref_service.h"

namespace chromecast {
class CastCrashUploaderDefault : public CastCrashUploader {
 public:
  static CastCrashUploader* Create(PrefService*);
  void UploadDumps(const std::string& uuid,
                   const std::string& application_feedback,
                   const bool can_send_usage_stats) override;
};
}  // namespace chromecast

#endif  // CHROMECAST_APP_CAST_CRASH_UPLOADER_DEFAULT_H_
