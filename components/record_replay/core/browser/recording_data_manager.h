// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDING_DATA_MANAGER_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDING_DATA_MANAGER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/record_replay/core/browser/recording.pb.h"

namespace record_replay {

// Manages persistent storage for recording protos.
//
// Tied to the lifecycle of a `Profile`.
class RecordingDataManager : public KeyedService {
 public:
  RecordingDataManager() = default;
  RecordingDataManager(const RecordingDataManager&) = delete;
  RecordingDataManager& operator=(const RecordingDataManager&) = delete;
  RecordingDataManager(RecordingDataManager&&) = delete;
  RecordingDataManager& operator=(RecordingDataManager&&) = delete;
  ~RecordingDataManager() override = default;

  // Adds a recording to the database.
  virtual void AddRecording(Recording recording) = 0;

  // Retrieves every Recording that matches the given `url`.
  virtual void GetRecordingsByUrl(
      std::string url,
      base::OnceCallback<void(std::vector<Recording>)> callback) = 0;
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDING_DATA_MANAGER_H_
