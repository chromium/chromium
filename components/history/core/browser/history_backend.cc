// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_backend.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "build/build_config.h"
#include "build/ios_buildflags.h"
#include "components/favicon/core/favicon_backend.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_backend_observer.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/keyword_search_term_util.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/browser/sync/history_sync_bridge.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/browser/url_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "sql/error_delegate_util.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/transaction.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_IOS_APP_EXTENSION)
#include "base/ios/scoped_critical_action.h"
#endif

using base::Time;
using base::TimeTicks;
using favicon::FaviconBitmap;
using favicon::FaviconBitmapID;
using favicon::FaviconBitmapIDSize;
using favicon::FaviconBitmapType;
using favicon::IconMapping;
using syncer::ClientTagBasedDataTypeProcessor;

/* The HistoryBackend consists of two components:

    HistoryDatabase (stores past 3 months of history)
      URLDatabase (stores a list of URLs)
      DownloadDatabase (stores a list of downloads)
      VisitDatabase (stores a list of visits for the URLs)
      VisitedLinkDatabase (stores a list of triple-key partitioned URLs)
      VisitSegmentDatabase (stores groups of URLs for the most visited view).

    ExpireHistoryBackend (manages deleting things older than 3 months)
*/

namespace history {

namespace {

using OsType = syncer::DeviceInfo::OsType;
using FormFactor = syncer::DeviceInfo::FormFactor;

#if DCHECK_IS_ON()
// Use to keep track of paths used to host HistoryBackends. This class
// is thread-safe. No two backends should ever run at the same time using the
// same directory since they will contend on the files created there.
class HistoryPathsTracker {
 public:
  HistoryPathsTracker(const HistoryPathsTracker&) = delete;
  HistoryPathsTracker& operator=(const HistoryPathsTracker&) = delete;

  static HistoryPathsTracker* GetInstance() {
    static base::NoDestructor<HistoryPathsTracker> instance;
    return instance.get();
  }

  void AddPath(const base::FilePath& file_path) {
    base::AutoLock auto_lock(lock_);
    paths_.insert(file_path);
  }

  void RemovePath(const base::FilePath& file_path) {
    base::AutoLock auto_lock(lock_);
    auto it = paths_.find(file_path);

    // If the backend was created without a db we are not tracking it.
    if (it != paths_.end())
      paths_.erase(it);
  }

  bool HasPath(const base::FilePath& file_path) {
    base::AutoLock auto_lock(lock_);
    return paths_.find(file_path) != paths_.end();
  }

 private:
  friend class base::NoDestructor<HistoryPathsTracker>;

  HistoryPathsTracker() = default;
  ~HistoryPathsTracker() = default;

  base::Lock lock_;
  base::flat_set<base::FilePath> paths_ GUARDED_BY(lock_);
};
#endif

void RunUnlessCanceled(
    base::OnceClosure closure,
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled) {
  if (!is_canceled.Run())
    std::move(closure).Run();
}

// How long we'll wait to do a commit, so that things are batched together.
const int kCommitIntervalSeconds = 10;

// The maximum number of items we'll allow in the redirect list before
// deleting some.
const int kMaxRedirectCount = 32;

// The maximum number of days for which domain visit metrics are computed
// each time HistoryBackend::GetDomainDiversity() is called.
constexpr int kDomainDiversityMaxBacktrackedDays = 7;

// An offset that corrects possible error in date/time arithmetic caused by
// fluctuation of day length due to Daylight Saving Time (DST). For example,
// given midnight M, its next midnight can be computed as (M + 24 hour
// + offset).LocalMidnight(). In most modern DST systems, the DST shift is
// typically 1 hour. However, a larger value of 4 is chosen here to
// accommodate larger DST shifts that have been used historically and to
// avoid other potential issues.
constexpr int kDSTRoundingOffsetHours = 4;

// When batch-deleting foreign visits (i.e. visits coming from other devices),
// this specifies how many visits to delete in a single HistoryDBTask. This
// usually happens when history sync was turned off.
constexpr int kSyncHistoryForeignVisitsToDeletePerBatch = 100;

// Merges `update` into `existing` by overwriting fields in `existing` that are
// not the default value in `update`.
void MergeUpdateIntoExistingModelAnnotations(
    const VisitContentModelAnnotations& update,
    VisitContentModelAnnotations& existing) {
  if (update.visibility_score !=
      VisitContentModelAnnotations::kDefaultVisibilityScore) {
    existing.visibility_score = update.visibility_score;
  }

  if (!update.categories.empty()) {
    existing.categories = update.categories;
  }

  if (update.page_topics_model_version !=
      VisitContentModelAnnotations::kDefaultPageTopicsModelVersion) {
    existing.page_topics_model_version = update.page_topics_model_version;
  }

  if (!update.entities.empty()) {
    existing.entities = update.entities;
  }
}

class DeleteForeignVisitsDBTask : public HistoryDBTask {
 public:
  ~DeleteForeignVisitsDBTask() override = default;

  bool RunOnDBThread(HistoryBackend* backend, HistoryDatabase* db) override {
    VisitID max_visit_id = db->GetDeleteForeignVisitsUntilId();
    int max_count = kSyncHistoryForeignVisitsToDeletePerBatch;

    VisitVector visits;
    if (!db->GetSomeForeignVisits(max_visit_id, max_count, &visits)) {
      // Some error happened; no point in going on.
      return true;
    }

    backend->RemoveVisits(visits,
                          DeletionInfo::Reason::kDeleteAllForeignVisits);

    bool done = visits.size() < static_cast<size_t>(max_count);
    if (done) {
      // Nothing more to be deleted; clean up the deletion flag.
      db->SetDeleteForeignVisitsUntilId(kInvalidVisitID);
    }
    // Note: As long as this returns false, RunOnDBThread() will get run again
    // (see also comment on HistoryDBTask::RunOnDBThread()).
    return done;
  }

  void DoneRunOnMainThread() override {}
};

// On iOS devices, Returns true if the device that created the foreign visit is
// an Android or iOS device, and has a mobile form factor.
//
// On non-iOS devices, returns false.
bool CanAddForeignVisitToSegments(
    const VisitRow& foreign_visit,
    const std::string& local_device_originator_cache_guid,
    const SyncDeviceInfoMap& sync_device_info) {
#if BUILDFLAG(IS_IOS)
  if (foreign_visit.originator_cache_guid.empty() ||
      !foreign_visit.consider_for_ntp_most_visited) {
    return false;
  }

  auto foreign_device_info_iter =
      sync_device_info.find(foreign_visit.originator_cache_guid);
  auto local_device_info_iter =
      sync_device_info.find(local_device_originator_cache_guid);

  if (foreign_device_info_iter == sync_device_info.end() ||
      local_device_info_iter == sync_device_info.end()) {
    return false;
  }

  std::pair<OsType, FormFactor> foreign_device_info =
      foreign_device_info_iter->second;
  std::pair<OsType, FormFactor> local_device_info =
      local_device_info_iter->second;

  if (local_device_info.first != OsType::kIOS ||
      local_device_info.second != FormFactor::kPhone) {
    return false;
  }

  return foreign_device_info.second == FormFactor::kPhone &&
         (foreign_device_info.first == OsType::kAndroid ||
          foreign_device_info.first == OsType::kIOS);
#else
  return false;
#endif
}

// Returns whether a page visit has a ui::PageTransition type that allows us
// to construct a triple partition key for the VisitedLinkDatabase.
bool IsVisitedLinkTransition(ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
}
// We require a `top_level_site` and a frame_origin to construct a
// visited link partition key. So if `top_level_url` and/or `fame_url` are NULL
// OR the transition type is a context where we know we cannot accurately
// construct a triple partition key, then we skip the VisitedLinkDatabase.
// We aren't adding ephemeral keys because inherently, their state shouldn't
// be persisted across browsing sessions.
bool AddToVisitedLinkDatabase(ui::PageTransition transition,
                              std::optional<GURL> top_level_url,
                              std::optional<GURL> frame_url,
                              bool is_ephemeral) {
  return IsVisitedLinkTransition(transition) && top_level_url.has_value() &&
         frame_url.has_value() && !is_ephemeral;
}

}  // namespace

std::u16string FormatUrlForRedirectComparison(const GURL& url) {
  GURL::Replacements remove_port;
  remove_port.ClearPort();
  return url_formatter::FormatUrl(
      url.ReplaceComponents(remove_port),
      url_formatter::kFormatUrlOmitHTTP | url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitUsernamePassword |
          url_formatter::kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::NONE, nullptr, nullptr, nullptr);
}

base::Time MidnightNDaysLater(base::Time time, int days) {
  return (time.LocalMidnight() + base::Days(days) +
          base::Hours(kDSTRoundingOffsetHours))
      .LocalMidnight();
}

QueuedHistoryDBTask::QueuedHistoryDBTask(
    std::unique_ptr<HistoryDBTask> task,
    scoped_refptr<base::SequencedTaskRunner> origin_loop,
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled)
    : task_(std::move(task)),
      origin_loop_(origin_loop),
      is_canceled_(is_canceled) {
  DCHECK(task_);
  DCHECK(origin_loop_);
  DCHECK(!is_canceled_.is_null());
}

QueuedHistoryDBTask::~QueuedHistoryDBTask() {
  // Ensure that `task_` is destroyed on its origin thread.
  origin_loop_->PostTask(FROM_HERE,
                         base::BindOnce(&base::DeletePointer<HistoryDBTask>,
                                        base::Unretained(task_.release())));
}

bool QueuedHistoryDBTask::is_canceled() {
  return is_canceled_.Run();
}

bool QueuedHistoryDBTask::Run(HistoryBackend* backend, HistoryDatabase* db) {
  return task_->RunOnDBThread(backend, db);
}

void QueuedHistoryDBTask::DoneRun() {
  origin_loop_->PostTask(
      FROM_HERE,
      base::BindOnce(&RunUnlessCanceled,
                     base::BindOnce(&HistoryDBTask::DoneRunOnMainThread,
                                    base::Unretained(task_.get())),
                     is_canceled_));
}

// HistoryBackend --------------------------------------------------------------

// static
bool HistoryBackend::IsTypedIncrement(ui::PageTransition transition) {
  if (ui::PageTransitionIsNewNavigation(transition) &&
      ((ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) &&
        !ui::PageTransitionIsRedirect(transition)) ||
       ui::PageTransitionCoreTypeIs(transition,
                                    ui::PAGE_TRANSITION_KEYWORD_GENERATED))) {
    return true;
  }
  return false;
}

HistoryBackend::HistoryBackend(
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<HistoryBackendClient> backend_client,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : delegate_(std::move(delegate)),
      recent_redirects_(kMaxRedirectCount),
      backend_client_(std::move(backend_client)),
      expirer_(this, backend_client_.get(), task_runner),
      task_runner_(task_runner) {
  DCHECK(delegate_);
}

HistoryBackend::~HistoryBackend() {
  DCHECK(scheduled_commit_.IsCancelled()) << "Deleting without cleanup";
  queued_history_db_tasks_.clear();

  // Clear the error callback. The error callback that is installed does not
  // process an error immediately, rather it uses a PostTask() with `this`. As
  // `this` is being deleted, scheduling a PostTask() with `this` would be
  // fatal (use-after-free). Additionally, as we're in shutdown, there isn't
  // much point in trying to handle the error. If the error is really fatal,
  // we'll cleanup the next time the backend is created.
  if (db_)
    db_->reset_error_callback();

  // First close the databases before optionally running the "destroy" task.
  CloseAllDatabases();

  if (!backend_destroy_task_.is_null()) {
    // Notify an interested party (typically a unit test) that we're done.
    DCHECK(backend_destroy_task_runner_);
    backend_destroy_task_runner_->PostTask(FROM_HERE,
                                           std::move(backend_destroy_task_));
  }

#if DCHECK_IS_ON()
  HistoryPathsTracker::GetInstance()->RemovePath(history_dir_);
#endif
}

void HistoryBackend::Init(
    bool force_fail,
    const HistoryDatabaseParams& history_database_params) {
  TRACE_EVENT0("browser", "HistoryBackend::Init");

  DCHECK(base::PathExists(history_database_params.history_dir))
      << "History directory does not exist. If you are in a test make sure "
         "that ~TestingProfile() has not been called or that the "
         "ScopedTempDirectory used outlives this task.";

  if (!force_fail)
    InitImpl(history_database_params);
  delegate_->DBLoaded();

  history_sync_bridge_ = std::make_unique<HistorySyncBridge>(
      this, db_ ? db_->GetHistoryMetadataDB() : nullptr,
      std::make_unique<ClientTagBasedDataTypeProcessor>(
          syncer::HISTORY,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              history_database_params.channel)));

  if (db_ && db_->GetDeleteForeignVisitsUntilId() != kInvalidVisitID) {
    // A deletion of foreign visits was still ongoing during the previous
    // browser shutdown. Continue it.
    StartDeletingForeignVisits();
  }

  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&HistoryBackend::OnMemoryPressure,
                                     base::Unretained(this)));
}

void HistoryBackend::SetOnBackendDestroyTask(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::OnceClosure task) {
  TRACE_EVENT0("browser", "HistoryBackend::SetOnBackendDestroyTask");
  if (!backend_destroy_task_.is_null())
    DLOG(WARNING) << "Setting more than one destroy task, overriding";
  backend_destroy_task_runner_ = std::move(task_runner);
  backend_destroy_task_ = std::move(task);
}

void HistoryBackend::Closing() {
  TRACE_EVENT0("browser", "HistoryBackend::Closing");
  // The history system is shutting down. Cancel any pending/scheduled work.
  CancelScheduledCommit();
  queued_history_db_tasks_.clear();
  posted_history_db_task_.Cancel();
}

#if BUILDFLAG(IS_IOS)
void HistoryBackend::PersistState() {
  TRACE_EVENT0("browser", "HistoryBackend::PersistState");
  Commit();
}
#endif

void HistoryBackend::ClearCachedDataForContextID(ContextID context_id) {
  TRACE_EVENT0("browser", "HistoryBackend::ClearCachedDataForContextID");
  tracker_.ClearCachedDataForContextID(context_id);
}

base::FilePath HistoryBackend::GetFaviconsFileName() const {
  return history_dir_.Append(kFaviconsFilename);
}

SegmentID HistoryBackend::GetLastSegmentID(VisitID from_visit) {
  // Set is used to detect referrer loops.  Should not happen, but can
  // if the database is corrupt.
  std::set<VisitID> visit_set;
  VisitID visit_id = from_visit;
  while (visit_id) {
    VisitRow row;
    if (!db_->GetRowForVisit(visit_id, &row)) {
      return 0;
    }
    if (row.segment_id) {
      return row.segment_id;  // Found a visit in this change with a segment.
    }

    // Check the referrer of this visit, if any.
    visit_id = row.referring_visit;

    if (visit_set.find(visit_id) != visit_set.end()) {
      DLOG(WARNING) << "Loop in referer chain, possible db corruption";
      return 0;
    }
    visit_set.insert(visit_id);
  }
  return 0;
}

SegmentID HistoryBackend::AssignSegmentForNewVisit(
    const GURL& url,
    VisitID from_visit,
    VisitID visit_id,
    ui::PageTransition transition_type,
    const Time ts) {
  if (!db_) {
    return 0;
  }

  // We only consider main frames.
  if (!ui::PageTransitionIsMainFrame(transition_type)) {
    return 0;
  }

  SegmentID segment_id = CalculateSegmentID(url, from_visit, transition_type);

  if (!segment_id) {
    return 0;
  }

  // Set the segment in the visit.
  if (!db_->SetSegmentID(visit_id, segment_id)) {
    DLOG(WARNING) << "AssignSegmentForNewVisit: SetSegmentID failed: "
                  << segment_id;
    return 0;
  }

  // Finally, increase the counter for that segment / day.
  if (!db_->UpdateSegmentVisitCount(segment_id, ts, 1)) {
    DLOG(WARNING)
        << "AssignSegmentForNewVisit: UpdateSegmentVisitCount failed: "
        << segment_id;
    return 0;
  }

  return segment_id;
}

SegmentID HistoryBackend::CalculateSegmentID(
    const GURL& url,
    VisitID from_visit,
    ui::PageTransition transition_type) {
  // We only consider main frames.
  if (!ui::PageTransitionIsMainFrame(transition_type))
    return 0;

  SegmentID segment_id = 0;

  // Are we at the beginning of a new segment?
  // Note that navigating to an existing entry (with back/forward) reuses the
  // same transition type.  We are not adding it as a new segment in that case
  // because if this was the target of a redirect, we might end up with
  // 2 entries for the same final URL. Ex: User types google.net, gets
  // redirected to google.com. A segment is created for google.net. On
  // google.com users navigates through a link, then press back. That last
  // navigation is for the entry google.com transition typed. We end up adding
  // a segment for that one as well. So we end up with google.net and google.com
  // in the segment table, showing as 2 entries in the NTP.
  // Note also that we should still be updating the visit count for that segment
  // which we are not doing now. It should be addressed when
  // http://crbug.com/96860 is fixed.
  if ((ui::PageTransitionCoreTypeIs(transition_type,
                                    ui::PAGE_TRANSITION_TYPED) ||
       ui::PageTransitionCoreTypeIs(transition_type,
                                    ui::PAGE_TRANSITION_AUTO_BOOKMARK)) &&
      (transition_type & ui::PAGE_TRANSITION_FORWARD_BACK) == 0) {
    // If so, create or get the segment.
    std::string segment_name = db_->ComputeSegmentName(url);
    URLID url_id = db_->GetRowForURL(url, nullptr);
    if (!url_id)
      return 0;

    segment_id = db_->GetSegmentNamed(segment_name);
    if (!segment_id) {
      segment_id = db_->CreateSegment(url_id, segment_name);
      if (!segment_id) {
        DLOG(WARNING) << "CalculateSegmentID: CreateSegment failed: "
                      << segment_name;
        return 0;
      }
    } else {
      // Note: if we update an existing segment, we update the url used to
      // represent that segment in order to minimize stale most visited
      // images.
      db_->UpdateSegmentRepresentationURL(segment_id, url_id);
    }
  } else {
    // Note: it is possible there is no segment ID set for this visit chain.
    // This can happen if the initial navigation wasn't AUTO_BOOKMARK or
    // TYPED. (For example GENERATED). In this case this visit doesn't count
    // toward any segment.
    segment_id = GetLastSegmentID(from_visit);
  }

  return segment_id;
}

void HistoryBackend::UpdateSegmentForExistingForeignVisit(VisitRow& visit_row) {
  CHECK(can_add_foreign_visits_to_segments_);
  CHECK(!visit_row.originator_cache_guid.empty());

  URLRow url_row;
  if (!db_->GetURLRow(visit_row.url_id, &url_row)) {
    DLOG(WARNING) << "Failed to get id " << visit_row.url_id
                  << " from history.urls.";
    return;
  }

  SegmentID new_segment_id =
      (can_add_foreign_visits_to_segments_ &&
       CanAddForeignVisitToSegments(
           visit_row, local_device_originator_cache_guid_, sync_device_info_))
          ? CalculateSegmentID(url_row.url(), visit_row.referring_visit,
                               visit_row.transition)
          : 0;

  if (visit_row.segment_id == new_segment_id) {
    return;
  }

  if (visit_row.segment_id != 0 &&
      !db_->UpdateSegmentVisitCount(visit_row.segment_id, visit_row.visit_time,
                                    -1)) {
    // Decrement the count of the old segment.
    DLOG(WARNING) << "UpdateSegmentForExistingForeignVisit: "
                     "UpdateSegmentVisitCount failed: "
                  << visit_row.segment_id;
    return;
  }

  if (new_segment_id != 0 &&
      !db_->UpdateSegmentVisitCount(new_segment_id, visit_row.visit_time, 1)) {
    DLOG(WARNING) << "UpdateSegmentForExistingForeignVisit: "
                     "UpdateSegmentVisitCount failed: "
                  << new_segment_id;
    return;
  }

  visit_row.segment_id = new_segment_id;

  db_->SetSegmentID(visit_row.visit_id, new_segment_id);
}

void HistoryBackend::UpdateWithPageEndTime(ContextID context_id,
                                           int nav_entry_id,
                                           const GURL& url,
                                           Time end_ts) {
  TRACE_EVENT0("browser", "HistoryBackend::UpdateWithPageEndTime");
  // Will be filled with the URL ID and the visit ID of the last addition.
  VisitID visit_id = tracker_.GetLastVisit(context_id, nav_entry_id, url);
  UpdateVisitDuration(visit_id, end_ts);
}

void HistoryBackend::SetBrowsingTopicsAllowed(ContextID context_id,
                                              int nav_entry_id,
                                              const GURL& url) {
  TRACE_EVENT0("browser", "HistoryBackend::SetBrowsingTopicsAllowed");

  if (!db_)
    return;

  VisitID visit_id = tracker_.GetLastVisit(context_id, nav_entry_id, url);
  if (!visit_id)
    return;

  // Only add to the annotations table if the visit_id exists in the visits
  // table.
  VisitContentAnnotations annotations;
  if (db_->GetContentAnnotationsForVisit(visit_id, &annotations)) {
    annotations.annotation_flags |=
        VisitContentAnnotationFlag::kBrowsingTopicsEligible;
    db_->UpdateContentAnnotationsForVisit(visit_id, annotations);
  } else {
    annotations.annotation_flags |=
        VisitContentAnnotationFlag::kBrowsingTopicsEligible;
    db_->AddContentAnnotationsForVisit(visit_id, annotations);
  }
  ScheduleCommit();
}

void HistoryBackend::SetPageLanguageForVisit(ContextID context_id,
                                             int nav_entry_id,
                                             const GURL& url,
                                             const std::string& page_language) {
  VisitID visit_id = tracker_.GetLastVisit(context_id, nav_entry_id, url);
  if (!visit_id)
    return;

  SetPageLanguageForVisitByVisitID(visit_id, page_language);
}

void HistoryBackend::SetPageLanguageForVisitByVisitID(
    VisitID visit_id,
    const std::string& page_language) {
  TRACE_EVENT0("browser", "HistoryBackend::SetPageLanguageForVisitByVisitID");

  if (!db_)
    return;

  // Only add to the annotations table if the visit_id exists in the visits
  // table.
  VisitRow visit_row;
  if (db_->GetRowForVisit(visit_id, &visit_row)) {
    VisitContentAnnotations annotations;
    if (db_->GetContentAnnotationsForVisit(visit_id, &annotations)) {
      annotations.page_language = page_language;
      db_->UpdateContentAnnotationsForVisit(visit_id, annotations);
    } else {
      annotations.page_language = page_language;
      db_->AddContentAnnotationsForVisit(visit_id, annotations);
    }
    NotifyVisitUpdated(visit_row, VisitUpdateReason::kSetPageLanguage);
    ScheduleCommit();
  }
}

void HistoryBackend::SetPasswordStateForVisit(
    ContextID context_id,
    int nav_entry_id,
    const GURL& url,
    VisitContentAnnotations::PasswordState password_state) {
  VisitID visit_id = tracker_.GetLastVisit(context_id, nav_entry_id, url);
  if (!visit_id)
    return;

  SetPasswordStateForVisitByVisitID(visit_id, password_state);
}

void HistoryBackend::SetPasswordStateForVisitByVisitID(
    VisitID visit_id,
    VisitContentAnnotations::PasswordState password_state) {
  TRACE_EVENT0("browser", "HistoryBackend::SetPasswordStateForVisitByVisitID");

  if (!db_)
    return;

  // Only add to the annotations table if the visit_id exists in the visits
  // table.
  VisitRow visit_row;
  if (db_->GetRowForVisit(visit_id, &visit_row)) {
    VisitContentAnnotations annotations;
    if (db_->GetContentAnnotationsForVisit(visit_id, &annotations)) {
      annotations.password_state = password_state;
      db_->UpdateContentAnnotationsForVisit(visit_id, annotations);
    } else {
      annotations.password_state = password_state;
      db_->AddContentAnnotationsForVisit(visit_id, annotations);
    }
    NotifyVisitUpdated(visit_row, VisitUpdateReason::kSetPasswordState);
    ScheduleCommit();
  }
}

void HistoryBackend::AddContentModelAnnotationsForVisit(
    VisitID visit_id,
    const VisitContentModelAnnotations& model_annotations) {
  TRACE_EVENT0("browser", "HistoryBackend::AddContentModelAnnotationsForVisit");

  if (!db_)
    return;

  // Only add to the annotations table if the visit_id exists in the visits
  // table.
  VisitRow visit_row;
  if (db_->GetRowForVisit(visit_id, &visit_row)) {
    VisitContentAnnotations annotations;
    if (db_->GetContentAnnotationsForVisit(visit_id, &annotations)) {
      MergeUpdateIntoExistingModelAnnotations(model_annotations,
                                              annotations.model_annotations);
      db_->UpdateContentAnnotationsForVisit(visit_id, annotations);
    } else {
      annotations.model_annotations = model_annotations;
      db_->AddContentAnnotationsForVisit(visit_id, annotations);
    }
    ScheduleCommit();
  }
}

void HistoryBackend::AddRelatedSearchesForVisit(
    VisitID visit_id,
    const std::vector<std::string>& related_searches) {
  TRACE_EVENT0("browser", "HistoryBackend::AddRelatedSearchesForVisit");

  if (!db_)
    return;

  // Only add to the annotations table if the visit_id exists in the visits
  // table.
  VisitRow visit_row;
  if (db_->GetRowForVisit(visit_id, &visit_row)) {
    VisitContentAnnotations annotations;
    if (db_->GetContentAnnotationsForVisit(visit_id, &annotations)) {
      annotations.related_searches = related_searches;
      db_->UpdateContentAnnotationsForVisit(visit_id, annotations);
    } else {
      annotations.related_searches = related_searches;
      db_->AddContentAnnotationsForVisit(visit_id, annotations);
    }
    ScheduleCommit();
  }
}

void HistoryBackend::AddSearchMetadataForVisit(
    VisitID visit_id,
    const GURL& search_normalized_url,
    const std::u16string& search_terms) {
  TRACE_EVENT0("browser", "HistoryBackend::AddSearchMetadataForVisit");

  if (!db_)
    return;

  // Only add to the annotations table if the visit_id exists in the visits
  // table.
  VisitRow visit_row;
  if (db_->GetRowForVisit(visit_id, &visit_row)) {
    VisitContentAnnotations annotations;
    if (db_->GetContentAnnotationsForVisit(visit_id, &annotations)) {
      annotations.search_normalized_url = search_normalized_url;
      annotations.search_terms = search_terms;
      db_->UpdateContentAnnotationsForVisit(visit_id, annotations);
    } else {
      annotations.search_normalized_url = search_normalized_url;
      annotations.search_terms = search_terms;
      db_->AddContentAnnotationsForVisit(visit_id, annotations);
    }
    ScheduleCommit();
  }
}

void HistoryBackend::AddPageMetadataForVisit(
    VisitID visit_id,
    const std::string& alternative_title) {
  TRACE_EVENT0("browser", "HistoryBackend::AddPageMetadataForVisit");

  if (!db_)
    return;
  // Only add to the annotations table if the visit_id exists in the visits
  // table.
  VisitRow visit_row;
  if (db_->GetRowForVisit(visit_id, &visit_row)) {
    VisitContentAnnotations annotations;
    if (db_->GetContentAnnotationsForVisit(visit_id, &annotations)) {
      annotations.alternative_title = alternative_title;
      db_->UpdateContentAnnotationsForVisit(visit_id, annotations);
    } else {
      annotations.alternative_title = alternative_title;
      db_->AddContentAnnotationsForVisit(visit_id, annotations);
    }
    ScheduleCommit();
  }
}

void HistoryBackend::SetHasUrlKeyedImageForVisit(VisitID visit_id,
                                                 bool has_url_keyed_image) {
  TRACE_EVENT0("browser", "HistoryBackend::SetHasUrlKeyedImageForVisit");

  if (!db_) {
    return;
  }
  // Only add to the annotations table if the visit_id exists in the visits
  // table.
  VisitRow visit_row;
  if (db_->GetRowForVisit(visit_id, &visit_row)) {
    VisitContentAnnotations annotations;
    if (db_->GetContentAnnotationsForVisit(visit_id, &annotations)) {
      annotations.has_url_keyed_image = has_url_keyed_image;
      db_->UpdateContentAnnotationsForVisit(visit_id, annotations);
    } else {
      annotations.has_url_keyed_image = has_url_keyed_image;
      db_->AddContentAnnotationsForVisit(visit_id, annotations);
    }
    ScheduleCommit();
  }
}

void HistoryBackend::UpdateVisitDuration(VisitID visit_id, const Time end_ts) {
  if (!db_)
    return;

  // Get the starting visit_time for visit_id.
  VisitRow visit_row;
  if (db_->GetRowForVisit(visit_id, &visit_row)) {
    // We should never have a negative duration time even when time is skewed.
    visit_row.visit_duration = end_ts > visit_row.visit_time
                                   ? end_ts - visit_row.visit_time
                                   : base::Microseconds(0);
    db_->UpdateVisitRow(visit_row);
    NotifyVisitUpdated(visit_row, VisitUpdateReason::kUpdateVisitDuration);
  }
}

void HistoryBackend::MarkVisitAsKnownToSync(VisitID visit_id) {
  if (!db_) {
    return;
  }

  VisitRow visit_row;
  if (db_->GetRowForVisit(visit_id, &visit_row)) {
    visit_row.is_known_to_sync = true;

    if (db_->UpdateVisitRow(visit_row)) {
      db_->SetKnownToSyncVisitsExist(true);
    }

    // Purposely don't call `NotifyVisitUpdated()` here, because this change
    // itself is de minimis and triggered by the sync history backend observer.
  }
}

bool HistoryBackend::IsUntypedIntranetHost(const GURL& url) {
  if (!url.SchemeIs(url::kHttpScheme) && !url.SchemeIs(url::kHttpsScheme) &&
      !url.SchemeIs(url::kFtpScheme))
    return false;

  const std::string host = url.host();
  const size_t registry_length =
      net::registry_controlled_domains::GetCanonicalHostRegistryLength(
          host, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  return (registry_length == 0) && !db_->IsTypedHost(host, /*scheme=*/nullptr);
}

OriginCountAndLastVisitMap HistoryBackend::GetCountsAndLastVisitForOrigins(
    const std::set<GURL>& origins) const {
  if (!db_)
    return OriginCountAndLastVisitMap();
  if (origins.empty())
    return OriginCountAndLastVisitMap();

  URLDatabase::URLEnumerator it;
  if (!db_->InitURLEnumeratorForEverything(&it))
    return OriginCountAndLastVisitMap();

  OriginCountAndLastVisitMap origin_count_map;
  for (const GURL& origin : origins)
    origin_count_map[origin] = std::make_pair(0, base::Time());

  URLRow row;
  while (it.GetNextURL(&row)) {
    GURL origin = row.url().DeprecatedGetOriginAsURL();
    auto iter = origin_count_map.find(origin);
    if (iter != origin_count_map.end()) {
      std::pair<int, base::Time>& value = iter->second;
      ++(value.first);
      if (value.second.is_null() || value.second < row.last_visit())
        value.second = row.last_visit();
    }
  }

  return origin_count_map;
}

void HistoryBackend::AddPage(const HistoryAddPageArgs& request) {
  TRACE_EVENT0("browser", "HistoryBackend::AddPage");
  DCHECK(request.url.is_valid());

  if (!db_)
    return;

  // Will be filled with the visit ID of the last addition.
  VisitID last_visit_id = tracker_.GetLastVisit(
      request.context_id, request.nav_entry_id, request.referrer);

  GURL external_referrer_url;
  if (request.referrer.is_valid() && last_visit_id == kInvalidVisitID) {
    external_referrer_url = request.referrer;
  }

  const VisitID from_visit_id = last_visit_id;

  // If a redirect chain is given, we expect the last item in that chain to be
  // the final URL.
  DCHECK(request.redirects.empty() || request.redirects.back() == request.url);

  // If the user is adding older history, we need to make sure our times
  // are correct.
  if (request.time < first_recorded_time_)
    first_recorded_time_ = request.time;

  ui::PageTransition request_transition = request.transition;
  const bool is_keyword_generated = ui::PageTransitionCoreTypeIs(
      request_transition, ui::PAGE_TRANSITION_KEYWORD_GENERATED);

  // If the user is navigating to a not-previously-typed intranet hostname,
  // change the transition to TYPED so that the omnibox will learn that this is
  // a known host.
  const bool has_redirects = request.redirects.size() > 1;
  if (ui::PageTransitionIsMainFrame(request_transition) &&
      !ui::PageTransitionCoreTypeIs(request_transition,
                                    ui::PAGE_TRANSITION_TYPED) &&
      !is_keyword_generated) {
    // Check both the start and end of a redirect chain, since the user will
    // consider both to have been "navigated to".
    if (IsUntypedIntranetHost(request.url) ||
        (has_redirects && IsUntypedIntranetHost(request.redirects[0]))) {
      request_transition = ui::PageTransitionFromInt(
          ui::PAGE_TRANSITION_TYPED |
          ui::PageTransitionGetQualifier(request_transition));
    }
  }

  VisitID opener_visit = 0;
  if (request.opener) {
    opener_visit = tracker_.GetLastVisit(request.opener->context_id,
                                         request.opener->nav_entry_id,
                                         request.opener->url);
  }

  // Every url in the redirect chain gets the same `top_level_url` and
  // `frame_url` values.
  std::optional<GURL> top_level_url = std::nullopt;
  if (request.top_level_url.has_value() && request.top_level_url->is_valid()) {
    top_level_url = request.top_level_url;
  }
  std::optional<GURL> frame_url = std::nullopt;
  if (request.referrer.is_valid()) {
    frame_url = request.referrer;
  }

  if (!has_redirects) {
    // The single entry is both a chain start and end.
    ui::PageTransition t = ui::PageTransitionFromInt(
        request_transition | ui::PAGE_TRANSITION_CHAIN_START |
        ui::PAGE_TRANSITION_CHAIN_END);

    // No redirect case (one element means just the page itself).
    last_visit_id =
        AddPageVisit(request.url, request.time, last_visit_id,
                     external_referrer_url, t, request.hidden,
                     request.visit_source, IsTypedIncrement(t), opener_visit,
                     request.consider_for_ntp_most_visited,
                     request.local_navigation_id, request.title, top_level_url,
                     frame_url, request.app_id)
            .second;

    // Update the segment for this visit. KEYWORD_GENERATED visits should not
    // result in changing most visited, so we don't update segments (most
    // visited db).
    if (!is_keyword_generated && request.consider_for_ntp_most_visited) {
      AssignSegmentForNewVisit(request.url, from_visit_id, last_visit_id, t,
                               request.time);
    }
  } else {
    // Redirect case. Add the redirect chain.

    ui::PageTransition redirect_info = ui::PAGE_TRANSITION_CHAIN_START;

    RedirectList redirects = request.redirects;
    // In the presence of client redirects, `request.redirects` can be a partial
    // chain because previous calls to this function may have reported a
    // redirect chain already. This is fine for the visits database where we'll
    // just append data but insufficient for `recent_redirects_`
    // (backpropagation of favicons and titles), where we'd like the full
    // (extended) redirect chain. We use `extended_redirect_chain` to represent
    // this.
    RedirectList extended_redirect_chain;

    if (redirects[0].SchemeIs(url::kAboutScheme)) {
      // When the redirect source + referrer is "about" we skip it. This
      // happens when a page opens a new frame/window to about:blank and then
      // script sets the URL to somewhere else (used to hide the referrer). It
      // would be nice to keep all these redirects properly but we don't ever
      // see the initial about:blank load, so we don't know where the
      // subsequent client redirect came from.
      //
      // In this case, we just don't bother hooking up the source of the
      // redirects, so we remove it.
      redirects.erase(redirects.begin());
    } else if (request_transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
      redirect_info = ui::PAGE_TRANSITION_CLIENT_REDIRECT;
      // The first entry in the redirect chain initiated a client redirect.
      // We don't add this to the database since the referrer is already
      // there, so we skip over it but change the transition type of the first
      // transition to client redirect.
      //
      // The referrer is invalid when restoring a session that features an
      // https tab that redirects to a different host or to http. In this
      // case we don't need to reconnect the new redirect with the existing
      // chain.
      if (request.referrer.is_valid()) {
        // redirects.begin() should equal request.referrer, but sometimes it
        // doesn't for an unknown reason. See crbug.com/1502514.
        redirects.erase(redirects.begin());

        // If the navigation entry for this visit has replaced that for the
        // first visit, remove the CHAIN_END marker from the first visit. This
        // can be called a lot, for example, the page cycler, and most of the
        // time we won't have changed anything.
        VisitRow visit_row;
        if (request.did_replace_entry) {
          if (db_->GetRowForVisit(last_visit_id, &visit_row) &&
              visit_row.transition & ui::PAGE_TRANSITION_CHAIN_END) {
            visit_row.transition = ui::PageTransitionFromInt(
                visit_row.transition & ~ui::PAGE_TRANSITION_CHAIN_END);
            db_->UpdateVisitRow(visit_row);
            NotifyVisitUpdated(visit_row, VisitUpdateReason::kUpdateTransition);
          }

          extended_redirect_chain = GetCachedRecentRedirects(request.referrer);
        }
      }
    }

    bool transfer_typed_credit_from_first_to_second_url = false;
    if (redirects.size() > 1) {
      // Check if the first redirect is the same as the original URL but
      // upgraded to HTTPS. This ignores the port numbers (in case of
      // non-standard HTTP or HTTPS ports) and trivial subdomains (e.g., "www."
      // or "m.").
      if (IsTypedIncrement(request_transition) &&
          redirects[0].SchemeIs(url::kHttpScheme) &&
          redirects[1].SchemeIs(url::kHttpsScheme) &&
          FormatUrlForRedirectComparison(redirects[0]) ==
              FormatUrlForRedirectComparison(redirects[1])) {
        transfer_typed_credit_from_first_to_second_url = true;
      } else if (ui::PageTransitionCoreTypeIs(
                     request_transition, ui::PAGE_TRANSITION_FORM_SUBMIT)) {
        // If this is a form submission, the user was on the previous page and
        // we should have saved the title and favicon already. Don't overwrite
        // it with the redirected page. For example, a page titled "Create X"
        // should not be updated to "Newly Created Item" on a successful POST
        // when the new page is titled "Newly Created Item".
        redirects.erase(redirects.begin());
      }
    }

    for (size_t redirect_index = 0; redirect_index < redirects.size();
         redirect_index++) {
      DCHECK(redirects[redirect_index].is_valid());

      constexpr int kRedirectQualifiers = ui::PAGE_TRANSITION_CHAIN_START |
                                          ui::PAGE_TRANSITION_CHAIN_END |
                                          ui::PAGE_TRANSITION_IS_REDIRECT_MASK;
      // Remove any redirect-related qualifiers that `request_transition` may
      // have (there usually shouldn't be any, except for CLIENT_REDIRECT which
      // was already handled above), and replace them with the `redirect_info`.
      ui::PageTransition t = ui::PageTransitionFromInt(
          (request_transition & ~kRedirectQualifiers) | redirect_info);

      // If this is the last transition, add a CHAIN_END marker.
      if (redirect_index == (redirects.size() - 1)) {
        t = ui::PageTransitionFromInt(t | ui::PAGE_TRANSITION_CHAIN_END);
      }

      bool should_increment_typed_count = IsTypedIncrement(t);
      if (transfer_typed_credit_from_first_to_second_url) {
        if (redirect_index == 0)
          should_increment_typed_count = false;
        else if (redirect_index == 1)
          should_increment_typed_count = true;
      }

      // Record all redirect visits with the same timestamp. We don't display
      // them anyway, and if we ever decide to, we can reconstruct their order
      // from the redirect chain. Only place the opener on the initial visit in
      // the chain.
      last_visit_id =
          AddPageVisit(redirects[redirect_index], request.time, last_visit_id,
                       redirect_index == 0 ? external_referrer_url : GURL(), t,
                       request.hidden, request.visit_source,
                       should_increment_typed_count,
                       redirect_index == 0 ? opener_visit : 0,
                       request.consider_for_ntp_most_visited,
                       request.local_navigation_id, request.title,
                       top_level_url, frame_url, request.app_id)
              .second;

      if (t & ui::PAGE_TRANSITION_CHAIN_START) {
        if (request.consider_for_ntp_most_visited) {
          AssignSegmentForNewVisit(redirects[redirect_index], from_visit_id,
                                   last_visit_id, t, request.time);
        }
      }

      // Subsequent transitions in the redirect list must all be server
      // redirects.
      redirect_info = ui::PAGE_TRANSITION_SERVER_REDIRECT;
    }

    // Last, save this redirect chain for later so we can set titles & favicons
    // on the redirected pages properly. For this we use the extended redirect
    // chain, which includes URLs from chained redirects.
    extended_redirect_chain.insert(extended_redirect_chain.end(),
                                   std::make_move_iterator(redirects.begin()),
                                   std::make_move_iterator(redirects.end()));
    recent_redirects_.Put(request.url, extended_redirect_chain);
  }

  // The below code assumes that last_visit_id should be populated with the
  // VisitID for the visit that is being added by this method.
  bool current_visit_was_successfully_added =
      last_visit_id != kInvalidVisitID && last_visit_id != from_visit_id;

  if (current_visit_was_successfully_added && request.context_annotations) {
    // The `request` contains only the on-visit annotation fields; all other
    // fields aren't known yet. Leave them empty.
    VisitContextAnnotations annotations;
    annotations.on_visit = *request.context_annotations;
    AddContextAnnotationsForVisit(last_visit_id, annotations);
  }

  // TODO(brettw) bug 1140015: Add an "add page" notification so the history
  // views can keep in sync.

  // Add the last visit to the tracker so we can get outgoing transitions.
  // Keyword-generated visits are artificially generated. They duplicate the
  // real navigation, and are added to ensure autocompletion in the omnibox
  // works. As they are artificial they shouldn't be tracked for referral
  // chains.
  // TODO(evanm): Due to http://b/1194536 we lose the referrers of a subframe
  // navigation anyway, so last_visit_id is always zero for them.  But adding
  // them here confuses main frame history, so we skip them for now.
  if (!ui::PageTransitionCoreTypeIs(request_transition,
                                    ui::PAGE_TRANSITION_AUTO_SUBFRAME) &&
      !ui::PageTransitionCoreTypeIs(request_transition,
                                    ui::PAGE_TRANSITION_MANUAL_SUBFRAME) &&
      !is_keyword_generated && current_visit_was_successfully_added) {
    tracker_.AddVisit(request.context_id, request.nav_entry_id, request.url,
                      last_visit_id);
  }

  delegate_->NotifyVisitedLinksAdded(request);

  ScheduleCommit();
}

void HistoryBackend::InitImpl(
    const HistoryDatabaseParams& history_database_params) {
  DCHECK(!db_) << "Initializing HistoryBackend twice";
  // In the rare case where the db fails to initialize a dialog may get shown
  // the blocks the caller, yet allows other messages through. For this reason
  // we only set db_ to the created database if creation is successful. That
  // way other methods won't do anything as db_ is still null.

  // Compute the file names.
  history_dir_ = history_database_params.history_dir;

#if DCHECK_IS_ON()
  DCHECK(!HistoryPathsTracker::GetInstance()->HasPath(history_dir_))
      << "There already is a HistoryBackend running using the file at: "
      << history_database_params.history_dir
      << ". Tests have to make sure that HistoryBackend destruction is "
         "complete using SetOnBackendDestroyTask() or other flush mechanisms "
         "before creating a new HistoryBackend that uses the same directory.";

  HistoryPathsTracker::GetInstance()->AddPath(history_dir_);
#endif

  base::FilePath history_name = history_dir_.Append(kHistoryFilename);
  base::FilePath favicon_name = GetFaviconsFileName();

  // Delete the old index database files which are no longer used.
  DeleteFTSIndexDatabases();

  // History database.
  db_ = std::make_unique<HistoryDatabase>(
      history_database_params.download_interrupt_reason_none,
      history_database_params.download_interrupt_reason_crash);

  // Unretained to avoid a ref loop with db_.
  db_->set_error_callback(base::BindRepeating(
      &HistoryBackend::DatabaseErrorCallback, base::Unretained(this)));

  diagnostics_string_.clear();
  sql::InitStatus status = db_->Init(history_name);
  switch (status) {
    case sql::INIT_OK:
      break;
    case sql::INIT_FAILURE: {
      // A null db_ will cause all calls on this object to notice this error
      // and to not continue. If the error callback scheduled killing the
      // database, the task it posted has not executed yet. Try killing the
      // database now before we close it.
      bool kill_db = scheduled_kill_db_;
      if (kill_db)
        KillHistoryDatabase();

      // The frequency of this UMA will indicate how often history
      // initialization fails.
      UMA_HISTOGRAM_BOOLEAN("History.AttemptedToFixProfileError", kill_db);
      [[fallthrough]];
    }
    case sql::INIT_TOO_NEW: {
      diagnostics_string_ += sql::GetCorruptFileDiagnosticsInfo(history_name);
      delegate_->NotifyProfileError(status, diagnostics_string_);
      db_.reset();
      return;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }

  // Fill the in-memory database and send it back to the history service on the
  // main thread.
  {
    std::unique_ptr<InMemoryHistoryBackend> mem_backend(
        new InMemoryHistoryBackend);
    if (mem_backend->Init(history_name))
      delegate_->SetInMemoryBackend(std::move(mem_backend));
  }
  db_->BeginExclusiveMode();  // Must be after the mem backend read the data.

  // Favicon database.
  favicon_backend_ = favicon::FaviconBackend::Create(favicon_name, this);
  // Unlike the main database, we don't error out if the favicon database can't
  // be created. Generally, this shouldn't happen since the favicon and main
  // database versions should be in sync. We'll just continue without favicons
  // in this case or any other error.

  // Generate the history and favicon database metrics only after performing
  // any migration work.
  if (base::RandInt(1, 100) == 50) {
    // Only do this computation sometimes since it can be expensive.
    db_->ComputeDatabaseMetrics(history_name);
  }

  favicon::FaviconDatabase* favicon_db_ptr =
      favicon_backend_ ? favicon_backend_->db() : nullptr;

  expirer_.SetDatabases(db_.get(), favicon_db_ptr);

  // Open the long-running transaction.
  BeginSingletonTransaction();

  // Get the first item in our database.
  db_->GetStartDate(&first_recorded_time_);

  // Start expiring old stuff.
  expirer_.StartExpiringOldStuff(base::Days(kExpireDaysThreshold));
}

void HistoryBackend::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  // TODO(sebmarchand): Check if MEMORY_PRESSURE_LEVEL_MODERATE should also be
  // ignored.
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }
  if (db_)
    db_->TrimMemory();
  if (favicon_backend_)
    favicon_backend_->TrimMemory();
}

void HistoryBackend::CloseAllDatabases() {
  // Reset to avoid dangling pointers to the database.
  history_sync_bridge_.reset();
  expirer_.SetDatabases(/*main_db=*/nullptr, /*favicon_db=*/nullptr);
  if (db_) {
    CommitSingletonTransactionIfItExists();
    db_.reset();
    // Forget the first recorded time since the database is closed.
    first_recorded_time_ = base::Time();
  }
  favicon_backend_.reset();
}

std::pair<URLID, VisitID> HistoryBackend::AddPageVisit(
    const GURL& url,
    Time time,
    VisitID referring_visit,
    const GURL& external_referrer_url,
    ui::PageTransition transition,
    bool hidden,
    VisitSource visit_source,
    bool should_increment_typed_count,
    VisitID opener_visit,
    bool consider_for_ntp_most_visited,
    std::optional<int64_t> local_navigation_id,
    std::optional<std::u16string> title,
    std::optional<GURL> top_level_url,
    std::optional<GURL> frame_url,
    std::optional<std::string> app_id,
    std::optional<base::TimeDelta> visit_duration,
    std::optional<std::string> originator_cache_guid,
    std::optional<VisitID> originator_visit_id,
    std::optional<VisitID> originator_referring_visit,
    std::optional<VisitID> originator_opener_visit,
    bool is_known_to_sync,
    bool is_ephemeral) {
  DCHECK(url.is_valid());
  // See if this URL is already in the DB.
  URLRow url_info(url);
  URLID url_id = db_->GetRowForURL(url, &url_info);
  if (url_id) {
    // Update of an existing row.
    if (!ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD))
      url_info.set_visit_count(url_info.visit_count() + 1);
    if (should_increment_typed_count)
      url_info.set_typed_count(url_info.typed_count() + 1);
    if (url_info.last_visit() < time)
      url_info.set_last_visit(time);
    if (title)
      url_info.set_title(title.value());

    // Only allow un-hiding of pages, never hiding.
    if (!hidden)
      url_info.set_hidden(false);

    db_->UpdateURLRow(url_id, url_info);
  } else {
    // Addition of a new row.
    url_info.set_visit_count(1);
    url_info.set_typed_count(should_increment_typed_count ? 1 : 0);
    url_info.set_last_visit(time);
    if (title)
      url_info.set_title(title.value());
    url_info.set_hidden(hidden);

    url_id = db_->AddURL(url_info);
    if (!url_id) {
      DLOG(ERROR) << "AddPageVisit: Adding URL failed: " << url_info.url();
      return std::make_pair(0, 0);
    }
    url_info.set_id(url_id);
  }

  VisitedLinkRow visited_link_info;
  if (base::FeatureList::IsEnabled(kPopulateVisitedLinkDatabase)) {
    // Returns whether or not the current row should be added to the
    // VisitedLinkDatabase
    if (AddToVisitedLinkDatabase(transition, top_level_url, frame_url,
                                 is_ephemeral)) {
      // Determine if the visited link is already in the database.
      VisitedLinkID existing_row_id = db_->GetRowForVisitedLink(
          url_id, *top_level_url, *frame_url, visited_link_info);
      // If the returned row id is valid, we update this existing row.
      if (existing_row_id) {
        if (!db_->UpdateVisitedLinkRowVisitCount(
                existing_row_id, visited_link_info.visit_count + 1)) {
          // If the update fails, log an error and return.
          DLOG(ERROR) << "AddPageVisit: Updating VisitedLink failed: " << url
                      << " " << *top_level_url << " " << *frame_url;
          return std::make_pair(0, 0);
        }
      } else {  // otherwise, insert this new visited link.
        VisitedLinkID new_row_id =
            db_->AddVisitedLink(url_id, *top_level_url, *frame_url, 1);
        if (!new_row_id) {
          // If the insert fails, log an error and return.
          DLOG(ERROR) << "AddPageVisit: Inserting VisitedLink failed: " << url
                      << " " << *top_level_url << " " << *frame_url;
          return std::make_pair(0, 0);
        }
        db_->GetVisitedLinkRow(new_row_id, visited_link_info);
      }
    }
  }

  // Add the visit with the time to the database.
  VisitRow visit_info(url_id, time, referring_visit, transition,
                      /*arg_segment_id=*/0, should_increment_typed_count,
                      opener_visit);
  visit_info.external_referrer_url = external_referrer_url;
  if (visit_duration.has_value())
    visit_info.visit_duration = *visit_duration;
  if (originator_cache_guid.has_value())
    visit_info.originator_cache_guid = *originator_cache_guid;
  if (originator_visit_id.has_value())
    visit_info.originator_visit_id = *originator_visit_id;
  if (originator_referring_visit.has_value())
    visit_info.originator_referring_visit = *originator_referring_visit;
  if (originator_opener_visit.has_value())
    visit_info.originator_opener_visit = *originator_opener_visit;
  if (visited_link_info.id) {
    visit_info.visited_link_id = visited_link_info.id;
  }

  // TODO(crbug.com/40280017): any visit added via sync should not have a
  // corresponding entry in the VisitedLinkDatabase.
  if (visit_source == VisitSource::SOURCE_SYNCED) {
    CHECK(visit_info.visited_link_id == kInvalidVisitedLinkID);
  }

  visit_info.is_known_to_sync = is_known_to_sync;
  visit_info.consider_for_ntp_most_visited = consider_for_ntp_most_visited;
  visit_info.app_id = app_id;
  visit_info.visit_id = db_->AddVisit(&visit_info, visit_source);

  if (visit_info.visit_time < first_recorded_time_)
    first_recorded_time_ = visit_info.visit_time;

  // Broadcast a notification of the visit.
  if (visit_info.visit_id) {
    NotifyURLVisited(url_info, visit_info, local_navigation_id);
  } else {
    DLOG(ERROR) << "Failed to build visit insert statement:  "
                << "url_id = " << url_id;
  }

  return std::make_pair(url_id, visit_info.visit_id);
}

// TODO(crbug.com/40279741): Determine if we want to record these URLs in the
// VisitedLinkDatabase, and if so, plumb the correct value for `top_level_site`.
void HistoryBackend::AddPagesWithDetails(const URLRows& urls,
                                         VisitSource visit_source) {
  TRACE_EVENT0("browser", "HistoryBackend::AddPagesWithDetails");

  if (!db_)
    return;

  URLRows changed_urls;
  for (auto i = urls.begin(); i != urls.end(); ++i) {
    DCHECK(!i->last_visit().is_null());

    // As of M37, we no longer maintain an archived database, ignore old visits.
    if (IsExpiredVisitTime(i->last_visit()))
      continue;

    URLRow existing_url;
    URLID url_id = db_->GetRowForURL(i->url(), &existing_url);
    if (!url_id) {
      // Add the page if it doesn't exist.
      url_id = db_->AddURL(*i);
      if (!url_id) {
        DLOG(ERROR) << "AddPagesWithDetails: Adding URL failed: " << i->url();
        return;
      }

      changed_urls.push_back(*i);
      changed_urls.back().set_id(url_id);  // i->id_ is likely 0.
    }

    // Sync code manages the visits itself.
    if (visit_source != SOURCE_SYNCED) {
      // Make up a visit to correspond to the last visit to the page.
      VisitRow visit_info(
          url_id, i->last_visit(), /*arg_referring_visit=*/0,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                    ui::PAGE_TRANSITION_CHAIN_START |
                                    ui::PAGE_TRANSITION_CHAIN_END),
          /*arg_segment_id=*/0, /*arg_incremented_omnibox_typed_score=*/false,
          /*arg_opener_visit=*/0);
      if (!db_->AddVisit(&visit_info, visit_source)) {
        DLOG(ERROR) << "AddPagesWithDetails: Adding visit failed: " << i->url();
        return;
      }

      if (visit_info.visit_time < first_recorded_time_)
        first_recorded_time_ = visit_info.visit_time;
    }
  }

  // Broadcast a notification for typed URLs that have been modified. This
  // will be picked up by the in-memory URL database on the main thread.
  //
  // TODO(brettw) bug 1140015: Add an "add page" notification so the history
  // views can keep in sync.
  NotifyURLsModified(changed_urls, /*is_from_expiration=*/false);
  ScheduleCommit();
}

bool HistoryBackend::IsExpiredVisitTime(const base::Time& time) const {
  return time < expirer_.GetCurrentExpirationTime();
}

// static
int HistoryBackend::GetForeignVisitsToDeletePerBatchForTest() {
  return kSyncHistoryForeignVisitsToDeletePerBatch;
}

sql::Database& HistoryBackend::GetDBForTesting() {
  return db_->GetDBForTesting();  // IN-TEST
}

void HistoryBackend::SetPageTitle(const GURL& url,
                                  const std::u16string& title) {
  TRACE_EVENT0("browser", "HistoryBackend::SetPageTitle");

  if (!db_)
    return;

  // Search for recent redirects which should get the same title. We make a
  // dummy list containing the exact URL visited if there are no redirects so
  // the processing below can be the same.
  RedirectList dummy_list;
  RedirectList* redirects;
  auto iter = recent_redirects_.Get(url);
  if (iter != recent_redirects_.end()) {
    redirects = &iter->second;

    // This redirect chain should have the destination URL as the last item.
    DCHECK(!redirects->empty());
    DCHECK_EQ(redirects->back(), url);
  } else {
    // No redirect chain stored, make up one containing the URL we want so we
    // can use the same logic below.
    dummy_list.push_back(url);
    redirects = &dummy_list;
  }

  URLRows changed_urls;
  for (const auto& redirect : *redirects) {
    URLRow row;
    URLID row_id = db_->GetRowForURL(redirect, &row);
    if (row_id && row.title() != title) {
      row.set_title(title);
      db_->UpdateURLRow(row_id, row);
      changed_urls.push_back(row);
    }
  }

  // Broadcast notifications for any URLs that have changed. This will
  // update the in-memory database and the InMemoryURLIndex.
  if (!changed_urls.empty()) {
    NotifyURLsModified(changed_urls, /*is_from_expiration=*/false);
    ScheduleCommit();
  }
}

void HistoryBackend::AddPageNoVisitForBookmark(const GURL& url,
                                               const std::u16string& title) {
  TRACE_EVENT0("browser", "HistoryBackend::AddPageNoVisitForBookmark");
  DCHECK(url.is_valid());

  if (!db_)
    return;

  URLRow url_info(url);
  URLID url_id = db_->GetRowForURL(url, &url_info);
  if (url_id) {
    // URL is already known, nothing to do.
    return;
  }

  if (!title.empty()) {
    url_info.set_title(title);
  } else {
    url_info.set_title(base::UTF8ToUTF16(url.spec()));
  }

  url_info.set_last_visit(Time::Now());
  // Mark the page hidden. If the user types it in, it'll unhide.
  url_info.set_hidden(true);

  db_->AddURL(url_info);
}

bool HistoryBackend::CanAddURL(const GURL& url) const {
  return delegate_->CanAddURL(url);
}

bool HistoryBackend::GetAllTypedURLs(URLRows* urls) {
  DCHECK(urls);
  if (!db_)
    return false;
  std::vector<URLID> url_ids;
  if (!db_->GetAllURLIDsForTransition(ui::PAGE_TRANSITION_TYPED, &url_ids))
    return false;
  urls->reserve(url_ids.size());
  for (const auto& url_id : url_ids) {
    URLRow url;
    if (!db_->GetURLRow(url_id, &url))
      return false;
    urls->push_back(url);
  }
  return true;
}

bool HistoryBackend::GetVisitsForURL(URLID id, VisitVector* visits) {
  if (db_)
    return db_->GetVisitsForURL(id, visits);
  return false;
}

std::map<GURL, VisitRow> HistoryBackend::GetMostRecentVisitForEachURL(
    const std::vector<GURL>& urls) {
  std::map<GURL, VisitRow> visit_rows;
  for (auto url : urls) {
    QueryURLResult result;
    result.success = db_->GetRowForURL(url, &result.row);
    if (result.success) {
      VisitRow visit_row;
      if (db_->GetMostRecentVisitForURL(result.row.id(), &visit_row)) {
        visit_rows[url] = visit_row;
      }
    }
  }
  return visit_rows;
}

bool HistoryBackend::GetMostRecentVisitForURL(URLID id, VisitRow* visit_row) {
  if (db_)
    return db_->GetMostRecentVisitForURL(id, visit_row);
  return false;
}

bool HistoryBackend::GetMostRecentVisitsForURL(URLID id,
                                               int max_visits,
                                               VisitVector* visits) {
  if (db_)
    return db_->GetMostRecentVisitsForURL(id, max_visits, visits);
  return false;
}

QueryURLResult HistoryBackend::GetMostRecentVisitsForGurl(GURL url,
                                                          int max_visits) {
  QueryURLResult result;
  if (db_ && GetURL(url, &result.row) &&
      db_->GetMostRecentVisitsForURL(result.row.id(), max_visits,
                                     &result.visits)) {
    result.success = true;
  }
  return result;
}

bool HistoryBackend::GetForeignVisit(const std::string& originator_cache_guid,
                                     VisitID originator_visit_id,
                                     VisitRow* visit_row) {
  if (!db_)
    return false;

  return db_->GetRowForForeignVisit(originator_cache_guid, originator_visit_id,
                                    visit_row);
}

VisitID HistoryBackend::AddSyncedVisit(
    const GURL& url,
    const std::u16string& title,
    bool hidden,
    const VisitRow& visit,
    const std::optional<VisitContextAnnotations>& context_annotations,
    const std::optional<VisitContentAnnotations>& content_annotations) {
  DCHECK_EQ(visit.visit_id, kInvalidVisitID);
  DCHECK_EQ(visit.url_id, 0);
  DCHECK(!visit.visit_time.is_null());
  DCHECK(!visit.originator_cache_guid.empty());
  DCHECK(visit.is_known_to_sync);

  if (!db_) {
    return kInvalidVisitID;
  }

  if (!CanAddURL(url)) {
    return kInvalidVisitID;
  }

  DCHECK(url.is_valid());

  auto [url_id, visit_id] = AddPageVisit(
      url, visit.visit_time, visit.referring_visit, visit.external_referrer_url,
      visit.transition, hidden, VisitSource::SOURCE_SYNCED,
      IsTypedIncrement(visit.transition), visit.opener_visit,
      visit.consider_for_ntp_most_visited,
      /*local_navigation_id=*/std::nullopt, title,
      /*top_level_url=*/std::nullopt, /*frame_url=*/std::nullopt, visit.app_id,
      visit.visit_duration, visit.originator_cache_guid,
      visit.originator_visit_id, visit.originator_referring_visit,
      visit.originator_opener_visit, visit.is_known_to_sync);

  if (visit_id == kInvalidVisitID) {
    // Adding the page visit failed, do not continue.
    return kInvalidVisitID;
  }

  if (context_annotations) {
    AddContextAnnotationsForVisit(visit_id, *context_annotations);
  }
  if (content_annotations) {
    SetPageLanguageForVisitByVisitID(visit_id,
                                     content_annotations->page_language);
    SetPasswordStateForVisitByVisitID(visit_id,
                                      content_annotations->password_state);
  }

  db_->SetMayContainForeignVisits(true);

  if (can_add_foreign_visits_to_segments_ &&
      CanAddForeignVisitToSegments(visit, local_device_originator_cache_guid_,
                                   sync_device_info_)) {
    AssignSegmentForNewVisit(url, visit.referring_visit, visit_id,
                             visit.transition, visit.visit_time);
  }

  ScheduleCommit();
  return visit_id;
}

VisitID HistoryBackend::UpdateSyncedVisit(
    const GURL& url,
    const std::u16string& title,
    bool hidden,
    const VisitRow& visit,
    const std::optional<VisitContextAnnotations>& context_annotations,
    const std::optional<VisitContentAnnotations>& content_annotations) {
  DCHECK_EQ(visit.visit_id, kInvalidVisitID);
  DCHECK_EQ(visit.url_id, 0);
  DCHECK(!visit.visit_time.is_null());
  DCHECK(!visit.originator_cache_guid.empty());
  DCHECK(visit.is_known_to_sync);

  if (!db_) {
    return kInvalidVisitID;
  }

  VisitRow original_row;
  if (!db_->GetLastRowForVisitByVisitTime(visit.visit_time, &original_row)) {
    return kInvalidVisitID;
  }

  if (original_row.originator_cache_guid != visit.originator_cache_guid) {
    // The existing visit came from a different device; something is wrong.
    return kInvalidVisitID;
  }

  VisitID visit_id = original_row.visit_id;

  // If the existing foreign visit is about to be deleted, don't update it. The
  // HistorySyncBridge will instead create a new visit. (This can happen if Sync
  // gets stopped, then started again before all the old foreign visits are
  // cleaned up.)
  if (visit_id <= db_->GetDeleteForeignVisitsUntilId()) {
    return kInvalidVisitID;
  }

  // If we can't find the corresponding URLRow, or its actual URL doesn't match,
  // something's wrong.
  URLRow url_row;
  if (!db_->GetURLRow(original_row.url_id, &url_row) || url_row.url() != url) {
    return kInvalidVisitID;
  }

  // Update the URLRow - its title may have changed.
  url_row.set_title(title);
  url_row.set_hidden(hidden);
  db_->UpdateURLRow(url_row.id(), url_row);

  VisitRow updated_row = visit;
  // The fields `visit_id` and `url_id` aren't set in visits coming from sync,
  // so take those from the existing row.
  updated_row.visit_id = visit_id;
  updated_row.url_id = original_row.url_id;
  // Similarly, `referring_visit` and `opener_visit` aren't set in visits from
  // sync (they have originator_referring_visit and originator_opener_visit
  // instead.)
  updated_row.referring_visit = original_row.referring_visit;
  updated_row.opener_visit = original_row.opener_visit;

  // `segment_id` is computed locally and not synced, so keep any value from the
  // existing row. It'll be updated below, if necessary.
  updated_row.segment_id = original_row.segment_id;

  // TODO(crbug.com/40280017): any VisitedLinkID associated with `updated_row`
  // will be voided to avoid storing stale/incorrect VisitedLinkIDs once
  // elements of the VisitRow's partition key change (in this case the
  // referring_visit).
  if (!db_->UpdateVisitRow(updated_row)) {
    return kInvalidVisitID;
  }

  if (can_add_foreign_visits_to_segments_) {
    UpdateSegmentForExistingForeignVisit(updated_row);
  }

  // If provided, add or update the ContextAnnotations.
  if (context_annotations) {
    VisitContextAnnotations existing_annotations;
    if (db_->GetContextAnnotationsForVisit(visit_id, &existing_annotations)) {
      // Update the existing annotations with the fields actually used/populated
      // by Sync - for now, that's exactly the on-visit fields.
      existing_annotations.on_visit = context_annotations->on_visit;
      db_->UpdateContextAnnotationsForVisit(visit_id, existing_annotations);
    } else {
      db_->AddContextAnnotationsForVisit(visit_id, *context_annotations);
    }
  }

  // If provided, add or update the ContentAnnotations.
  if (content_annotations) {
    SetPageLanguageForVisitByVisitID(visit_id,
                                     content_annotations->page_language);
    SetPasswordStateForVisitByVisitID(visit_id,
                                      content_annotations->password_state);
  }

  NotifyVisitUpdated(updated_row, VisitUpdateReason::kUpdateSyncedVisit);
  ScheduleCommit();
  return updated_row.visit_id;
}

bool HistoryBackend::UpdateVisitReferrerOpenerIDs(VisitID visit_id,
                                                  VisitID referrer_id,
                                                  VisitID opener_id) {
  if (!db_)
    return false;

  VisitRow row;
  if (!db_->GetRowForVisit(visit_id, &row))
    return false;

  row.referring_visit = referrer_id;
  row.opener_visit = opener_id;

  // TODO(crbug.com/40280017): any VisitedLinkID associated with `row`
  // will be voided to avoid storing stale/incorrect VisitedLinkIDs once
  // elements of the VisitRow's partition key change (in this case the
  // referring_visit).
  bool result = db_->UpdateVisitRow(row);

  if (result && can_add_foreign_visits_to_segments_) {
    UpdateSegmentForExistingForeignVisit(row);
  }

  ScheduleCommit();

  return result;
}

void HistoryBackend::DeleteAllForeignVisitsAndResetIsKnownToSync() {
  if (!db_)
    return;

  if (db_->KnownToSyncVisitsExist()) {
    db_->SetKnownToSyncVisitsExist(false);
    // It might be bad performance that we do a full table scan setting a bit
    // right before we delete all the foreign visits. In practice, I bet it
    // doesn't matter, since sync turnoffs are rare, and sequencing this after
    // completing the foreign visit deletion adds code complexity.
    db_->SetAllVisitsAsNotKnownToSync();
  }

  // Skip this if the DB doesn't contain any foreign visits, or all the foreign
  // visits are already scheduled for deletion - nothing to do.
  if (db_->MayContainForeignVisits()) {
    bool already_running =
        db_->GetDeleteForeignVisitsUntilId() != kInvalidVisitID;

    // Set the max-foreign-visit-to-delete to the current max visit ID in the
    // DB. This ensures that any visits added in the future (after the
    // DeleteAllForeignVisits() call) will not be affected. (This matters if
    // Sync gets enabled again, and starts adding foreign visits again, before
    // the deletion process has completed.)
    VisitID max_visit_to_delete = db_->GetMaxVisitIDInUse();
    db_->SetDeleteForeignVisitsUntilId(max_visit_to_delete);
    // Already set the "may contain foreign visits" bit to false, since all the
    // existing foreign visits are about to be deleted. This ensures that the
    // bit can be safely set to true again if new foreign visits are added, even
    // before the deletion completes.
    db_->SetMayContainForeignVisits(false);

    // Only schedule a deletion task if there isn't one already running. If
    // there is one already running, it'll pick up the new limit automatically.
    if (!already_running) {
      StartDeletingForeignVisits();
    }
  }
}

bool HistoryBackend::RemoveVisits(const VisitVector& visits,
                                  DeletionInfo::Reason deletion_reason) {
  if (!db_)
    return false;

  expirer_.ExpireVisits(visits, deletion_reason);
  ScheduleCommit();
  return true;
}

bool HistoryBackend::GetVisitsSource(const VisitVector& visits,
                                     VisitSourceMap* sources) {
  if (!db_)
    return false;

  db_->GetVisitsSource(visits, sources);
  return true;
}

bool HistoryBackend::GetVisitSource(const VisitID visit_id,
                                    VisitSource* source) {
  if (!db_)
    return false;

  *source = db_->GetVisitSource(visit_id);
  return true;
}

bool HistoryBackend::GetURL(const GURL& url, URLRow* url_row) {
  if (db_)
    return db_->GetRowForURL(url, url_row) != 0;
  return false;
}

bool HistoryBackend::GetURLByID(URLID url_id, URLRow* url_row) {
  if (db_)
    return db_->GetURLRow(url_id, url_row);
  return false;
}

bool HistoryBackend::GetVisitByID(VisitID visit_id, VisitRow* visit_row) {
  if (db_)
    return db_->GetRowForVisit(visit_id, visit_row);
  return false;
}

bool HistoryBackend::GetLastVisitByTime(base::Time visit_time,
                                        VisitRow* visit_row) {
  if (db_)
    return db_->GetLastRowForVisitByVisitTime(visit_time, visit_row);
  return false;
}

QueryURLResult HistoryBackend::QueryURL(const GURL& url, bool want_visits) {
  QueryURLResult result;
  result.success = db_ && db_->GetRowForURL(url, &result.row);
  // Optionally query the visits.
  if (result.success && want_visits)
    db_->GetVisitsForURL(result.row.id(), &result.visits);
  return result;
}

std::vector<QueryURLResult> HistoryBackend::QueryURLs(
    const std::vector<GURL>& urls,
    bool want_visits) {
  std::vector<QueryURLResult> results;
  for (auto url : urls) {
    results.push_back(QueryURL(url, want_visits));
  }
  return results;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
HistoryBackend::GetHistorySyncControllerDelegate() {
  if (history_sync_bridge_) {
    return history_sync_bridge_->change_processor()->GetControllerDelegate();
  }
  return nullptr;
}

void HistoryBackend::SetSyncTransportState(
    syncer::SyncService::TransportState state) {
  if (history_sync_bridge_) {
    history_sync_bridge_->SetSyncTransportState(state);
  }
}

// Statistics ------------------------------------------------------------------

HistoryCountResult HistoryBackend::GetHistoryCount(const Time& begin_time,
                                                   const Time& end_time) {
  int count = 0;
  return {db_ && db_->GetHistoryCount(begin_time, end_time, &count), count};
}

HistoryCountResult HistoryBackend::CountUniqueHostsVisitedLastMonth() {
  return {!!db_, db_ ? db_->CountUniqueHostsVisitedLastMonth() : 0};
}

std::pair<DomainDiversityResults, DomainDiversityResults>
HistoryBackend::GetDomainDiversity(
    base::Time report_time,
    int number_of_days_to_report,
    DomainMetricBitmaskType metric_type_bitmask) {
  DCHECK_GE(number_of_days_to_report, 0);
  DomainDiversityResults local_result;
  DomainDiversityResults all_result;

  if (!db_)
    return std::make_pair(local_result, all_result);

  number_of_days_to_report =
      std::min(number_of_days_to_report, kDomainDiversityMaxBacktrackedDays);

  base::Time current_midnight = report_time.LocalMidnight();

  for (int days_back = 0; days_back < number_of_days_to_report; ++days_back) {
    DomainMetricSet local_metric_set;
    local_metric_set.end_time = current_midnight;
    DomainMetricSet all_metric_set;
    all_metric_set.end_time = current_midnight;

    if (metric_type_bitmask & kEnableLast1DayMetric) {
      base::Time last_midnight = MidnightNDaysLater(current_midnight, -1);
      auto [local_domains, all_domains] =
          db_->CountUniqueDomainsVisited(last_midnight, current_midnight);
      local_metric_set.one_day_metric =
          DomainMetricCountType(local_domains, last_midnight);
      all_metric_set.one_day_metric =
          DomainMetricCountType(all_domains, last_midnight);
    }

    if (metric_type_bitmask & kEnableLast7DayMetric) {
      base::Time seven_midnights_ago = MidnightNDaysLater(current_midnight, -7);
      auto [local_domains, all_domains] =
          db_->CountUniqueDomainsVisited(seven_midnights_ago, current_midnight);
      local_metric_set.seven_day_metric =
          DomainMetricCountType(local_domains, seven_midnights_ago);
      all_metric_set.seven_day_metric =
          DomainMetricCountType(all_domains, seven_midnights_ago);
    }

    if (metric_type_bitmask & kEnableLast28DayMetric) {
      base::Time twenty_eight_midnights_ago =
          MidnightNDaysLater(current_midnight, -28);
      auto [local_domains, all_domains] = db_->CountUniqueDomainsVisited(
          twenty_eight_midnights_ago, current_midnight);
      local_metric_set.twenty_eight_day_metric =
          DomainMetricCountType(local_domains, twenty_eight_midnights_ago);
      all_metric_set.twenty_eight_day_metric =
          DomainMetricCountType(all_domains, twenty_eight_midnights_ago);
    }
    local_result.push_back(local_metric_set);
    all_result.push_back(all_metric_set);

    current_midnight = MidnightNDaysLater(current_midnight, -1);
  }

  return std::make_pair(local_result, all_result);
}

DomainsVisitedResult HistoryBackend::GetUniqueDomainsVisited(
    base::Time begin_time,
    base::Time end_time) {
  if (!db_) {
    return {};
  }

  return db_->GetUniqueDomainsVisited(begin_time, end_time);
}

GetAllAppIdsResult HistoryBackend::GetAllAppIds() {
  if (!db_) {
    return {};
  }
  return db_->GetAllAppIds();
}

HistoryLastVisitResult HistoryBackend::GetLastVisitToHost(
    const std::string& host,
    base::Time begin_time,
    base::Time end_time) {
  base::Time last_visit;
  return {
      db_ && db_->GetLastVisitToHost(host, begin_time, end_time, &last_visit),
      last_visit};
}

HistoryLastVisitResult HistoryBackend::GetLastVisitToOrigin(
    const url::Origin& origin,
    base::Time begin_time,
    base::Time end_time) {
  base::Time last_visit;
  return {db_ && db_->GetLastVisitToOrigin(origin, begin_time, end_time,
                                           &last_visit),
          last_visit};
}

HistoryLastVisitResult HistoryBackend::GetLastVisitToURL(const GURL& url,
                                                         base::Time end_time) {
  base::Time last_visit;
  return {
      db_ && db_->GetLastVisitToURL(url, end_time, &last_visit),
      last_visit,
  };
}

DailyVisitsResult HistoryBackend::GetDailyVisitsToHost(const GURL& host,
                                                       base::Time begin_time,
                                                       base::Time end_time) {
  if (!db_) {
    return {};
  }
  return db_->GetDailyVisitsToHost(host, begin_time, end_time);
}

// Keyword visits --------------------------------------------------------------

void HistoryBackend::SetKeywordSearchTermsForURL(const GURL& url,
                                                 KeywordID keyword_id,
                                                 const std::u16string& term) {
  TRACE_EVENT0("browser", "HistoryBackend::SetKeywordSearchTermsForURL");

  if (!db_)
    return;

  // Get the ID for this URL.
  URLRow row;
  if (!db_->GetRowForURL(url, &row)) {
    // There is a small possibility the url was deleted before the keyword
    // was added. Ignore the request.
    return;
  }

  db_->SetKeywordSearchTermsForURL(row.id(), keyword_id, term);
  delegate_->NotifyKeywordSearchTermUpdated(row, keyword_id, term);

  ScheduleCommit();
}

void HistoryBackend::DeleteAllSearchTermsForKeyword(KeywordID keyword_id) {
  TRACE_EVENT0("browser", "HistoryBackend::DeleteAllSearchTermsForKeyword");

  if (!db_)
    return;

  db_->DeleteAllSearchTermsForKeyword(keyword_id);
  ScheduleCommit();
}

void HistoryBackend::DeleteKeywordSearchTermForURL(const GURL& url) {
  TRACE_EVENT0("browser", "HistoryBackend::DeleteKeywordSearchTermForURL");

  if (!db_)
    return;

  URLID url_id = db_->GetRowForURL(url, nullptr);
  if (!url_id)
    return;
  db_->DeleteKeywordSearchTermForURL(url_id);
  delegate_->NotifyKeywordSearchTermDeleted(url_id);

  ScheduleCommit();
}

void HistoryBackend::DeleteMatchingURLsForKeyword(KeywordID keyword_id,
                                                  const std::u16string& term) {
  TRACE_EVENT0("browser", "HistoryBackend::DeleteMatchingURLsForKeyword");

  if (!db_)
    return;

  std::vector<KeywordSearchTermRow> rows;
  if (db_->GetKeywordSearchTermRows(term, &rows)) {
    std::vector<GURL> items_to_delete;
    URLRow url_row;
    for (const auto& row : rows) {
      if (row.keyword_id == keyword_id && db_->GetURLRow(row.url_id, &url_row))
        items_to_delete.push_back(url_row.url());
    }
    DeleteURLs(items_to_delete);
  }
}

// Clusters --------------------------------------------------------------------

void HistoryBackend::AddContextAnnotationsForVisit(
    VisitID visit_id,
    const VisitContextAnnotations& visit_context_annotations) {
  TRACE_EVENT0("browser", "HistoryBackend::AddContextAnnotationsForVisit");
  DCHECK(visit_id);
  VisitRow visit_row;
  if (!db_ || !db_->GetRowForVisit(visit_id, &visit_row))
    return;
  db_->AddContextAnnotationsForVisit(visit_id, visit_context_annotations);
  NotifyVisitUpdated(visit_row, VisitUpdateReason::kAddContextAnnotations);
  ScheduleCommit();
}

void HistoryBackend::SetOnCloseContextAnnotationsForVisit(
    VisitID visit_id,
    const VisitContextAnnotations& visit_context_annotations) {
  TRACE_EVENT0("browser",
               "HistoryBackend::SetOnCloseContextAnnotationsForVisit");
  DCHECK(visit_id);
  VisitRow visit_row;
  if (!db_ || !db_->GetRowForVisit(visit_id, &visit_row))
    return;
  VisitContextAnnotations existing_annotations;
  if (db_->GetContextAnnotationsForVisit(visit_id, &existing_annotations)) {
    // Retain the on-visit fields of the existing annotations.
    VisitContextAnnotations merged_annotations = visit_context_annotations;
    merged_annotations.on_visit = existing_annotations.on_visit;
    db_->UpdateContextAnnotationsForVisit(visit_id, merged_annotations);
  } else {
    db_->AddContextAnnotationsForVisit(visit_id, visit_context_annotations);
  }
  NotifyVisitUpdated(visit_row,
                     VisitUpdateReason::kSetOnCloseContextAnnotations);
  ScheduleCommit();
}

std::vector<AnnotatedVisit> HistoryBackend::GetAnnotatedVisits(
    const QueryOptions& options,
    bool compute_redirect_chain_start_properties,
    bool get_unclustered_visits_only,
    bool* limited_by_max_count) {
  // Gets `VisitVector` matching `options`, then for each visit, gets the
  // associated `URLRow`, `VisitContextAnnotations`, and
  // `VisitContentAnnotations`.

  TRACE_EVENT0("browser", "HistoryBackend::GetAnnotatedVisits");
  if (!db_)
    return {};

  // TODO(tommycli): This whole method looks very similar to QueryHistoryBasic,
  //  and even returns a similar structure. We should investigate combining the
  //  two, while somehow still avoiding fetching unnecessary fields, such as
  //  `VisitContextAnnotations`. Probably we need to expand `QueryOptions`.
  VisitVector visit_rows;

  // Set the optional out-param if it's non-nullptr.
  bool limited = db_->GetVisibleVisitsInRange(options, &visit_rows);
  if (limited_by_max_count) {
    *limited_by_max_count = limited;
  }

  if (get_unclustered_visits_only) {
    auto remove_it = base::ranges::remove_if(
        visit_rows.begin(), visit_rows.end(), [&](auto& visit) {
          // This may seem slow, but it's an indexed lookup.
          return db_->GetClusterIdContainingVisit(visit.visit_id) > 0;
        });
    visit_rows.erase(remove_it, visit_rows.end());
  }

  DCHECK_LE(static_cast<int>(visit_rows.size()), options.EffectiveMaxCount());

  return ToAnnotatedVisitsFromRows(visit_rows,
                                   compute_redirect_chain_start_properties);
}

std::vector<AnnotatedVisit> HistoryBackend::ToAnnotatedVisitsFromRows(
    const VisitVector& visit_rows,
    bool compute_redirect_chain_start_properties) {
  if (!db_)
    return {};

  VisitSourceMap sources;
  GetVisitsSource(visit_rows, &sources);

  std::vector<AnnotatedVisit> annotated_visits;
  for (const auto& visit_row : visit_rows) {
    // Add a result row for this visit, get the URL info from the DB.
    URLRow url_row;
    if (!db_->GetURLRow(visit_row.url_id, &url_row)) {
      DLOG(ERROR) << "Failed to get id " << visit_row.url_id
                  << " from history.urls.";
      continue;  // DB out of sync and URL doesn't exist, try to recover.
    }

    // The return values for these annotation fetches are not checked for
    // failures, because visits can lack annotations for legitimate reasons.
    // In these cases, the annotations members are left unchanged.
    // TODO(tommycli): Migrate these fields to use std::optional to make the
    //  optional nature more explicit.
    VisitContextAnnotations context_annotations;
    db_->GetContextAnnotationsForVisit(visit_row.visit_id,
                                       &context_annotations);
    VisitContentAnnotations content_annotations;
    db_->GetContentAnnotationsForVisit(visit_row.visit_id,
                                       &content_annotations);

    VisitID referring_visit_of_redirect_chain_start = 0;
    VisitID opener_visit_of_redirect_chain_start = 0;
    if (compute_redirect_chain_start_properties) {
      VisitRow redirect_start = GetRedirectChainStart(visit_row);
      referring_visit_of_redirect_chain_start = redirect_start.referring_visit;
      opener_visit_of_redirect_chain_start = redirect_start.opener_visit;
    }

    const auto source = sources.count(visit_row.visit_id) == 0
                            ? VisitSource::SOURCE_BROWSED
                            : sources[visit_row.visit_id];

    annotated_visits.emplace_back(url_row, visit_row, context_annotations,
                                  content_annotations,
                                  referring_visit_of_redirect_chain_start,
                                  opener_visit_of_redirect_chain_start, source);
  }

  return annotated_visits;
}

std::vector<AnnotatedVisit> HistoryBackend::ToAnnotatedVisitsFromIds(
    const std::vector<VisitID>& visit_ids,
    bool compute_redirect_chain_start_properties) {
  if (!db_)
    return {};
  VisitVector visit_rows;
  for (const auto visit_id : visit_ids) {
    VisitRow visit_row;
    if (db_->GetRowForVisit(visit_id, &visit_row))
      visit_rows.push_back(visit_row);
  }
  return ToAnnotatedVisitsFromRows(visit_rows,
                                   compute_redirect_chain_start_properties);
}

std::vector<ClusterVisit> HistoryBackend::ToClusterVisits(
    const std::vector<VisitID>& visit_ids,
    bool include_duplicates) {
  auto annotated_visits = ToAnnotatedVisitsFromIds(
      visit_ids, /*compute_redirect_chain_start_properties=*/false);
  std::vector<ClusterVisit> cluster_visits;
  std::set<VisitID> seen_duplicate_ids;
  base::ranges::for_each(annotated_visits, [&](const auto& annotated_visit) {
    ClusterVisit cluster_visit =
        db_->GetClusterVisit(annotated_visit.visit_row.visit_id);
    // `cluster_visit` should be valid in the normal flow, but DB corruption can
    // happen.
    if (cluster_visit.annotated_visit.visit_row.visit_id == kInvalidVisitID)
      return;
    cluster_visit.annotated_visit = annotated_visit;
    if (include_duplicates) {
      cluster_visit.duplicate_visits = ToDuplicateClusterVisits(
          db_->GetDuplicateClusterVisitIdsForClusterVisit(
              annotated_visit.visit_row.visit_id));
      base::ranges::for_each(
          cluster_visit.duplicate_visits, [&](const auto& duplicate_visit) {
            seen_duplicate_ids.insert(duplicate_visit.visit_id);
          });
    }
    cluster_visits.push_back(cluster_visit);
  });

  if (include_duplicates && !seen_duplicate_ids.empty()) {
    // Prune out top-level visits that are duplicates elsewhere.
    std::erase_if(cluster_visits, [&](const auto& cluster_visit) {
      return seen_duplicate_ids.contains(
          cluster_visit.annotated_visit.visit_row.visit_id);
    });
  }
  return cluster_visits;
}

std::vector<DuplicateClusterVisit> HistoryBackend::ToDuplicateClusterVisits(
    const std::vector<VisitID>& visit_ids) {
  std::vector<DuplicateClusterVisit> duplicate_cluster_visits;
  for (auto visit_id : visit_ids) {
    VisitRow visit_row;
    URLRow url_row;
    if (db_->GetRowForVisit(visit_id, &visit_row) &&
        GetURLByID(visit_row.url_id, &url_row)) {
      duplicate_cluster_visits.push_back(
          {visit_id, url_row.url(), visit_row.visit_time});
    }
  }
  return duplicate_cluster_visits;
}

base::Time HistoryBackend::FindMostRecentClusteredTime() {
  TRACE_EVENT0("browser", "HistoryBackend::FindMostRecentClusteredTime");
  if (!db_)
    return base::Time::Min();
  // `max_visits` doesn't matter since it's a soft cap and `max_clusters` is 1.
  const auto clusters = GetMostRecentClusters(
      base::Time::Min(), base::Time::Max(),
      /*max_clusters=*/1, /*max_visits_soft_cap=*/0, false);
  // TODO(manukh): If the most recent cluster is invalid (due to DB corruption),
  //  `GetMostRecentClusters()` will return no clusters. We should handle this
  //  case and not assume we've exhausted history.
  return clusters.empty() ? base::Time::Min()
                          : clusters[0]
                                .GetMostRecentVisit()
                                .annotated_visit.visit_row.visit_time;
}

void HistoryBackend::ReplaceClusters(
    const std::vector<int64_t>& ids_to_delete,
    const std::vector<Cluster>& clusters_to_add) {
  TRACE_EVENT0("browser", "HistoryBackend::ReplaceClusters");
  if (!db_)
    return;
  db_->DeleteClusters(ids_to_delete);
  db_->AddClusters(clusters_to_add);
  ScheduleCommit();
}

int64_t HistoryBackend::ReserveNextClusterIdWithVisit(
    const ClusterVisit& cluster_visit) {
  TRACE_EVENT0("browser", "HistoryBackend::ReserveNextClusterIdWithVisit");
  int64_t cluster_id =
      db_ ? db_->ReserveNextClusterId(/*originator_cache_guid=*/"",
                                      /*originator_cluster_id=*/0)
          : 0;
  if (cluster_id == 0) {
    // DB write was not successful, just return.
    return 0;
  }
  AddVisitsToCluster(cluster_id, {cluster_visit});
  return cluster_id;
}

void HistoryBackend::AddVisitsToCluster(
    int64_t cluster_id,
    const std::vector<ClusterVisit>& visits) {
  TRACE_EVENT0("browser", "HistoryBackend::AddVisitsToCluster");
  if (!db_)
    return;

  db_->AddVisitsToCluster(cluster_id, visits);
}

void HistoryBackend::AddVisitToSyncedCluster(
    const history::ClusterVisit& cluster_visit,
    const std::string& originator_cache_guid,
    int64_t originator_cluster_id) {
  TRACE_EVENT0("browser", "HistoryBackend::AddVisitToSyncedCluster");
  if (!db_) {
    return;
  }

  int64_t local_cluster_id = db_->GetClusterIdForSyncedDetails(
      originator_cache_guid, originator_cluster_id);
  if (local_cluster_id == 0) {
    // Reserve a new one since one with the synced details does not already
    // exist.
    local_cluster_id =
        db_->ReserveNextClusterId(originator_cache_guid, originator_cluster_id);
  }
  if (local_cluster_id == 0) {
    // Cluster failed to be added to the DB - unclear if/how this can happen.
    return;
  }

  db_->AddVisitsToCluster(local_cluster_id, {cluster_visit});
}

void HistoryBackend::UpdateClusterTriggerability(
    const std::vector<Cluster>& clusters) {
  TRACE_EVENT0("browser", "HistoryBackend::UpdateClusterTriggerability");
  if (!db_) {
    return;
  }

  db_->UpdateClusterTriggerability(clusters);
}

void HistoryBackend::HideVisits(const std::vector<VisitID>& visit_ids) {
  TRACE_EVENT0("browser", "HistoryBackend::HideVisits");
  if (!db_)
    return;
  db_->HideVisits(visit_ids);
}

void HistoryBackend::UpdateClusterVisit(
    const history::ClusterVisit& cluster_visit) {
  TRACE_EVENT0("browser", "HistoryBackend::UpdateClusterVisit");
  if (!db_) {
    return;
  }

  int64_t cluster_id = db_->GetClusterIdContainingVisit(
      cluster_visit.annotated_visit.visit_row.visit_id);
  if (cluster_id == 0) {
    // No cluster visit persisted, just return.
    return;
  }

  db_->UpdateClusterVisit(cluster_id, cluster_visit);
}

void HistoryBackend::UpdateVisitsInteractionState(
    const std::vector<VisitID>& visit_ids,
    const ClusterVisit::InteractionState interaction_state) {
  TRACE_EVENT0("browser", "HistoryBackend::UpdateVisitsInteractionState");
  if (!db_) {
    return;
  }
  db_->UpdateVisitsInteractionState(visit_ids, interaction_state);
}

std::vector<Cluster> HistoryBackend::GetMostRecentClusters(
    base::Time inclusive_min_time,
    base::Time exclusive_max_time,
    size_t max_clusters,
    size_t max_visits_soft_cap,
    bool include_keywords_and_duplicates) {
  TRACE_EVENT0("browser", "HistoryBackend::GetMostRecentClusters");
  if (!db_)
    return {};
  const auto cluster_ids = db_->GetMostRecentClusterIds(
      inclusive_min_time, exclusive_max_time, max_clusters);
  std::vector<Cluster> clusters;
  size_t accumulated_visits_count = 0;
  for (const auto cluster_id : cluster_ids) {
    const auto cluster =
        GetCluster(cluster_id, include_keywords_and_duplicates);
    // `cluster` should be valid in the normal flow, but DB corruption can
    // happen. `GetCluster()` returning a cluster_id` of 0 indicates an invalid
    // cluster.
    if (cluster.cluster_id > 0) {
      accumulated_visits_count += cluster.visits.size();
      clusters.push_back(std::move(cluster));
      if (accumulated_visits_count >= max_visits_soft_cap)
        break;
    }
  }
  return clusters;
}

Cluster HistoryBackend::GetCluster(int64_t cluster_id,
                                   bool include_keywords_and_duplicates) {
  TRACE_EVENT0("browser", "HistoryBackend::GetCluster");
  if (!db_)
    return {};

  const auto cluster_visits = ToClusterVisits(
      db_->GetVisitIdsInCluster(cluster_id), include_keywords_and_duplicates);
  // `cluster_visits` shouldn't be empty in the normal flow, but DB corruption
  // can happen.
  if (cluster_visits.empty())
    return {};

  Cluster cluster = db_->GetCluster(cluster_id);
  cluster.visits = cluster_visits;
  if (include_keywords_and_duplicates)
    cluster.keyword_to_data_map = db_->GetClusterKeywords(cluster_id);
  return cluster;
}

int64_t HistoryBackend::GetClusterIdContainingVisit(VisitID visit_id) {
  TRACE_EVENT0("browser", "HistoryBackend::GetClusterIdContainingVisit");

  return db_ ? db_->GetClusterIdContainingVisit(visit_id) : 0;
}

VisitRow HistoryBackend::GetRedirectChainStart(VisitRow visit) {
  VisitVector redirect_chain = GetRedirectChain(visit);
  if (redirect_chain.empty())
    return {};
  return redirect_chain.front();
}

VisitVector HistoryBackend::GetRedirectChain(VisitRow visit) {
  // Iterate up `visit.referring_visit` while `visit.transition` is a redirect.
  VisitVector result;
  result.push_back(visit);
  if (db_) {
    base::flat_set<VisitID> visit_set;
    while (!(visit.transition & ui::PAGE_TRANSITION_CHAIN_START)) {
      visit_set.insert(visit.visit_id);
      // `GetRowForVisit()` should not return false if the DB is correct.
      VisitRow referring_visit;
      if (!db_->GetRowForVisit(visit.referring_visit, &referring_visit))
        return {};
      if (visit_set.count(referring_visit.visit_id)) {
        DLOG(WARNING) << "Loop in visit redirect chain, possible db corruption";
        break;
      }
      result.push_back(referring_visit);
      visit = referring_visit;
    }
  }
  std::reverse(result.begin(), result.end());
  return result;
}

// Observers -------------------------------------------------------------------

void HistoryBackend::AddObserver(HistoryBackendObserver* observer) {
  observers_.AddObserver(observer);
}

void HistoryBackend::RemoveObserver(HistoryBackendObserver* observer) {
  observers_.RemoveObserver(observer);
}

// Downloads -------------------------------------------------------------------

uint32_t HistoryBackend::GetNextDownloadId() {
  return db_ ? db_->GetNextDownloadId() : kInvalidDownloadId;
}

// Get all the download entries from the database.
std::vector<DownloadRow> HistoryBackend::QueryDownloads() {
  std::vector<DownloadRow> rows;
  if (db_)
    db_->QueryDownloads(&rows);
  return rows;
}

// Update a particular download entry.
void HistoryBackend::UpdateDownload(const DownloadRow& data,
                                    bool should_commit_immediately) {
  TRACE_EVENT0("browser", "HistoryBackend::UpdateDownload");
  if (!db_)
    return;
  db_->UpdateDownload(data);
  if (should_commit_immediately)
    Commit();
  else
    ScheduleCommit();
}

bool HistoryBackend::CreateDownload(const DownloadRow& history_info) {
  TRACE_EVENT0("browser", "HistoryBackend::CreateDownload");
  if (!db_)
    return false;
  bool success = db_->CreateDownload(history_info);
#if BUILDFLAG(IS_ANDROID)
  // On android, browser process can get easily killed. Download will no longer
  // be able to resume and the temporary file will linger forever if the
  // download is not committed before that. Do the commit right away to avoid
  // uncommitted download entry if browser is killed.
  Commit();
#else
  ScheduleCommit();
#endif
  return success;
}

void HistoryBackend::RemoveDownloads(const std::set<uint32_t>& ids) {
  TRACE_EVENT0("browser", "HistoryBackend::RemoveDownloads");
  if (!db_)
    return;
  size_t downloads_count_before = db_->CountDownloads();
  // HistoryBackend uses a long-running Transaction that is committed
  // periodically, so this loop doesn't actually hit the disk too hard.
  for (uint32_t id : ids)
    db_->RemoveDownload(id);
  ScheduleCommit();
  size_t downloads_count_after = db_->CountDownloads();

  DCHECK_LE(downloads_count_after, downloads_count_before);
  if (downloads_count_after > downloads_count_before)
    return;
  size_t num_downloads_deleted = downloads_count_before - downloads_count_after;
  DCHECK_GE(ids.size(), num_downloads_deleted);
}

QueryResults HistoryBackend::QueryHistory(const std::u16string& text_query,
                                          const QueryOptions& options) {
  QueryResults query_results;
  base::TimeTicks beginning_time = base::TimeTicks::Now();
  if (db_) {
    if (text_query.empty()) {
      // Basic history query for the main database.
      QueryHistoryBasic(options, &query_results);
    } else {
      // Text history query.
      QueryHistoryText(text_query, options, &query_results);
    }
  }
  UMA_HISTOGRAM_TIMES("History.QueryHistory",
                      TimeTicks::Now() - beginning_time);
  return query_results;
}

// Basic time-based querying of history.
void HistoryBackend::QueryHistoryBasic(const QueryOptions& options,
                                       QueryResults* result) {
  // First get all visits.
  VisitVector visits;
  bool has_more_results = db_->GetVisibleVisitsInRange(options, &visits);
  DCHECK_LE(static_cast<int>(visits.size()), options.EffectiveMaxCount());

  // Now add them and the URL rows to the results.
  std::vector<URLResult> matching_results;
  URLResult url_result;
  for (const auto& visit : visits) {
    // Add a result row for this visit, get the URL info from the DB.
    if (!db_->GetURLRow(visit.url_id, &url_result)) {
      DLOG(ERROR) << "Failed to get id " << visit.url_id
                  << " from history.urls.";
      continue;  // DB out of sync and URL doesn't exist, try to recover.
    }

    if (!url_result.url().is_valid()) {
      DVLOG(0) << "Got invalid URL from history.urls with id " << visit.url_id
               << ":  " << url_result.url().possibly_invalid_spec();
      continue;  // Don't report invalid URLs in case of corruption.
    }

    url_result.set_visit_time(visit.visit_time);
    url_result.set_app_id(visit.app_id);

    VisitContentAnnotations content_annotations;
    db_->GetContentAnnotationsForVisit(visit.visit_id, &content_annotations);
    url_result.set_content_annotations(content_annotations);

    // Set whether the visit was blocked for a managed user by looking at the
    // transition type.
    url_result.set_blocked_visit(
        (visit.transition & ui::PAGE_TRANSITION_BLOCKED) != 0);

    // We don't set any of the query-specific parts of the URLResult, since
    // snippets and stuff don't apply to basic querying.
    matching_results.push_back(std::move(url_result));
  }
  result->SetURLResults(std::move(matching_results));

  if (!has_more_results && options.begin_time <= first_recorded_time_)
    result->set_reached_beginning(true);
}

// Text-based querying of history.
void HistoryBackend::QueryHistoryText(const std::u16string& text_query,
                                      const QueryOptions& options,
                                      QueryResults* result) {
  URLRows text_matches =
      options.host_only
          ? GetMatchesForHost(text_query)
          : db_->GetTextMatchesWithAlgorithm(
                text_query, options.matching_algorithm.value_or(
                                query_parser::MatchingAlgorithm::DEFAULT));

  std::vector<URLResult> matching_visits;
  VisitVector visits;  // Declare outside loop to prevent re-construction.
  for (const auto& text_match : text_matches) {
    // Get all visits for given URL match.
    db_->GetVisibleVisitsForURL(text_match.id(), options, &visits);
    for (const auto& visit : visits) {
      URLResult url_result(text_match);
      url_result.set_visit_time(visit.visit_time);
      url_result.set_app_id(visit.app_id);

      VisitContentAnnotations content_annotations;
      db_->GetContentAnnotationsForVisit(visit.visit_id, &content_annotations);
      url_result.set_content_annotations(content_annotations);

      matching_visits.push_back(url_result);
    }
  }

  std::sort(matching_visits.begin(), matching_visits.end(),
            URLResult::CompareVisitTime);

  size_t max_results = options.max_count == 0
                           ? std::numeric_limits<size_t>::max()
                           : static_cast<int>(options.max_count);
  bool has_more_results = false;
  if (matching_visits.size() > max_results) {
    has_more_results = true;
    matching_visits.resize(max_results);
  }
  result->SetURLResults(std::move(matching_visits));

  if (!has_more_results && options.begin_time <= first_recorded_time_)
    result->set_reached_beginning(true);
}

URLRows HistoryBackend::GetMatchesForHost(const std::u16string& host_name) {
  URLRows results;
  URLDatabase::URLEnumerator iter;

  if (db_ && db_->InitURLEnumeratorForEverything(&iter)) {
    URLRow row;
    std::string host_name_utf8 = base::UTF16ToUTF8(host_name);
    while (iter.GetNextURL(&row)) {
      if (row.url().is_valid() && row.url().host() == host_name_utf8) {
        results.push_back(std::move(row));
      }
    }
  }

  return results;
}

RedirectList HistoryBackend::QueryRedirectsFrom(const GURL& from_url) {
  if (!db_)
    return {};

  URLID from_url_id = db_->GetRowForURL(from_url, nullptr);
  VisitID cur_visit = db_->GetMostRecentVisitForURL(from_url_id, nullptr);
  if (!cur_visit)
    return {};  // No visits for URL.

  RedirectList redirects;
  GetRedirectsFromSpecificVisit(cur_visit, &redirects);
  return redirects;
}

RedirectList HistoryBackend::QueryRedirectsTo(const GURL& to_url) {
  if (!db_)
    return {};

  URLID to_url_id = db_->GetRowForURL(to_url, nullptr);
  VisitID cur_visit = db_->GetMostRecentVisitForURL(to_url_id, nullptr);
  if (!cur_visit)
    return {};  // No visits for URL.

  RedirectList redirects;
  GetRedirectsToSpecificVisit(cur_visit, &redirects);
  return redirects;
}

VisibleVisitCountToHostResult HistoryBackend::GetVisibleVisitCountToHost(
    const GURL& url) {
  VisibleVisitCountToHostResult result;
  result.success = db_ && db_->GetVisibleVisitCountToHost(url, &result.count,
                                                          &result.first_visit);
  return result;
}

MostVisitedURLList HistoryBackend::QueryMostVisitedURLs(int result_count) {
  if (!db_)
    return {};

  auto url_filter =
      backend_client_
          ? base::BindRepeating(&HistoryBackendClient::IsWebSafe,
                                base::Unretained(backend_client_.get()))
          : base::NullCallback();
  std::vector<std::unique_ptr<PageUsageData>> data =
      db_->QuerySegmentUsage(result_count, url_filter);

  MostVisitedURLList result;
  for (const std::unique_ptr<PageUsageData>& current_data : data) {
    result.emplace_back(current_data->GetURL(), current_data->GetTitle());
    result.back().visit_count = current_data->GetVisitCount();
    result.back().last_visit_time = current_data->GetLastVisitTimeslot();
    result.back().score = current_data->GetScore();
  }
  return result;
}

KeywordSearchTermVisitList HistoryBackend::QueryMostRepeatedQueriesForKeyword(
    KeywordID keyword_id,
    size_t result_count) {
  if (!db_)
    return {};

  const base::ElapsedTimer query_timer;

  auto enumerator = db_->CreateKeywordSearchTermVisitEnumerator(keyword_id);
  if (!enumerator) {
    return {};
  }

  KeywordSearchTermVisitList search_terms;
  history::GetMostRepeatedSearchTermsFromEnumerator(*enumerator, result_count,
                                                    &search_terms);
  DCHECK_LE(search_terms.size(), result_count);
  base::UmaHistogramTimes("History.QueryMostRepeatedQueriesTimeV2",
                          query_timer.Elapsed());
  return search_terms;
}

void HistoryBackend::GetRedirectsFromSpecificVisit(VisitID cur_visit,
                                                   RedirectList* redirects) {
  // Follow any redirects from the given visit and add them to the list.
  // It *should* be impossible to get a circular chain here, but we check
  // just in case to avoid infinite loops.
  GURL cur_url;
  std::set<VisitID> visit_set;
  visit_set.insert(cur_visit);
  while (db_->GetRedirectFromVisit(cur_visit, &cur_visit, &cur_url)) {
    if (visit_set.find(cur_visit) != visit_set.end()) {
      DUMP_WILL_BE_NOTREACHED() << "Loop in visit chain, giving up";
      return;
    }
    visit_set.insert(cur_visit);
    redirects->push_back(cur_url);
  }
}

void HistoryBackend::GetRedirectsToSpecificVisit(VisitID cur_visit,
                                                 RedirectList* redirects) {
  // Follow redirects going to cur_visit. These are added to `redirects` in
  // the order they are found. If a redirect chain looks like A -> B -> C and
  // `cur_visit` = C, redirects will be {B, A} in that order.
  if (!db_)
    return;

  GURL cur_url;
  std::set<VisitID> visit_set;
  visit_set.insert(cur_visit);
  while (db_->GetRedirectToVisit(cur_visit, &cur_visit, &cur_url)) {
    if (visit_set.find(cur_visit) != visit_set.end()) {
      DUMP_WILL_BE_NOTREACHED() << "Loop in visit chain, giving up";
      return;
    }
    visit_set.insert(cur_visit);
    redirects->push_back(cur_url);
  }
}

void HistoryBackend::DeleteFTSIndexDatabases() {
  // Find files on disk matching the text databases file pattern so we can
  // quickly test for and delete them.
  base::FilePath::StringType filepattern = FILE_PATH_LITERAL("History Index *");
  base::FileEnumerator enumerator(history_dir_, false,
                                  base::FileEnumerator::FILES, filepattern);
  base::FilePath current_file;
  while (!(current_file = enumerator.Next()).empty()) {
    sql::Database::Delete(current_file);
  }
}

std::vector<favicon_base::FaviconRawBitmapResult> HistoryBackend::GetFavicon(
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const std::vector<int>& desired_sizes) {
  return UpdateFaviconMappingsAndFetch({}, icon_url, icon_type, desired_sizes);
}

favicon_base::FaviconRawBitmapResult HistoryBackend::GetLargestFaviconForURL(
    const GURL& page_url,
    const std::vector<favicon_base::IconTypeSet>& icon_types_list,
    int minimum_size_in_pixels) {
  if (!db_ || !favicon_backend_)
    return {};

  return favicon_backend_->GetLargestFaviconForUrl(page_url, icon_types_list,
                                                   minimum_size_in_pixels);
}

std::vector<favicon_base::FaviconRawBitmapResult>
HistoryBackend::GetFaviconsForURL(const GURL& page_url,
                                  const favicon_base::IconTypeSet& icon_types,
                                  const std::vector<int>& desired_sizes,
                                  bool fallback_to_host) {
  if (!favicon_backend_)
    return {};
  return favicon_backend_->GetFaviconsForUrl(page_url, icon_types,
                                             desired_sizes, fallback_to_host);
}

std::vector<favicon_base::FaviconRawBitmapResult>
HistoryBackend::GetFaviconForID(favicon_base::FaviconID favicon_id,
                                int desired_size) {
  if (!favicon_backend_)
    return {};
  return favicon_backend_->GetFaviconForId(favicon_id, desired_size);
}

std::vector<GURL> HistoryBackend::GetFaviconURLsForURL(const GURL& page_url) {
  if (!favicon_backend_)
    return {};
  return favicon_backend_->GetFaviconUrlsForUrl(page_url);
}

std::vector<favicon_base::FaviconRawBitmapResult>
HistoryBackend::UpdateFaviconMappingsAndFetch(
    const base::flat_set<GURL>& page_urls,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const std::vector<int>& desired_sizes) {
  if (!favicon_backend_)
    return {};
  auto result = favicon_backend_->UpdateFaviconMappingsAndFetch(
      page_urls, icon_url, icon_type, desired_sizes);
  if (!result.updated_page_urls.empty()) {
    for (auto& page_url : result.updated_page_urls)
      SendFaviconChangedNotificationForPageAndRedirects(page_url);
    ScheduleCommit();
  }
  return result.bitmap_results;
}

void HistoryBackend::DeleteFaviconMappings(
    const base::flat_set<GURL>& page_urls,
    favicon_base::IconType icon_type) {
  if (!favicon_backend_ || !db_)
    return;

  auto deleted_page_urls =
      favicon_backend_->DeleteFaviconMappings(page_urls, icon_type);
  for (auto& deleted_page_url : deleted_page_urls)
    SendFaviconChangedNotificationForPageAndRedirects(deleted_page_url);
  if (!deleted_page_urls.empty())
    ScheduleCommit();
}

void HistoryBackend::MergeFavicon(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    scoped_refptr<base::RefCountedMemory> bitmap_data,
    const gfx::Size& pixel_size) {
  if (!favicon_backend_ || !db_)
    return;

  favicon::MergeFaviconResult result = favicon_backend_->MergeFavicon(
      page_url, icon_url, icon_type, bitmap_data, pixel_size);
  if (result.did_page_to_icon_mapping_change)
    SendFaviconChangedNotificationForPageAndRedirects(page_url);
  if (result.did_icon_change)
    SendFaviconChangedNotificationForIconURL(icon_url);
  ScheduleCommit();
}

void HistoryBackend::SetFavicons(const base::flat_set<GURL>& page_urls,
                                 favicon_base::IconType icon_type,
                                 const GURL& icon_url,
                                 const std::vector<SkBitmap>& bitmaps) {
  if (!favicon_backend_)
    return;

  ProcessSetFaviconsResult(
      favicon_backend_->SetFavicons(page_urls, icon_type, icon_url, bitmaps,
                                    FaviconBitmapType::ON_VISIT),
      icon_url);
}

void HistoryBackend::CloneFaviconMappingsForPages(
    const GURL& page_url_to_read,
    const favicon_base::IconTypeSet& icon_types,
    const base::flat_set<GURL>& page_urls_to_write) {
  TRACE_EVENT0("browser", "HistoryBackend::CloneFaviconMappingsForPages");

  if (!db_ || !favicon_backend_)
    return;

  std::set<GURL> changed_urls = favicon_backend_->CloneFaviconMappingsForPages(
      page_url_to_read, icon_types, page_urls_to_write);
  if (changed_urls.empty())
    return;

  ScheduleCommit();
  NotifyFaviconsChanged(changed_urls, GURL());
}

bool HistoryBackend::CanSetOnDemandFavicons(const GURL& page_url,
                                            favicon_base::IconType icon_type) {
  return favicon_backend_ && db_ &&
         favicon_backend_->CanSetOnDemandFavicons(page_url, icon_type);
}

bool HistoryBackend::SetOnDemandFavicons(const GURL& page_url,
                                         favicon_base::IconType icon_type,
                                         const GURL& icon_url,
                                         const std::vector<SkBitmap>& bitmaps) {
  if (!favicon_backend_ || !db_)
    return false;

  return ProcessSetFaviconsResult(favicon_backend_->SetOnDemandFavicons(
                                      page_url, icon_type, icon_url, bitmaps),
                                  icon_url);
}

void HistoryBackend::SetFaviconsOutOfDateForPage(const GURL& page_url) {
  if (favicon_backend_ &&
      favicon_backend_->SetFaviconsOutOfDateForPage(page_url)) {
    ScheduleCommit();
  }
}

void HistoryBackend::SetFaviconsOutOfDateBetween(base::Time begin,
                                                 base::Time end) {
  if (favicon_backend_ &&
      favicon_backend_->SetFaviconsOutOfDateBetween(begin, end)) {
    ScheduleCommit();
  }
}

void HistoryBackend::TouchOnDemandFavicon(const GURL& icon_url) {
  TRACE_EVENT0("browser", "HistoryBackend::TouchOnDemandFavicon");

  if (!favicon_backend_)
    return;
  favicon_backend_->TouchOnDemandFavicon(icon_url);
  ScheduleCommit();
}

void HistoryBackend::SetImportedFavicons(
    const favicon_base::FaviconUsageDataList& favicon_usage) {
  TRACE_EVENT0("browser", "HistoryBackend::SetImportedFavicons");

  if (!db_ || !favicon_backend_)
    return;

  Time now = Time::Now();

  // Track all URLs that had their favicons set or updated.
  std::set<GURL> favicons_changed;

  favicon::FaviconDatabase* favicon_db = favicon_backend_->db();
  for (const auto& favicon_usage_data : favicon_usage) {
    favicon_base::FaviconID favicon_id = favicon_db->GetFaviconIDForFaviconURL(
        favicon_usage_data.favicon_url, favicon_base::IconType::kFavicon);
    if (!favicon_id) {
      // This favicon doesn't exist yet, so we create it using the given data.
      // TODO(pkotwicz): Pass in real pixel size.
      favicon_id = favicon_db->AddFavicon(
          favicon_usage_data.favicon_url, favicon_base::IconType::kFavicon,
          new base::RefCountedBytes(favicon_usage_data.png_data),
          FaviconBitmapType::ON_VISIT, now, gfx::Size());
    }

    // Save the mapping from all the URLs to the favicon.
    for (const auto& url : favicon_usage_data.urls) {
      URLRow url_row;
      if (!db_->GetRowForURL(url, &url_row)) {
        // If the URL is present as a bookmark, add the url in history to
        // save the favicon mapping. This will match with what history db does
        // for regular bookmarked URLs with favicons - when history db is
        // cleaned, we keep an entry in the db with 0 visits as long as that
        // url is bookmarked. The same is applicable to the saved credential's
        // URLs.
        if (backend_client_ && backend_client_->IsPinnedURL(url)) {
          DCHECK(url.is_valid());
          URLRow url_info(url);
          url_info.set_visit_count(0);
          url_info.set_typed_count(0);
          url_info.set_last_visit(base::Time());
          url_info.set_hidden(false);
          db_->AddURL(url_info);
          favicon_db->AddIconMapping(url, favicon_id);
          favicons_changed.insert(url);
        }
      } else {
        if (!favicon_db->GetIconMappingsForPageURL(
                url, {favicon_base::IconType::kFavicon},
                /*mapping_data=*/nullptr)) {
          // URL is present in history, update the favicon *only* if it is not
          // set already.
          favicon_db->AddIconMapping(url, favicon_id);
          favicons_changed.insert(url);
        }
      }
    }
  }

  if (!favicons_changed.empty()) {
    // Send the notification about the changed favicon URLs.
    NotifyFaviconsChanged(favicons_changed, GURL());
  }
}

RedirectList HistoryBackend::GetCachedRecentRedirects(const GURL& page_url) {
  auto iter = recent_redirects_.Get(page_url);
  if (iter != recent_redirects_.end()) {
    // The redirect chain should have the destination URL as the last item.
    DCHECK(!iter->second.empty());
    DCHECK_EQ(iter->second.back(), page_url);
    return iter->second;
  }
  // No known redirects, construct mock redirect chain containing `page_url`.
  return RedirectList{page_url};
}

void HistoryBackend::SendFaviconChangedNotificationForPageAndRedirects(
    const GURL& page_url) {
  RedirectList redirect_list = GetCachedRecentRedirects(page_url);
  if (!redirect_list.empty()) {
    std::set<GURL> favicons_changed(redirect_list.begin(), redirect_list.end());
    NotifyFaviconsChanged(favicons_changed, GURL());
  }
}

void HistoryBackend::SendFaviconChangedNotificationForIconURL(
    const GURL& icon_url) {
  NotifyFaviconsChanged(std::set<GURL>(), icon_url);
}

void HistoryBackend::Commit() {
  TRACE_EVENT0("browser", "HistoryBackend::Commit");
  if (!db_)
    return;

#if BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_IOS_APP_EXTENSION)
  // Attempts to get the application running long enough to commit the database
  // transaction if it is currently being backgrounded.
  base::ios::ScopedCriticalAction scoped_critical_action(
      "HistoryBackend::Commit");
#endif

  // Note that a commit may not actually have been scheduled if a caller
  // explicitly calls this instead of using ScheduleCommit. Likewise, we
  // may reset the flag written by a pending commit. But this is OK! It
  // will merely cause extra commits (which is kind of the idea). We
  // could optimize more for this case (we may get two extra commits in
  // some cases) but it hasn't been important yet.
  CancelScheduledCommit();

  CommitSingletonTransactionIfItExists();
  BeginSingletonTransaction();

  // `FaviconBackend` has its OWN internal long-running transaction.
  if (favicon_backend_)
    favicon_backend_->Commit();
}

void HistoryBackend::ScheduleCommit() {
  TRACE_EVENT0("browser", "HistoryBackend::ScheduleCommit");
  // Non-cancelled means there's an already scheduled commit. Note that
  // CancelableOnceClosure starts cancelled with the default constructor.
  if (!scheduled_commit_.IsCancelled())
    return;

  scheduled_commit_.Reset(
      base::BindOnce(&HistoryBackend::Commit, base::Unretained(this)));

  task_runner_->PostDelayedTask(FROM_HERE, scheduled_commit_.callback(),
                                base::Seconds(kCommitIntervalSeconds));
}

void HistoryBackend::CancelScheduledCommit() {
  TRACE_EVENT0("browser", "HistoryBackend::CancelScheduledCommit");
  scheduled_commit_.Cancel();
}

void HistoryBackend::ProcessDBTaskImpl() {
  if (!db_) {
    // db went away, release all the refs.
    queued_history_db_tasks_.clear();
    return;
  }

  // Remove any canceled tasks.
  while (!queued_history_db_tasks_.empty()) {
    QueuedHistoryDBTask* task = queued_history_db_tasks_.front().get();
    if (!task->is_canceled())
      break;

    queued_history_db_tasks_.pop_front();
  }
  if (queued_history_db_tasks_.empty())
    return;

  // Run the first task.
  std::unique_ptr<QueuedHistoryDBTask> task =
      std::move(queued_history_db_tasks_.front());
  queued_history_db_tasks_.pop_front();
  if (task->Run(this, db_.get())) {
    // The task is done, notify the callback.
    task->DoneRun();
  } else {
    // The task wants to run some more. Schedule it at the end of the current
    // tasks.
    queued_history_db_tasks_.push_back(std::move(task));
  }

  // If there are more tasks queued, schedule the next one.
  // Note: Using PostTask() ensures the history sequence gets unblocked for
  // other work.
  if (!queued_history_db_tasks_.empty()) {
    posted_history_db_task_.Reset(
        base::BindOnce(&HistoryBackend::ProcessDBTaskImpl, this));
    task_runner_->PostTask(FROM_HERE, posted_history_db_task_.callback());
  }
}

void HistoryBackend::BeginSingletonTransaction() {
  TRACE_EVENT0("browser", "HistoryBackend::BeginSingletonTransaction");
  DCHECK(!singleton_transaction_);

  DCHECK_EQ(db_->transaction_nesting(), 0);
  singleton_transaction_ = db_->CreateTransaction();

  bool success = singleton_transaction_->Begin();
  if (success) {
    DCHECK_EQ(db_->transaction_nesting(), 1);
  } else {
    // Failing to begin the transaction happens very occasionally in the wild,
    // at about 1 failure per million, almost exclusively on Windows. Previous
    // analysis showed SQLITE_BUSY to be the main cause, which could suggest
    // some other process (could be malware) trying to read Chrome history.
    // See https://crbug.com/1377512 for more discussion.
    //
    // In any case, failing here is not a big deal, because Chrome will try to
    // start another transaction again at the next commit interval. Clear out
    // the `singleton_transaction_` pointer, because it's only kept around if
    // it was successfully begun.
    singleton_transaction_.reset();
  }
}

void HistoryBackend::CommitSingletonTransactionIfItExists() {
  TRACE_EVENT0("browser",
               "HistoryBackend::CommitSingletonTransactionIfItExists");

  if (!singleton_transaction_) {
    DCHECK_EQ(db_->transaction_nesting(), 0)
        << "There should not be any transactions other than the singleton one.";
    return;
  }

  DCHECK_EQ(db_->transaction_nesting(), 1)
      << "Someone opened multiple transactions.";

  bool success = singleton_transaction_->Commit();
  UMA_HISTOGRAM_BOOLEAN("History.Backend.TransactionCommitSuccess", success);
  if (success) {
    DCHECK_EQ(db_->transaction_nesting(), 0)
        << "Someone left a transaction open.";
  } else {
    // The long-running transaction fails to commit about 1 per 100,000 times.
    // The crash reports are again predominantly on Windows. The exact breakdown
    // is less clear here compared to BEGIN, but some logs show "no transaction
    // is active" and some show SQLITE_BUSY. Maybe this UMA will reveal things.
    sql::UmaHistogramSqliteResult("History.Backend.TransactionCommitError",
                                  diagnostics_.reported_sqlite_error_code);
  }
  singleton_transaction_.reset();
}

////////////////////////////////////////////////////////////////////////////////
//
// Generic operations
//
////////////////////////////////////////////////////////////////////////////////

void HistoryBackend::DeleteURLs(const std::vector<GURL>& urls) {
  if (!db_)
    return;

  TRACE_EVENT0("browser", "HistoryBackend::DeleteURLs");

  expirer_.DeleteURLs(urls, base::Time::Max());

  db_->GetStartDate(&first_recorded_time_);
  // Force a commit, if the user is deleting something for privacy reasons, we
  // want to get it on disk ASAP.
  Commit();
}

void HistoryBackend::DeleteURL(const GURL& url) {
  if (!db_)
    return;

  TRACE_EVENT0("browser", "HistoryBackend::DeleteURL");

  expirer_.DeleteURL(url, base::Time::Max());

  db_->GetStartDate(&first_recorded_time_);
  // Force a commit, if the user is deleting something for privacy reasons, we
  // want to get it on disk ASAP.
  Commit();
}

void HistoryBackend::DeleteURLsUntil(
    const std::vector<std::pair<GURL, base::Time>>& urls_and_timestamps) {
  if (!db_)
    return;

  TRACE_EVENT0("browser", "HistoryBackend::DeleteURLsUntil");

  for (const auto& pair : urls_and_timestamps) {
    expirer_.DeleteURL(pair.first, pair.second);
  }
  db_->GetStartDate(&first_recorded_time_);
  // Force a commit, if the user is deleting something for privacy reasons, we
  // want to get it on disk ASAP.
  Commit();
}

void HistoryBackend::ExpireHistoryBetween(
    const std::set<GURL>& restrict_urls,
    std::optional<std::string> restrict_app_id,
    Time begin_time,
    Time end_time,
    bool user_initiated) {
  if (!db_)
    return;

  if (begin_time.is_null() && (end_time.is_null() || end_time.is_max()) &&
      restrict_urls.empty() && !restrict_app_id) {
    // Special case deleting all history so it can be faster and to reduce the
    // possibility of an information leak.
    DeleteAllHistory();
  } else {
    // Clearing parts of history, have the expirer do the depend
    expirer_.ExpireHistoryBetween(restrict_urls, restrict_app_id, begin_time,
                                  end_time, user_initiated);

    // Force a commit, if the user is deleting something for privacy reasons,
    // we want to get it on disk ASAP.
    Commit();
  }

  if (begin_time <= first_recorded_time_)
    db_->GetStartDate(&first_recorded_time_);
}

void HistoryBackend::ExpireHistoryForTimes(const std::set<base::Time>& times,
                                           base::Time begin_time,
                                           base::Time end_time) {
  if (times.empty() || !db_)
    return;

  QueryOptions options;
  options.begin_time = begin_time;
  options.end_time = end_time;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  QueryResults results;
  QueryHistoryBasic(options, &results);

  // 1st pass: find URLs that are visited at one of `times`.
  std::set<GURL> urls;
  for (const auto& result : results) {
    if (times.count(result.visit_time()) > 0)
      urls.insert(result.url());
  }
  if (urls.empty())
    return;

  // 2nd pass: collect all visit times of those URLs.
  std::vector<base::Time> times_to_expire;
  for (const auto& result : results) {
    if (urls.count(result.url()))
      times_to_expire.push_back(result.visit_time());
  }

  // Put the times in reverse chronological order and remove
  // duplicates (for expirer_.ExpireHistoryForTimes()).
  std::sort(times_to_expire.begin(), times_to_expire.end(),
            std::greater<base::Time>());
  times_to_expire.erase(
      std::unique(times_to_expire.begin(), times_to_expire.end()),
      times_to_expire.end());

  // Expires by times and commit.
  DCHECK(!times_to_expire.empty());
  expirer_.ExpireHistoryForTimes(times_to_expire);
  Commit();

  DCHECK_GE(times_to_expire.back(), first_recorded_time_);
  // Update `first_recorded_time_` if we expired it.
  if (times_to_expire.back() == first_recorded_time_)
    db_->GetStartDate(&first_recorded_time_);
}

void HistoryBackend::ExpireHistory(
    const std::vector<ExpireHistoryArgs>& expire_list) {
  if (db_) {
    bool update_first_recorded_time = false;

    for (const auto& expire : expire_list) {
      expirer_.ExpireHistoryBetween(expire.urls, expire.restrict_app_id,
                                    expire.begin_time, expire.end_time, true);

      if (expire.begin_time < first_recorded_time_)
        update_first_recorded_time = true;
    }
    Commit();

    // Update `first_recorded_time_` if any deletion might have affected it.
    if (update_first_recorded_time)
      db_->GetStartDate(&first_recorded_time_);
  }
}

void HistoryBackend::ExpireHistoryBeforeForTesting(base::Time end_time) {
  if (!db_)
    return;

  expirer_.ExpireHistoryBeforeForTesting(end_time);
}

void HistoryBackend::URLsNoLongerBookmarked(const std::set<GURL>& urls) {
  TRACE_EVENT0("browser", "HistoryBackend::URLsNoLongerBookmarked");

  if (!db_)
    return;

  for (const auto& url : urls) {
    VisitVector visits;
    URLRow url_row;
    if (db_->GetRowForURL(url, &url_row))
      db_->GetVisitsForURL(url_row.id(), &visits);
    // We need to call DeleteURL() even if the DB didn't contain this URL, so
    // that we can delete all associated icons in the case of deleting an
    // unvisited bookmarked URL.
    if (visits.empty())
      expirer_.DeleteURL(url, base::Time::Max());
  }
}

void HistoryBackend::DatabaseErrorCallback(int error, sql::Statement* stmt) {
  // Collect Perfetto traces of any database errors, catastrophic or not, so
  // we can detect wrong SQL statements in the wild.
  diagnostics_string_ = db_->GetDiagnosticInfo(error, stmt, &diagnostics_);
  TRACE_EVENT_INSTANT(
      "history", "HistoryBackend::DatabaseErrorCallback",
      perfetto::protos::pbzero::ChromeTrackEvent::kSqlDiagnostics,
      diagnostics_);

  // Raze the database for catastrophic errors.
  if (!scheduled_kill_db_ && sql::IsErrorCatastrophic(error)) {
    scheduled_kill_db_ = true;

    // Don't just do the close/delete here, as we are being called by `db` and
    // that seems dangerous.
    // TODO(crbug.com/41395467): It is also dangerous to kill the database
    // by a posted task: tasks that run before KillHistoryDatabase still can try
    // to use the broken database. Consider protecting against other tasks using
    // the DB or consider changing KillHistoryDatabase() to use RazeAndClose()
    // (then it can be cleared immediately).
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryBackend::KillHistoryDatabase, this));
  }

  // Record UMA at the end because we want to use PREEMPTIVE_TRACING_MODE.
  sql::UmaHistogramSqliteResult("History.DatabaseSqliteError", error);
}

void HistoryBackend::KillHistoryDatabase() {
  TRACE_EVENT0("browser", "HistoryBackend::KillHistoryDatabase");
  scheduled_kill_db_ = false;
  if (!db_)
    return;

  // Notify the sync bridge about storage error. It'll report failures to the
  // sync engine and stop accepting remote updates.
  if (history_sync_bridge_)
    history_sync_bridge_->OnDatabaseError();

  // Rollback transaction because Raze() cannot be called from within a
  // transaction. Deleting the object causes the rollback in the destructor.
  singleton_transaction_.reset();

  db_->Raze();

  // The expirer keeps tabs on the active databases. Tell it about the
  // databases which will be closed.
  expirer_.SetDatabases(nullptr, nullptr);

  CloseAllDatabases();
}

void HistoryBackend::SetSyncDeviceInfo(SyncDeviceInfoMap sync_device_info) {
  sync_device_info_ = std::move(sync_device_info);
}

void HistoryBackend::SetLocalDeviceOriginatorCacheGuid(
    std::string local_device_originator_cache_guid) {
  local_device_originator_cache_guid_ =
      std::move(local_device_originator_cache_guid);
}

void HistoryBackend::SetCanAddForeignVisitsToSegments(bool add_foreign_visits) {
  can_add_foreign_visits_to_segments_ = add_foreign_visits;
}

void HistoryBackend::ProcessDBTask(
    std::unique_ptr<HistoryDBTask> task,
    scoped_refptr<base::SequencedTaskRunner> origin_loop,
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled) {
  TRACE_EVENT0("browser", "HistoryBackend::ProcessDBTask");
  bool scheduled = !queued_history_db_tasks_.empty();
  queued_history_db_tasks_.push_back(std::make_unique<QueuedHistoryDBTask>(
      std::move(task), origin_loop, is_canceled));
  if (!scheduled)
    ProcessDBTaskImpl();
}

void HistoryBackend::RunDBTask(
    base::OnceCallback<void(HistoryBackend*, URLDatabase*)> callback) {
  std::move(callback).Run(this, db_.get());
}

void HistoryBackend::NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                                           const GURL& icon_url) {
  delegate_->NotifyFaviconsChanged(page_urls, icon_url);
}

void HistoryBackend::NotifyURLVisited(
    const URLRow& url_row,
    const VisitRow& visit_row,
    std::optional<int64_t> local_navigation_id) {
  for (HistoryBackendObserver& observer : observers_)
    observer.OnURLVisited(this, url_row, visit_row);

  delegate_->NotifyURLVisited(url_row, visit_row, local_navigation_id);
}

void HistoryBackend::NotifyURLsModified(const URLRows& changed_urls,
                                        bool is_from_expiration) {
  for (HistoryBackendObserver& observer : observers_)
    observer.OnURLsModified(this, changed_urls, is_from_expiration);

  delegate_->NotifyURLsModified(changed_urls);
}

void HistoryBackend::NotifyDeletions(DeletionInfo deletion_info) {
  std::set<GURL> origins;
  for (const history::URLRow& row : deletion_info.deleted_rows())
    origins.insert(row.url().DeprecatedGetOriginAsURL());

  deletion_info.set_deleted_urls_origin_map(
      GetCountsAndLastVisitForOrigins(origins));

  for (HistoryBackendObserver& observer : observers_) {
    observer.OnHistoryDeletions(
        this, deletion_info.IsAllHistory(), deletion_info.is_from_expiration(),
        deletion_info.deleted_rows(), deletion_info.favicon_urls());
  }

  delegate_->NotifyDeletions(std::move(deletion_info));
}

void HistoryBackend::NotifyVisitUpdated(const VisitRow& visit,
                                        VisitUpdateReason reason) {
  for (HistoryBackendObserver& observer : observers_) {
    observer.OnVisitUpdated(visit, reason);
  }
}

void HistoryBackend::NotifyVisitsDeleted(
    const std::vector<DeletedVisit>& visits) {
  std::vector<DeletedVisitedLink> links;
  for (const DeletedVisit& visit : visits) {
    tracker_.RemoveVisitById(visit.visit_row.visit_id);
    for (HistoryBackendObserver& observer : observers_) {
      observer.OnVisitDeleted(visit.visit_row);
    }
    // Determine if a VisitedLink was deleted as a result of the deleted Visit.
    if (visit.deleted_visited_link.has_value()) {
      links.push_back(visit.deleted_visited_link.value());
    }
  }
  // We want to avoid posting a new task for every VisitedLink deleted, so we
  // notify the `delegate_` in a batch.
  if (!links.empty()) {
    delegate_->NotifyVisitedLinksDeleted(links);
  }
}

// Deleting --------------------------------------------------------------------

void HistoryBackend::DeleteAllHistory() {
  // Our approach to deleting all history is:
  //  1. Copy the pinned URLs and their dependencies to new tables with
  //     temporary names.
  //  2. Delete the original tables. Since tables can not share pages, we know
  //     that any data we don't want to keep is now in an unused page.
  //  3. Renaming the temporary tables to match the original.
  //  4. Vacuuming the database to delete the unused pages.
  //
  // Since we are likely to have very few pinned URLs and their dependencies
  // compared to all history, this is also much faster than just deleting from
  // the original tables directly.

  // Get the pinned URLs.
  std::vector<URLAndTitle> pinned_url;
  if (backend_client_)
    pinned_url = backend_client_->GetPinnedURLs();

  URLRows kept_url_rows;
  std::vector<GURL> starred_urls;
  for (URLAndTitle& url_and_title : pinned_url) {
    URLRow row;
    if (db_->GetRowForURL(url_and_title.url, &row)) {
      // Clear the last visit time so when we write these rows they are "clean."
      row.set_last_visit(Time());
      row.set_visit_count(0);
      row.set_typed_count(0);
      kept_url_rows.push_back(row);
    }

    starred_urls.push_back(std::move(url_and_title.url));
  }

  // Delete all cached favicons which are not used by the UI.
  if (!ClearAllFaviconHistory(starred_urls)) {
    DLOG(ERROR) << "Favicon history could not be cleared";
    // We continue in this error case. If the user wants to delete their
    // history, we should delete as much as we can.
  }

  // ClearAllMainHistory will change the IDs of the URLs in kept_urls.
  // Therefore, we clear the list afterwards to make sure nobody uses this
  // invalid data.
  if (!ClearAllMainHistory(kept_url_rows))
    DLOG(ERROR) << "Main history could not be cleared";
  kept_url_rows.clear();

  db_->GetStartDate(&first_recorded_time_);

  tracker_.Clear();

  // Send out the notification that history is cleared. The in-memory database
  // will pick this up and clear itself.
  NotifyDeletions(DeletionInfo::ForAllHistory());
}

bool HistoryBackend::ClearAllFaviconHistory(
    const std::vector<GURL>& kept_urls) {
  if (!favicon_backend_) {
    // When we have no reference to the favicon database, maybe there was an
    // error opening it. In this case, we just try to blow it away to try to
    // fix the error if it exists. This may fail, in which case either the
    // file doesn't exist or there's no more we can do.
    sql::Database::Delete(GetFaviconsFileName());
    return true;
  }
  if (!favicon_backend_->ClearAllExcept(kept_urls))
    return false;

#if BUILDFLAG(IS_ANDROID)
  // TODO(michaelbai): Add the unit test once AndroidProviderBackend is
  // available in HistoryBackend.
  db_->ClearAndroidURLRows();
#endif
  return true;
}

void HistoryBackend::ClearAllOnDemandFavicons() {
  expirer_.ClearOldOnDemandFaviconsIfPossible(base::Time::Now());
}

bool HistoryBackend::ClearAllMainHistory(const URLRows& kept_urls) {
  // Create the duplicate URL table. We will copy the kept URLs into this.
  if (!db_->CreateTemporaryURLTable())
    return false;

  // Insert the URLs into the temporary table.
  for (const auto& url : kept_urls)
    db_->AddTemporaryURL(url);

  // Replace the original URL table with the temporary one.
  if (!db_->CommitTemporaryURLTable())
    return false;

  // Delete the old tables and recreate them empty.
  db_->RecreateAllTablesButURL();

  // Vacuum to reclaim the space from the dropped tables. This must be done
  // when there is no transaction open, and we assume that our long-running
  // transaction is currently open.
  CommitSingletonTransactionIfItExists();
  db_->Vacuum();
  BeginSingletonTransaction();
  db_->GetStartDate(&first_recorded_time_);

  return true;
}

std::vector<GURL> HistoryBackend::GetCachedRecentRedirectsForPage(
    const GURL& page_url) {
  return GetCachedRecentRedirects(page_url);
}

bool HistoryBackend::ProcessSetFaviconsResult(
    const favicon::SetFaviconsResult& result,
    const GURL& icon_url) {
  if (!result.did_change_database())
    return false;

  ScheduleCommit();
  if (result.did_update_bitmap)
    SendFaviconChangedNotificationForIconURL(icon_url);
  for (const GURL& page_url : result.updated_page_urls)
    SendFaviconChangedNotificationForPageAndRedirects(page_url);
  return true;
}

void HistoryBackend::StartDeletingForeignVisits() {
  ProcessDBTask(std::make_unique<DeleteForeignVisitsDBTask>(), task_runner_,
                /*is_canceled=*/base::BindRepeating([]() { return false; }));
}

}  // namespace history
