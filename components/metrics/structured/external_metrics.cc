// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/external_metrics.h"

#include <sys/file.h>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/current_thread.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/metrics/structured/storage.pb.h"

namespace metrics {
namespace structured {
namespace {

EventsProto ReadAndDeleteEvents(const base::FilePath& directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  EventsProto result;
  if (!base::DirectoryExists(directory))
    return result;

  base::FileEnumerator enumerator(directory, false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    std::string proto_str;
    EventsProto proto;

    // We may try to read a file as it's being written by cros. To avoid this,
    // cros locks each file exclusively before writing. Check we can get a
    // shared lock for reading, and otherwise ignore the file. Note these are
    // advisory POSIX locks. We don't actually use the file object for reading.
    static const uint32_t open_flags =
        base::File::FLAG_OPEN | base::File::FLAG_READ;
    base::File file(path, open_flags);
    if (file.Lock(base::File::LockMode::kShared) != base::File::FILE_OK)
      continue;

    bool read_ok = base::ReadFileToString(path, &proto_str) &&
                   proto.ParseFromString(proto_str);
    base::DeleteFile(path);
    file.Unlock();

    if (!read_ok)
      continue;

    // MergeFrom performs a copy that could be a move if done manually. But all
    // the protos here are expected to be small, so let's keep it simple.
    result.mutable_uma_events()->MergeFrom(*proto.mutable_uma_events());
    result.mutable_non_uma_events()->MergeFrom(*proto.mutable_non_uma_events());
  }

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
}

ExternalMetrics::~ExternalMetrics() = default;

void ExternalMetrics::CollectEventsAndReschedule() {
  CollectEvents();
  ScheduleCollector();
}

void ExternalMetrics::ScheduleCollector() {
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ExternalMetrics::CollectEventsAndReschedule,
                     weak_factory_.GetWeakPtr()),
      collection_interval_);
}

void ExternalMetrics::CollectEvents() {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadAndDeleteEvents, events_directory_),
      base::BindOnce(&ExternalMetrics::OnEventsCollected,
                     weak_factory_.GetWeakPtr()));
}

void ExternalMetrics::OnEventsCollected(EventsProto events) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback_, std::move(events)));
}

}  // namespace structured
}  // namespace metrics
