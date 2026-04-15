// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/recording_data_manager_impl.h"

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "components/record_replay/core/browser/capabilities_database.h"
#include "components/record_replay/core/browser/recording.pb.h"

namespace record_replay {

RecordingDataManagerImpl::RecordingDataManagerImpl(base::FilePath profile_path)
    : db_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  db_.AsyncCall(&CapabilitiesDatabase::Init).WithArgs(std::move(profile_path));
}

RecordingDataManagerImpl::~RecordingDataManagerImpl() = default;

void RecordingDataManagerImpl::AddRecording(Recording recording) {
  db_.AsyncCall(&CapabilitiesDatabase::AddRecording)
      .WithArgs(std::move(recording));
}

void RecordingDataManagerImpl::GetRecordingsByUrl(
    std::string url,
    base::OnceCallback<void(std::vector<Recording>)> callback) {
  db_.AsyncCall(&CapabilitiesDatabase::GetRecordingsByUrl)
      .WithArgs(std::move(url))
      .Then(std::move(callback));
}

}  // namespace record_replay
