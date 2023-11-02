// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_CAST_CRASH_STORAGE_H_
#define CHROMECAST_CRASH_CAST_CRASH_STORAGE_H_

#include "base/strings/string_piece.h"

namespace chromecast {

// Stores crash key annotations.
// This is used to provide indirection for platform dependent storage
// implementations.
class CastCrashStorage {
 public:
  static CastCrashStorage* GetInstance();

  CastCrashStorage() = default;
  virtual ~CastCrashStorage() = default;

  virtual void SetLastLaunchedApp(base::StringPiece app_id) = 0;
  virtual void ClearLastLaunchedApp() = 0;

  virtual void SetCurrentApp(base::StringPiece app_id) = 0;
  virtual void ClearCurrentApp() = 0;

  virtual void SetPreviousApp(base::StringPiece app_id) = 0;
  virtual void ClearPreviousApp() = 0;

  virtual void SetStadiaSessionId(base::StringPiece session_id) = 0;
  virtual void ClearStadiaSessionId() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_CAST_CRASH_STORAGE_H_
