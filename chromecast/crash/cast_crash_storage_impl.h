// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_CAST_CRASH_STORAGE_IMPL_H_
#define CHROMECAST_CRASH_CAST_CRASH_STORAGE_IMPL_H_

#include "chromecast/crash/cast_crash_storage.h"

namespace chromecast {

class CastCrashStorageImpl final : public CastCrashStorage {
 public:
  CastCrashStorageImpl();
  ~CastCrashStorageImpl() override;
  CastCrashStorageImpl& operator=(const CastCrashStorageImpl&) = delete;
  CastCrashStorageImpl(const CastCrashStorageImpl&) = delete;

  // CastCrashStorage implementation:
  void SetLastLaunchedApp(base::StringPiece app_id) override;
  void ClearLastLaunchedApp() override;
  void SetCurrentApp(base::StringPiece app_id) override;
  void ClearCurrentApp() override;
  void SetPreviousApp(base::StringPiece app_id) override;
  void ClearPreviousApp() override;
  void SetStadiaSessionId(base::StringPiece session_id) override;
  void ClearStadiaSessionId() override;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_CAST_CRASH_STORAGE_IMPL_H_
