// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/external_metrics.h"

#include "base/containers/fixed_flat_set.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/storage.pb.h"
#include "components/metrics/structured/structured_metrics_features.h"

namespace metrics::structured {
namespace {

void FilterEvents(
    google::protobuf::RepeatedPtrField<metrics::StructuredEventProto>* events,
    const base::flat_set<uint64_t>& disallowed_projects) {
  auto it = events->begin();
  while (it != events->end()) {
    if (disallowed_projects.contains(it->project_name_hash())) {
      it = events->erase(it);
    } else {
      ++it;
    }
  }
}

// TODO(b/181724341): Remove this once the bluetooth metrics are fully enabled.
void MaybeFilterBluetoothEvents(
    google::protobuf::RepeatedPtrField<metrics::StructuredEventProto>* events) {
  // Event name hashes of all bluetooth events listed in
  // src/platform2/metrics/structured/structured.xml.
  static constexpr auto kBluetoothEventHashes =
      base::MakeFixedFlatSet<uint64_t>(
          {// BluetoothAdapterStateChanged
           UINT64_C(959829856916771459),
           // BluetoothPairingStateChanged
           UINT64_C(11839023048095184048),
           // BluetoothAclConnectionStateChanged
           UINT64_C(1880220404408566268),
           // BluetoothProfileConnectionStateChanged
           UINT64_C(7217682640379679663),
           // BluetoothDeviceInfoReport
           UINT64_C(1506471670382892394)});

  if (base::FeatureList::IsEnabled(kBluetoothSessionizedMetrics)) {
    return;
  }

  // Remove all bluetooth events.
  auto it = events->begin();
  while (it != events->end()) {
    if (kBluetoothEventHashes.contains(it->event_name_hash())) {
      it = events->erase(it);
    } else {
      ++it;
    }
  }
}

bool FilterProto(EventsProto* proto,
                 const base::flat_set<uint64_t>& disallowed_projects) {
  FilterEvents(proto->mutable_uma_events(), disallowed_projects);
  FilterEvents(proto->mutable_non_uma_events(), disallowed_projects);
  return proto->uma_events_size() > 0 || proto->non_uma_events_size() > 0;
}

EventsProto ReadAndDeleteEvents(
    const base::FilePath& directory,
    const base::flat_set<uint64_t>& disallowed_projects,
    bool recording_enabled) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  EventsProto result;
  if (!base::DirectoryExists(directory)) {
    return result;
  }

  base::FileEnumerator enumerator(directory, false,
                                  base::FileEnumerator::FILES);
  int file_counter = 0;

  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    std::string proto_str;
    int64_t file_size;
    EventsProto proto;

    ++file_counter;

    // There may be too many messages in the directory to hold in-memory. This
    // could happen if the process in which Structured metrics resides is
    // either crash-looping or taking too long to process externally recorded
    // events.
    //
    // Events will be dropped in that case so that more recent events can be
    // processed. Events will be dropped if recording has been disabled.
    if (!recording_enabled || file_counter > GetFileLimitPerScan()) {
      base::DeleteFile(path);
      continue;
    }

    // If an event is abnormally large, ignore it to prevent OOM.
    bool fs_ok = base::GetFileSize(path, &file_size);

    // If file size get is successful, log the file size.
    if (fs_ok) {
      LogEventFileSizeKB(static_cast<int>(file_size / 1024));
    }

    if (!fs_ok || file_size > GetFileSizeByteLimit()) {
      base::DeleteFile(path);
      continue;
    }

    bool read_ok = base::ReadFileToString(path, &proto_str) &&
                   proto.ParseFromString(proto_str);
    base::DeleteFile(path);

    if (!read_ok) {
      continue;
    }

    if (!FilterProto(&proto, disallowed_projects)) {
      continue;
    }

    // MergeFrom performs a copy that could be a move if done manually. But
    // all the protos here are expected to be small, so let's keep it simple.
    result.mutable_uma_events()->MergeFrom(proto.uma_events());
    result.mutable_non_uma_events()->MergeFrom(proto.non_uma_events());
  }

  LogNumFilesPerExternalMetricsScan(file_counter);

  MaybeFilterBluetoothEvents(result.mutable_uma_events());
  MaybeFilterBluetoothEvents(result.mutable_non_uma_events());
  return result;
}

}  // namespace

ExternalMetrics::ExternalMetrics(const base::FilePath& events_directory,
                                 const base::TimeDelta& collection_interval,
                                 MetricsCollectedCallback callback)
    : events_directory_(events_directory),
      collection_interval_(collection_interval),
      callback_(std::move(callback)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  ScheduleCollector();
  CacheDisallowedProjectsSet();
}

ExternalMetrics::~ExternalMetrics() = default;

void ExternalMetrics::CollectEventsAndReschedule() {
  CollectEvents();
  ScheduleCollector();
}

void ExternalMetrics::ScheduleCollector() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExternalMetrics::CollectEventsAndReschedule,
                     weak_factory_.GetWeakPtr()),
      collection_interval_);
}

void ExternalMetrics::CollectEvents() {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadAndDeleteEvents, events_directory_,
                     disallowed_projects_, recording_enabled_),
      base::BindOnce(callback_));
}

void ExternalMetrics::CacheDisallowedProjectsSet() {
  const std::string& disallowed_list = GetDisabledProjects();
  if (disallowed_list.empty()) {
    return;
  }

  for (const auto& value :
       base::SplitString(disallowed_list, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    uint64_t project_name_hash;
    // Parse the string and keep only perfect conversions.
    if (base::StringToUint64(value, &project_name_hash)) {
      disallowed_projects_.insert(project_name_hash);
    }
  }
}

void ExternalMetrics::AddDisallowedProjectForTest(uint64_t project_name_hash) {
  disallowed_projects_.insert(project_name_hash);
}

void ExternalMetrics::EnableRecording() {
  recording_enabled_ = true;
}

void ExternalMetrics::DisableRecording() {
  recording_enabled_ = false;
}

}  // namespace metrics::structured
