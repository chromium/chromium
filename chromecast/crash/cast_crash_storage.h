// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_CAST_CRASH_STORAGE_H_
#define CHROMECAST_CRASH_CAST_CRASH_STORAGE_H_

#include <string_view>

namespace chromecast {

// Stores crash key annotations.
// This is used to provide indirection for platform dependent storage
// implementations.
class CastCrashStorage {
 public:
  static CastCrashStorage* GetInstance();

  CastCrashStorage() = default;
  virtual ~CastCrashStorage() = default;

  virtual void SetLastLaunchedApp(std::string_view app_id) = 0;
  virtual void ClearLastLaunchedApp() = 0;

  virtual void SetCurrentApp(std::string_view app_id) = 0;
  virtual void ClearCurrentApp() = 0;

  virtual void SetPreviousApp(std::string_view app_id) = 0;
  virtual void ClearPreviousApp() = 0;

  virtual void SetStadiaSessionId(std::string_view session_id) = 0;
  virtual void ClearStadiaSessionId() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_CAST_CRASH_STORAGE_H_
