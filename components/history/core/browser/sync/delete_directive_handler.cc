// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/delete_directive_handler.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/protocol/history_delete_directive_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync.pb.h"

namespace {

std::string RandASCIIString(size_t length) {
  std::string result;
  const int kMin = static_cast<int>(' ');
  const int kMax = static_cast<int>('~');
  for (size_t i = 0; i < length; ++i)
    result.push_back(static_cast<char>(base::RandInt(kMin, kMax)));
  return result;
}

std::string DeleteDirectiveToString(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  std::unique_ptr<base::DictionaryValue> value(
      syncer::HistoryDeleteDirectiveSpecificsToValue(delete_directive));
  std::string str;
  base::JSONWriter::Write(*value, &str);
  return str;
}

// Compare time range directives first by start time, then by end time.
bool TimeRangeLessThan(const syncer::SyncData& data1,
                       const syncer::SyncData& data2) {
  const sync_pb::TimeRangeDirective& range1 =
      data1.GetSpecifics().history_delete_directive().time_range_directive();
  const sync_pb::TimeRangeDirective& range2 =
      data2.GetSpecifics().history_delete_directive().time_range_directive();
  if (range1.start_time_usec() < range2.start_time_usec())
    return true;
  if (range1.start_time_usec() > range2.start_time_usec())
    return false;
  return range1.end_time_usec() < range2.end_time_usec();
}

// Converts a Unix timestamp in microseconds to a base::Time value.
base::Time UnixUsecToTime(int64_t usec) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(usec);
}

// Converts a base::Time value to a Unix timestamp in microseconds.
int64_t TimeToUnixUsec(base::Time time) {
  DCHECK(!time.is_null());
  return (time - base::Time::UnixEpoch()).InMicroseconds();
}

// Converts global IDs in |global_id_directive| to times.
void GetTimesFromGlobalIds(
    const sync_pb::GlobalIdDirective& global_id_directive,
    std::set<base::Time>* times) {
  for (int i = 0; i < global_id_directive.global_id_size(); ++i) {
    times->insert(
        base::Time::FromInternalValue(global_id_directive.global_id(i)));
  }
}

#if !defined(NDEBUG)
// Checks that the given delete directive is properly formed.
void CheckDeleteDirectiveValid(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  if (delete_directive.has_global_id_directive()) {
    const sync_pb::GlobalIdDirective& global_id_directive =
        delete_directive.global_id_directive();

    DCHECK(!delete_directive.has_time_range_directive());
    DCHECK(!delete_directive.has_url_directive());
    DCHECK_NE(global_id_directive.global_id_size(), 0);
    if (global_id_directive.has_start_time_usec())
      DCHECK_GE(global_id_directive.start_time_usec(), 0);
    if (global_id_directive.has_end_time_usec()) {
      DCHECK_GT(global_id_directive.end_time_usec(), 0);

      if (global_id_directive.has_start_time_usec()) {
        DCHECK_LE(global_id_directive.start_time_usec(),
                  global_id_directive.end_time_usec());
      }
    }

  } else if (delete_directive.has_time_range_directive()) {
    const sync_pb::TimeRangeDirective& time_range_directive =
        delete_directive.time_range_directive();

    DCHECK(!delete_directive.has_global_id_directive());
    DCHECK(!delete_directive.has_url_directive());
    DCHECK(time_range_directive.has_start_time_usec());
    DCHECK(time_range_directive.has_end_time_usec());
    DCHECK_GE(time_range_directive.start_time_usec(), 0);
    DCHECK_GT(time_range_directive.end_time_usec(), 0);
    DCHECK_GT(time_range_directive.end_time_usec(),
              time_range_directive.start_time_usec());
  } else if (delete_directive.has_url_directive()) {
    const sync_pb::UrlDirective& url_directive =
        delete_directive.url_directive();

    DCHECK(!delete_directive.has_global_id_directive());
    DCHECK(!delete_directive.has_time_range_directive());
    DCHECK(url_directive.has_url());
    DCHECK_GT(url_directive.end_time_usec(), 0);
  } else {
    NOTREACHED()
        << "Delete directive has no time range, global ID or url directive";
  }
}
#endif  // !defined(NDEBUG)

}  // anonymous namespace

namespace history {

class DeleteDirectiveHandler::DeleteDirectiveTask : public HistoryDBTask {
 public:
  DeleteDirectiveTask(
      base::WeakPtr<DeleteDirectiveHandler> delete_directive_handler,
      const syncer::SyncDataList& delete_directive,
      DeleteDirectiveHandler::PostProcessingAction post_processing_action)
      : delete_directive_handler_(delete_directive_handler),
        delete_directives_(delete_directive),
        post_processing_action_(post_processing_action) {}

  ~DeleteDirectiveTask() override {}

  // Implements HistoryDBTask.
  bool RunOnDBThread(HistoryBackend* backend, HistoryDatabase* db) override;
  void DoneRunOnMainThread() override;

 private:
  // Process a list of global Id directives. Delete all visits to a URL in
  // time ranges of directives if the timestamp of one visit matches with one
  // global id.
  void ProcessGlobalIdDeleteDirectives(
      HistoryBackend* history_backend,
      const syncer::SyncDataList& global_id_directives);

  // Process a list of time range directives, all history entries within the
  // time ranges are deleted. |time_range_directives| should be sorted by
  // |start_time_usec| and |end_time_usec| already.
  void ProcessTimeRangeDeleteDirectives(
      HistoryBackend* history_backend,
      const syncer::SyncDataList& time_range_directives);

  // Process a list of url directives, all history entries matching the
  // urls are deleted.
  void ProcessUrlDeleteDirectives(HistoryBackend* history_backend,
                                  const syncer::SyncDataList& url_directives);

  base::WeakPtr<DeleteDirectiveHandler> delete_directive_handler_;
  syncer::SyncDataList delete_directives_;
  DeleteDirectiveHandler::PostProcessingAction post_processing_action_;
};

bool DeleteDirectiveHandler::DeleteDirectiveTask::RunOnDBThread(
    HistoryBackend* backend,
    HistoryDatabase* db) {
  syncer::SyncDataList global_id_directives;
  syncer::SyncDataList time_range_directives;
  syncer::SyncDataList url_directives;
  for (const auto& sync_data : delete_directives_) {
    DCHECK_EQ(sync_data.GetDataType(), syncer::HISTORY_DELETE_DIRECTIVES);
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive =
        sync_data.GetSpecifics().history_delete_directive();
    if (delete_directive.has_global_id_directive()) {
      global_id_directives.push_back(sync_data);
    } else if (delete_directive.has_time_range_directive()) {
      time_range_directives.push_back(sync_data);
    } else if (delete_directive.has_url_directive()) {
      url_directives.push_back(sync_data);
    }
  }

  ProcessGlobalIdDeleteDirectives(backend, global_id_directives);
  std::sort(time_range_directives.begin(), time_range_directives.end(),
            TimeRangeLessThan);
  ProcessTimeRangeDeleteDirectives(backend, time_range_directives);
  ProcessUrlDeleteDirectives(backend, url_directives);
  return true;
}

void DeleteDirectiveHandler::DeleteDirectiveTask::DoneRunOnMainThread() {
  if (delete_directive_handler_) {
    delete_directive_handler_->FinishProcessing(post_processing_action_,
                                                delete_directives_);
  }
}

void DeleteDirectiveHandler::DeleteDirectiveTask::
    ProcessGlobalIdDeleteDirectives(
        HistoryBackend* history_backend,
        const syncer::SyncDataList& global_id_directives) {
  if (global_id_directives.empty())
    return;

  // Group times represented by global IDs by time ranges of delete directives.
  // It's more efficient for backend to process all directives with same time
  // range at once.
  typedef std::map<std::pair<base::Time, base::Time>, std::set<base::Time>>
      GlobalIdTimesGroup;
  GlobalIdTimesGroup id_times_group;
  for (size_t i = 0; i < global_id_directives.size(); ++i) {
    DVLOG(1) << "Processing delete directive: "
             << DeleteDirectiveToString(global_id_directives[i]
                                            .GetSpecifics()
                                            .history_delete_directive());

    const sync_pb::GlobalIdDirective& id_directive =
        global_id_directives[i]
            .GetSpecifics()
            .history_delete_directive()
            .global_id_directive();
    if (id_directive.global_id_size() == 0 ||
        !id_directive.has_start_time_usec() ||
        !id_directive.has_end_time_usec()) {
      DLOG(ERROR) << "Invalid global id directive.";
      continue;
    }
    GetTimesFromGlobalIds(id_directive,
                          &id_times_group[std::make_pair(
                              UnixUsecToTime(id_directive.start_time_usec()),
                              UnixUsecToTime(id_directive.end_time_usec()))]);
  }

  if (id_times_group.empty())
    return;

  // Call backend to expire history of directives in each group.
  for (GlobalIdTimesGroup::const_iterator group_it = id_times_group.begin();
       group_it != id_times_group.end(); ++group_it) {
    // Add 1us to cover history entries visited at the end time because time
    // range in directive is inclusive.
    history_backend->ExpireHistoryForTimes(
        group_it->second, group_it->first.first,
        group_it->first.second + base::TimeDelta::FromMicroseconds(1));
  }
}

void DeleteDirectiveHandler::DeleteDirectiveTask::
    ProcessTimeRangeDeleteDirectives(
        HistoryBackend* history_backend,
        const syncer::SyncDataList& time_range_directives) {
  if (time_range_directives.empty())
    return;

  // Iterate through time range directives. Expire history in combined
  // time range for multiple directives whose time ranges overlap.
  base::Time current_start_time;
  base::Time current_end_time;
  for (size_t i = 0; i < time_range_directives.size(); ++i) {
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive =
        time_range_directives[i].GetSpecifics().history_delete_directive();
    DVLOG(1) << "Processing time range directive: "
             << DeleteDirectiveToString(delete_directive);

    const sync_pb::TimeRangeDirective& time_range_directive =
        delete_directive.time_range_directive();
    if (!time_range_directive.has_start_time_usec() ||
        !time_range_directive.has_end_time_usec() ||
        time_range_directive.start_time_usec() >=
            time_range_directive.end_time_usec()) {
      DLOG(ERROR) << "Invalid time range directive.";
      continue;
    }

    base::Time directive_start_time =
        UnixUsecToTime(time_range_directive.start_time_usec());
    base::Time directive_end_time =
        UnixUsecToTime(time_range_directive.end_time_usec());
    if (directive_start_time > current_end_time) {
      if (!current_start_time.is_null()) {
        // Add 1us to cover history entries visited at the end time because
        // time range in directive is inclusive.
        history_backend->ExpireHistoryBetween(
            std::set<GURL>(), current_start_time,
            current_end_time + base::TimeDelta::FromMicroseconds(1),
            /*user_initiated*/ true);
      }
      current_start_time = directive_start_time;
    }
    if (directive_end_time > current_end_time)
      current_end_time = directive_end_time;
  }

  if (!current_start_time.is_null()) {
    history_backend->ExpireHistoryBetween(
        std::set<GURL>(), current_start_time,
        current_end_time + base::TimeDelta::FromMicroseconds(1),
        /*user_initiated*/ true);
  }
}

void DeleteDirectiveHandler::DeleteDirectiveTask::ProcessUrlDeleteDirectives(
    HistoryBackend* history_backend,
    const syncer::SyncDataList& url_directives) {
  std::vector<std::pair<GURL, base::Time>> deletions;
  for (const auto& sync_data : url_directives) {
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive =
        sync_data.GetSpecifics().history_delete_directive();
    DVLOG(1) << "Processing url directive: "
             << DeleteDirectiveToString(delete_directive);
    const sync_pb::UrlDirective& url_directive =
        delete_directive.url_directive();

    if (!url_directive.has_url() || !url_directive.has_end_time_usec())
      continue;

    GURL url(url_directive.url());
    base::Time end_time = UnixUsecToTime(url_directive.end_time_usec());
    if (url.is_valid())
      deletions.emplace_back(url, end_time);
  }
  if (!deletions.empty())
    history_backend->DeleteURLsUntil(deletions);
}

DeleteDirectiveHandler::DeleteDirectiveHandler(
    BackendTaskScheduler backend_task_scheduler)
    : backend_task_scheduler_(std::move(backend_task_scheduler)) {}

DeleteDirectiveHandler::~DeleteDirectiveHandler() {}

void DeleteDirectiveHandler::OnBackendLoaded() {
  backend_loaded_ = true;
  if (wait_until_ready_to_sync_cb_)
    std::move(wait_until_ready_to_sync_cb_).Run();
}

bool DeleteDirectiveHandler::CreateDeleteDirectives(
    const std::set<int64_t>& global_ids,
    base::Time begin_time,
    base::Time end_time) {
  base::Time now = base::Time::Now();
  sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;

  // Delete directives require a non-null begin time, so use 1 if it's null.
  int64_t begin_time_usecs =
      begin_time.is_null() ? 0 : TimeToUnixUsec(begin_time);

  // Determine the actual end time -- it should not be null or in the future.
  // TODO(dubroy): Use sane time (crbug.com/146090) here when it's available.
  base::Time end = (end_time.is_null() || end_time > now) ? now : end_time;
  // -1 because end time in delete directives is inclusive.
  int64_t end_time_usecs = TimeToUnixUsec(end) - 1;

  if (global_ids.empty()) {
    sync_pb::TimeRangeDirective* time_range_directive =
        delete_directive.mutable_time_range_directive();
    time_range_directive->set_start_time_usec(begin_time_usecs);
    time_range_directive->set_end_time_usec(end_time_usecs);
  } else {
    for (auto it = global_ids.begin(); it != global_ids.end(); ++it) {
      sync_pb::GlobalIdDirective* global_id_directive =
          delete_directive.mutable_global_id_directive();
      global_id_directive->add_global_id(*it);
      global_id_directive->set_start_time_usec(begin_time_usecs);
      global_id_directive->set_end_time_usec(end_time_usecs);
    }
  }
  syncer::SyncError error = ProcessLocalDeleteDirective(delete_directive);
  return !error.IsSet();
}

bool DeleteDirectiveHandler::CreateUrlDeleteDirective(const GURL& url) {
  DCHECK(url.is_valid());
  sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;

  sync_pb::UrlDirective* url_directive =
      delete_directive.mutable_url_directive();
  url_directive->set_url(url.spec());
  url_directive->set_end_time_usec(TimeToUnixUsec(base::Time::Now()));

  syncer::SyncError error = ProcessLocalDeleteDirective(delete_directive);
  return !error.IsSet();
}

syncer::SyncError DeleteDirectiveHandler::ProcessLocalDeleteDirective(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!sync_processor_) {
    return syncer::SyncError(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                             "Cannot send local delete directive to sync",
                             syncer::HISTORY_DELETE_DIRECTIVES);
  }
#if !defined(NDEBUG)
  CheckDeleteDirectiveValid(delete_directive);
#endif

  // Generate a random sync tag since history delete directives don't
  // have a 'built-in' ID.  8 bytes should suffice.
  std::string sync_tag = RandASCIIString(8);
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_history_delete_directive()->CopyFrom(
      delete_directive);
  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData(sync_tag, sync_tag, entity_specifics);
  syncer::SyncChange change(FROM_HERE, syncer::SyncChange::ACTION_ADD,
                            sync_data);
  syncer::SyncChangeList changes(1, change);
  return sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void DeleteDirectiveHandler::WaitUntilReadyToSync(base::OnceClosure done) {
  DCHECK(!wait_until_ready_to_sync_cb_);
  if (backend_loaded_) {
    std::move(done).Run();
  } else {
    // Wait until OnBackendLoaded() gets called.
    wait_until_ready_to_sync_cb_ = std::move(done);
  }
}

syncer::SyncMergeResult DeleteDirectiveHandler::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK_EQ(type, syncer::HISTORY_DELETE_DIRECTIVES);
  DCHECK(thread_checker_.CalledOnValidThread());

  sync_processor_ = std::move(sync_processor);
  if (!initial_sync_data.empty()) {
    // Drop processed delete directives during startup.
    backend_task_scheduler_.Run(FROM_HERE,
                                std::make_unique<DeleteDirectiveTask>(
                                    weak_ptr_factory_.GetWeakPtr(),
                                    initial_sync_data, DROP_AFTER_PROCESSING),
                                &internal_tracker_);
  }

  return syncer::SyncMergeResult(type);
}

void DeleteDirectiveHandler::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(type, syncer::HISTORY_DELETE_DIRECTIVES);
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_processor_.reset();
}

syncer::SyncDataList DeleteDirectiveHandler::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::HISTORY_DELETE_DIRECTIVES);
  // TODO(akalin): Keep track of existing delete directives.
  return syncer::SyncDataList();
}

syncer::SyncError DeleteDirectiveHandler::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!sync_processor_) {
    return syncer::SyncError(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                             "Sync is disabled.",
                             syncer::HISTORY_DELETE_DIRECTIVES);
  }

  syncer::SyncDataList delete_directives;
  for (auto it = change_list.begin(); it != change_list.end(); ++it) {
    switch (it->change_type()) {
      case syncer::SyncChange::ACTION_ADD:
        delete_directives.push_back(it->sync_data());
        break;
      case syncer::SyncChange::ACTION_DELETE:
        // TODO(akalin): Keep track of existing delete directives.
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  if (!delete_directives.empty()) {
    // Don't drop real-time delete directive so that sync engine can detect
    // redelivered delete directives to avoid processing them again and again
    // in one chrome session.
    backend_task_scheduler_.Run(FROM_HERE,
                                std::make_unique<DeleteDirectiveTask>(
                                    weak_ptr_factory_.GetWeakPtr(),
                                    delete_directives, KEEP_AFTER_PROCESSING),
                                &internal_tracker_);
  }

  return syncer::SyncError();
}

void DeleteDirectiveHandler::FinishProcessing(
    PostProcessingAction post_processing_action,
    const syncer::SyncDataList& delete_directives) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If specified, drop processed delete directive in sync model because they
  // only need to be applied once.
  if (sync_processor_.get() &&
      post_processing_action == DROP_AFTER_PROCESSING) {
    syncer::SyncChangeList change_list;
    for (size_t i = 0; i < delete_directives.size(); ++i) {
      change_list.push_back(syncer::SyncChange(
          FROM_HERE, syncer::SyncChange::ACTION_DELETE, delete_directives[i]));
    }
    sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  }
}

}  // namespace history
