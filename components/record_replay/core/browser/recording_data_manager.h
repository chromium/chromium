// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDING_DATA_MANAGER_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDING_DATA_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/record_replay/core/browser/recording.pb.h"

namespace record_replay {

// Manages persistent storage for recording protos and inferred capabilities.
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
  virtual void AddRecording(Recording recording,
                            base::OnceCallback<void(int64_t)> callback) = 0;

  // Retrieves every Recording that matches the given `url`.
  virtual void GetRecordingsByUrl(
      std::string url,
      base::OnceCallback<void(std::vector<Recording>)> callback) = 0;

  // Handles both insertion (when annotation_id is nullopt) and updates.
  virtual void SaveActivityAnnotation(std::optional<int64_t> annotation_id,
                                      ActivityAnnotation annotation,
                                      std::string target_url,
                                      std::optional<int64_t> recording_id,
                                      base::OnceClosure callback) = 0;

  // Retrieves the annotation for a given ID, if it exists.
  virtual void GetActivityAnnotation(
      int64_t annotation_id,
      base::OnceCallback<void(std::optional<ActivityAnnotation>)> callback) = 0;

  // Retrieves all annotations for a site, returning their IDs and proto data.
  virtual void GetActivityAnnotationsByUrl(
      std::string url,
      base::OnceCallback<void(
          std::vector<std::pair<int64_t, ActivityAnnotation>>)> callback) = 0;
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDING_DATA_MANAGER_H_
