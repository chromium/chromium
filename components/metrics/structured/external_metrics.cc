// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/external_metrics.h"

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/proto/event_storage.pb.h"
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

std::string_view Platform2ProjectName(uint64_t project_name_hash) {
  switch (project_name_hash) {
    case UINT64_C(827233605053062635):
      return "AudioPeripheral";
    case UINT64_C(524369188505453537):
      return "AudioPeripheralInfo";
    case UINT64_C(9074739597929991885):
      return "Bluetooth";
    case UINT64_C(1745381000935843040):
      return "BluetoothDevice";
    case UINT64_C(11181229631788078243):
      return "BluetoothChipset";
    case UINT64_C(8206859287963243715):
      return "Cellular";
    case UINT64_C(11294265225635075664):
      return "HardwareVerifier";
    case UINT64_C(4905803635010729907):
      return "RollbackEnterprise";
    case UINT64_C(9675127341789951965):
      return "Rmad";
    case UINT64_C(4690103929823698613):
      return "WiFiChipset";
    case UINT64_C(17922303533051575891):
      return "UsbDevice";
    case UINT64_C(1370722622176744014):
      return "UsbError";
    case UINT64_C(17319042894491683836):
      return "UsbPdDevice";
    case UINT64_C(6962789877417678651):
      return "UsbSession";
    case UINT64_C(4320592646346933548):
      return "WiFi";
    case UINT64_C(7302676440391025918):
      return "WiFiAP";
    default:
      return "UNKNOWN";
  }
}

void IncrementProjectCount(base::flat_map<uint64_t, int>& project_count_map,
                           uint64_t project_name_hash) {
  if (project_count_map.contains(project_name_hash)) {
    project_count_map[project_name_hash] += 1;
  } else {
    project_count_map[project_name_hash] = 1;
  }
}

void ProcessEventProtosProjectCounts(
    base::flat_map<uint64_t, int>& project_count_map,
    const EventsProto& proto) {
  // Process all events that were packed in the proto.
  for (const auto& event : proto.uma_events()) {
    IncrementProjectCount(project_count_map, event.project_name_hash());
  }

  for (const auto& event : proto.events()) {
    IncrementProjectCount(project_count_map, event.project_name_hash());
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
  FilterEvents(proto->mutable_events(), disallowed_projects);
  return proto->uma_events_size() > 0 || proto->events_size() > 0;
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

  base::DirReaderPosix dir_reader(directory.value().c_str());
  if (!dir_reader.IsValid()) {
    VLOG(2) << "Failed to load External Metrics directory: " << directory;
    return result;
  }

  int file_counter = 0;
  int dropped_events = 0;
  base::flat_map<uint64_t, int> dropped_projects_count, produced_projects_count;

  while (dir_reader.Next()) {
    base::FilePath path = directory.Append(dir_reader.name());
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_OPEN_ALWAYS);

    // This will fail on '.' and '..' files.
    if (!file.IsValid()) {
      continue;
    }

    // Ignore any directory.
    base::File::Info info;
    if (!file.GetInfo(&info) || info.is_directory) {
      continue;
    }

    // If recording is disabled delete the file.
    if (!recording_enabled) {
      base::DeleteFile(path);
      continue;
    }

    ++file_counter;

    std::string proto_str;
    int64_t file_size;
    EventsProto proto;

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

    // Process all events that were packed in the proto.
    ProcessEventProtosProjectCounts(produced_projects_count, proto);

    // There may be too many messages in the directory to hold in-memory.
    // This could happen if the process in which Structured metrics resides
    // is either crash-looping or taking too long to process externally
    // recorded events.
    //
    // Events will be dropped in that case so that more recent events can be
    // processed. Events will be dropped if recording has been disabled.
    if (file_counter > GetFileLimitPerScan()) {
      ++dropped_events;

      // Process all events that were packed in the proto.
      ProcessEventProtosProjectCounts(dropped_projects_count, proto);
      continue;
    }

    if (!read_ok) {
      continue;
    }

    if (!FilterProto(&proto, disallowed_projects)) {
      continue;
    }

    // MergeFrom performs a copy that could be a move if done manually. But
    // all the protos here are expected to be small, so let's keep it simple.
    result.mutable_uma_events()->MergeFrom(proto.uma_events());
    result.mutable_events()->MergeFrom(proto.events());
  }

  if (recording_enabled) {
    LogDroppedExternalMetrics(dropped_events);

    // Log histograms for each project with their appropriate counts.
    // If a project isn't seen then it will not be logged.
    for (const auto& project_counts : produced_projects_count) {
      LogProducedProjectExternalMetrics(
          Platform2ProjectName(project_counts.first), project_counts.second);
    }

    for (const auto& project_counts : dropped_projects_count) {
      LogDroppedProjectExternalMetrics(
          Platform2ProjectName(project_counts.first), project_counts.second);
    }
  }

  LogNumFilesPerExternalMetricsScan(file_counter);

  MaybeFilterBluetoothEvents(result.mutable_uma_events());
  MaybeFilterBluetoothEvents(result.mutable_events());

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
