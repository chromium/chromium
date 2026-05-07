// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/recording_data_manager_impl.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/browser/task_database.h"
#include "components/record_replay/core/common/record_replay_features.h"
#include "components/record_replay/core/common/record_replay_switches.h"

namespace record_replay {

namespace {

base::FilePath GetSeedingFilePath() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kActivityMetadataFile)) {
    return command_line->GetSwitchValuePath(switches::kActivityMetadataFile);
  }
  return base::FilePath();
}

}  // namespace

RecordingDataManagerImpl::RecordingDataManagerImpl(base::FilePath profile_path)
    : db_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           // CRITICAL: MUST use BLOCK_SHUTDOWN. SQLite database writes and
           // transactions must complete fully before shutdown to prevent
           // database corruption. CONTINUE_ON_SHUTDOWN would allow the thread
           // pool to kill the thread mid-write.
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  // Intent: Asynchronously initialize the database. Subsequent calls are safely
  // queued on the same sequenced task runner and will execute after Init
  // completes.
  db_.AsyncCall(&TaskDatabase::Init).WithArgs(std::move(profile_path));

  // Trigger asynchronous seeding by retrieving the local file path (if
  // specified) and the Feature configuration, prioritizing the file.
  db_.AsyncCall(&TaskDatabase::RunSeeding)
      .WithArgs(GetSeedingFilePath(),
                features::kRecordReplayAnnotationSeed.Get());
}

RecordingDataManagerImpl::~RecordingDataManagerImpl() = default;

void RecordingDataManagerImpl::AddRecording(
    Recording recording,
    base::OnceCallback<void(int64_t)> callback) {
  db_.AsyncCall(&TaskDatabase::AddRecording)
      .WithArgs(std::move(recording))
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::GetRecordingsByUrl(
    std::string url,
    base::OnceCallback<void(std::vector<Recording>)> callback) {
  db_.AsyncCall(&TaskDatabase::GetRecordingsByUrl)
      .WithArgs(std::move(url))
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::SaveActivityAnnotation(
    std::optional<int64_t> annotation_id,
    ActivityAnnotation annotation,
    std::string target_url,
    std::optional<int64_t> recording_id,
    base::OnceClosure callback) {
  db_.AsyncCall(&TaskDatabase::SaveActivityAnnotation)
      .WithArgs(annotation_id, std::move(annotation), std::move(target_url),
                recording_id)
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::GetActivityAnnotation(
    int64_t annotation_id,
    base::OnceCallback<void(std::optional<ActivityAnnotation>)> callback) {
  db_.AsyncCall(&TaskDatabase::GetActivityAnnotation)
      .WithArgs(annotation_id)
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::GetActivityAnnotationsByUrl(
    std::string url,
    base::OnceCallback<
        void(std::vector<std::pair<int64_t, ActivityAnnotation>>)> callback) {
  db_.AsyncCall(&TaskDatabase::GetActivityAnnotationsByUrl)
      .WithArgs(std::move(url))
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::SaveActivityData(
    int64_t annotation_id,
    ActivityData data,
    base::OnceCallback<void(bool)> callback) {
  db_.AsyncCall(&TaskDatabase::SaveActivityData)
      .WithArgs(annotation_id, std::move(data))
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::GetActivityData(
    int64_t annotation_id,
    base::OnceCallback<void(std::optional<ActivityData>)> callback) {
  db_.AsyncCall(&TaskDatabase::GetActivityData)
      .WithArgs(annotation_id)
      .Then(std::move(callback));
}

void RecordingDataManagerImpl::DeleteActivityData(
    int64_t annotation_id,
    base::OnceCallback<void(bool)> callback) {
  db_.AsyncCall(&TaskDatabase::DeleteActivityData)
      .WithArgs(annotation_id)
      .Then(std::move(callback));
}

}  // namespace record_replay
