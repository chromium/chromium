// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_FUCHSIA_CAST_CRASH_STORAGE_IMPL_FUCHSIA_H_
#define CHROMECAST_CRASH_FUCHSIA_CAST_CRASH_STORAGE_IMPL_FUCHSIA_H_

#include <fuchsia/feedback/cpp/fidl.h>
#include <lib/sys/cpp/service_directory.h>

#include <string_view>

#include "chromecast/crash/cast_crash_storage.h"

namespace chromecast {

class CastCrashStorageImplFuchsia final : public CastCrashStorage {
 public:
  explicit CastCrashStorageImplFuchsia(
      const sys::ServiceDirectory* incoming_directory);
  ~CastCrashStorageImplFuchsia() override;
  CastCrashStorageImplFuchsia& operator=(const CastCrashStorageImplFuchsia&) =
      delete;
  CastCrashStorageImplFuchsia(const CastCrashStorageImplFuchsia&) = delete;

  // CastCrashStorage implementation:
  void SetLastLaunchedApp(std::string_view app_id) override;
  void ClearLastLaunchedApp() override;
  void SetCurrentApp(std::string_view app_id) override;
  void ClearCurrentApp() override;
  void SetPreviousApp(std::string_view app_id) override;
  void ClearPreviousApp() override;
  void SetStadiaSessionId(std::string_view session_id) override;
  void ClearStadiaSessionId() override;

 private:
  void UpsertAnnotations(
      std::vector<fuchsia::feedback::Annotation> annotations);

  const sys::ServiceDirectory* const incoming_directory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_FUCHSIA_CAST_CRASH_STORAGE_IMPL_FUCHSIA_H_
