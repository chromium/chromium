// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_ENVIRONMENT_RECORDER_H_
#define COMPONENTS_METRICS_ENVIRONMENT_RECORDER_H_

#include <string>

#include "base/macros.h"

class PrefService;
class PrefRegistrySimple;

namespace metrics {

class SystemProfileProto;

// Stores system profile information to prefs for creating stability logs
// in the next launch of chrome, and reads data from previous launches.
class EnvironmentRecorder {
 public:
  explicit EnvironmentRecorder(PrefService* local_state);
  ~EnvironmentRecorder();

  // Serializes the system profile and records it in prefs for the next
  // session.  Returns the uncompressed serialized proto for passing to crash
  // reports, or the empty string if the proto can't be serialized.
  std::string SerializeAndRecordEnvironmentToPrefs(
      const SystemProfileProto& system_profile);

  // Loads the system_profile data stored in a previous chrome session, and
  // stores it in the |system_profile| object.
  // Returns true iff a system profile was successfully read.
  bool LoadEnvironmentFromPrefs(SystemProfileProto* system_profile);

  // Deletes system profile data from prefs.
  void ClearEnvironmentFromPrefs();

  // Stores the buildtime of the current binary and version in prefs.
  void SetBuildtimeAndVersion(int64_t buildtime, const std::string& version);

  // Gets the buildtime stored in prefs.
  int64_t GetLastBuildtime();

  // Gets the version stored in prefs.
  std::string GetLastVersion();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(EnvironmentRecorder);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_ENVIRONMENT_RECORDER_H_
