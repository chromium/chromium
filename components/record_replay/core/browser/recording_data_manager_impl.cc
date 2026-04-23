// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/recording_data_manager_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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

void RecordingDataManagerImpl::AddRecording(
    Recording recording,
    base::OnceCallback<void(int64_t)> callback) {
  db_.AsyncCall(&CapabilitiesDatabase::AddRecording)
      .WithArgs(std::move(recording))
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::GetRecordingsByUrl(
    std::string url,
    base::OnceCallback<void(std::vector<Recording>)> callback) {
  db_.AsyncCall(&CapabilitiesDatabase::GetRecordingsByUrl)
      .WithArgs(std::move(url))
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::SaveActivityAnnotation(
    std::optional<int64_t> annotation_id,
    ActivityAnnotation annotation,
    std::string target_url,
    std::optional<int64_t> recording_id,
    base::OnceClosure callback) {
  db_.AsyncCall(&CapabilitiesDatabase::SaveActivityAnnotation)
      .WithArgs(annotation_id, std::move(annotation), std::move(target_url),
                recording_id)
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::GetActivityAnnotation(
    int64_t annotation_id,
    base::OnceCallback<void(std::optional<ActivityAnnotation>)> callback) {
  db_.AsyncCall(&CapabilitiesDatabase::GetActivityAnnotation)
      .WithArgs(annotation_id)
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::GetActivityAnnotationsByUrl(
    std::string url,
    base::OnceCallback<
        void(std::vector<std::pair<int64_t, ActivityAnnotation>>)> callback) {
  db_.AsyncCall(&CapabilitiesDatabase::GetActivityAnnotationsByUrl)
      .WithArgs(std::move(url))
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::SaveActivityData(
    int64_t annotation_id,
    ActivityData data,
    base::OnceCallback<void(bool)> callback) {
  db_.AsyncCall(&CapabilitiesDatabase::SaveActivityData)
      .WithArgs(annotation_id, std::move(data))
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::GetActivityData(
    int64_t annotation_id,
    base::OnceCallback<void(std::optional<ActivityData>)> callback) {
  db_.AsyncCall(&CapabilitiesDatabase::GetActivityData)
      .WithArgs(annotation_id)
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::DeleteActivityData(
    int64_t annotation_id,
    base::OnceCallback<void(bool)> callback) {
  db_.AsyncCall(&CapabilitiesDatabase::DeleteActivityData)
      .WithArgs(annotation_id)
      .Then(std::move(callback));
}

}  // namespace record_replay
