// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The history system runs on a background sequence so that potentially slow
// database operations don't delay the browser. This backend processing is
// represented by HistoryBackend. The HistoryService's job is to dispatch to
// that sequence.
//
// Main thread                       backend_task_runner_
// -----------                       --------------
// HistoryService <----------------> HistoryBackend
//                                   -> HistoryDatabase
//                                      -> SQLite connection to History
//                                   -> FaviconDatabase
//                                      -> SQLite connection to favicons

#include "components/history/core/browser/history_service.h"

#include <functional>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_client.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/sync/delete_directive_handler.h"
#include "components/history/core/browser/visit_database.h"
#include "components/history/core/browser/visit_delegate.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(IS_IOS)
#include "base/critical_closure.h"
#endif

using base::Time;

namespace history {

// Sends messages from the backend to us on the main thread. This must be a
// separate class from the history service so that it can hold a reference to
// the history service (otherwise we would have to manually AddRef and
// Release when the Backend has a reference to us).
class HistoryService::BackendDelegate : public HistoryBackend::Delegate {
 public:
  BackendDelegate(
      const base::WeakPtr<HistoryService>& history_service,
      const scoped_refptr<base::SequencedTaskRunner>& service_task_runner,
      const CanAddURLCallback& can_add_url)
      : history_service_(history_service),
        service_task_runner_(service_task_runner),
        can_add_url_(can_add_url) {}

  bool CanAddURL(const GURL& url) const override {
    return can_add_url_ ? can_add_url_.Run(url) : true;
  }

  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override {
    // Send to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryService::NotifyProfileError,
                                  history_service_, init_status, diagnostics));
  }

  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override {
    // Send the backend to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryService::SetInMemoryBackend,
                                  history_service_, std::move(backend)));
  }

  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {
    // Send the notification to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryService::NotifyFaviconsChanged,
                                  history_service_, page_urls, icon_url));
  }

  void NotifyURLVisited(const URLRow& url_row,
                        const VisitRow& visit_row) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryService::NotifyURLVisited,
                                  history_service_, url_row, visit_row));
  }

  void NotifyURLsModified(const URLRows& changed_urls) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryService::NotifyURLsModified,
                                  history_service_, changed_urls));
  }

  void NotifyURLsDeleted(DeletionInfo deletion_info) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryService::NotifyURLsDeleted,
                                  history_service_, std::move(deletion_info)));
  }

  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const std::u16string& term) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HistoryService::NotifyKeywordSearchTermUpdated,
                       history_service_, row, keyword_id, term));
  }

  void NotifyKeywordSearchTermDeleted(URLID url_id) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HistoryService::NotifyKeywordSearchTermDeleted,
                       history_service_, url_id));
  }

  void DBLoaded() override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HistoryService::OnDBLoaded, history_service_));
  }

 private:
  const base::WeakPtr<HistoryService> history_service_;
  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
  CanAddURLCallback can_add_url_;
};

HistoryService::HistoryService() : HistoryService(nullptr, nullptr) {}

HistoryService::HistoryService(std::unique_ptr<HistoryClient> history_client,
                               std::unique_ptr<VisitDelegate> visit_delegate)
    : history_client_(std::move(history_client)),
      visit_delegate_(std::move(visit_delegate)),
      backend_loaded_(false) {}

HistoryService::~HistoryService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Shutdown the backend. This does nothing if Cleanup was already invoked.
  Cleanup();
}

bool HistoryService::BackendLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return backend_loaded_;
}

#if BUILDFLAG(IS_IOS)
void HistoryService::HandleBackgrounding() {
  TRACE_EVENT0("browser", "HistoryService::HandleBackgrounding");

  if (!backend_task_runner_ || !history_backend_.get())
    return;

  ScheduleTask(
      PRIORITY_NORMAL,
      base::MakeCriticalClosure(
          "HistoryService::HandleBackgrounding",
          base::BindOnce(&HistoryBackend::PersistState, history_backend_.get()),
          /*is_immediate=*/true));
}
#endif

void HistoryService::ClearCachedDataForContextID(ContextID context_id) {
  TRACE_EVENT0("browser", "HistoryService::ClearCachedDataForContextID");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::ClearCachedDataForContextID,
                              history_backend_, context_id));
}

void HistoryService::ClearAllOnDemandFavicons() {
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::ClearAllOnDemandFavicons,
                              history_backend_));
}

URLDatabase* HistoryService::InMemoryDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return in_memory_backend_ ? in_memory_backend_->db() : nullptr;
}

void HistoryService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Cleanup();
}

void HistoryService::SetKeywordSearchTermsForURL(const GURL& url,
                                                 KeywordID keyword_id,
                                                 const std::u16string& term) {
  TRACE_EVENT0("browser", "HistoryService::SetKeywordSearchTermsForURL");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_UI,
               base::BindOnce(&HistoryBackend::SetKeywordSearchTermsForURL,
                              history_backend_, url, keyword_id, term));
}

void HistoryService::DeleteAllSearchTermsForKeyword(KeywordID keyword_id) {
  TRACE_EVENT0("browser", "HistoryService::DeleteAllSearchTermsForKeyword");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (in_memory_backend_)
    in_memory_backend_->DeleteAllSearchTermsForKeyword(keyword_id);

  ScheduleTask(PRIORITY_UI,
               base::BindOnce(&HistoryBackend::DeleteAllSearchTermsForKeyword,
                              history_backend_, keyword_id));
}

void HistoryService::DeleteKeywordSearchTermForURL(const GURL& url) {
  TRACE_EVENT0("browser", "HistoryService::DeleteKeywordSearchTermForURL");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_UI,
               base::BindOnce(&HistoryBackend::DeleteKeywordSearchTermForURL,
                              history_backend_, url));
}

void HistoryService::DeleteMatchingURLsForKeyword(KeywordID keyword_id,
                                                  const std::u16string& term) {
  TRACE_EVENT0("browser", "HistoryService::DeleteMatchingURLsForKeyword");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_UI,
               base::BindOnce(&HistoryBackend::DeleteMatchingURLsForKeyword,
                              history_backend_, keyword_id, term));
}

void HistoryService::URLsNoLongerBookmarked(const std::set<GURL>& urls) {
  TRACE_EVENT0("browser", "HistoryService::URLsNoLongerBookmarked");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::URLsNoLongerBookmarked,
                              history_backend_, urls));
}

void HistoryService::SetOnCloseContextAnnotationsForVisit(
    VisitID visit_id,
    const VisitContextAnnotations& visit_context_annotations) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(
      PRIORITY_NORMAL,
      base::BindOnce(&HistoryBackend::SetOnCloseContextAnnotationsForVisit,
                     history_backend_, visit_id, visit_context_annotations));
}

base::CancelableTaskTracker::TaskId HistoryService::GetAnnotatedVisits(
    const QueryOptions& options,
    GetAnnotatedVisitsCallback callback,
    base::CancelableTaskTracker* tracker) const {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetAnnotatedVisits, history_backend_,
                     options, nullptr),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::ReplaceClusters(
    const std::vector<int64_t>& ids_to_delete,
    const std::vector<Cluster>& clusters_to_add,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::ReplaceClusters, history_backend_,
                     ids_to_delete, clusters_to_add),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::ReserveNextClusterId(
    ClusterIdCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::ReserveNextClusterId, history_backend_),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::AddVisitsToCluster(
    int64_t cluster_id,
    const std::vector<ClusterVisit>& visits,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::AddVisitsToCluster, history_backend_,
                     cluster_id, visits),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::UpdateClusterTriggerability(
    const std::vector<history::Cluster>& clusters,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::UpdateClusterTriggerability,
                     history_backend_, clusters),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::HideVisits(
    const std::vector<VisitID>& visit_ids,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::HideVisits, history_backend_, visit_ids),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::UpdateClusterVisit(
    const history::ClusterVisit& cluster_visit,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::UpdateClusterVisit, history_backend_,
                     cluster_visit),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::GetMostRecentClusters(
    base::Time inclusive_min_time,
    base::Time exclusive_max_time,
    size_t max_clusters,
    size_t max_visits_soft_cap,
    base::OnceCallback<void(std::vector<Cluster>)> callback,
    bool include_keywords_and_duplicates,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetMostRecentClusters, history_backend_,
                     inclusive_min_time, exclusive_max_time, max_clusters,
                     max_visits_soft_cap, include_keywords_and_duplicates),
      std::move(callback));
}

void HistoryService::AddObserver(HistoryServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void HistoryService::RemoveObserver(HistoryServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

base::CancelableTaskTracker::TaskId HistoryService::ScheduleDBTask(
    const base::Location& from_here,
    std::unique_ptr<HistoryDBTask> task,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "HistoryService::ScheduleDBTask");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  base::CancelableTaskTracker::TaskId task_id =
      tracker->NewTrackedTaskId(&is_canceled);
  // Use base::SingleThreadTaskRunner::GetCurrentDefault() to get a task runner
  // for the current message loop so that we can forward the call to the method
  // HistoryDBTask::DoneRunOnMainThread() in the correct thread.
  backend_task_runner_->PostTask(
      from_here,
      base::BindOnce(
          &HistoryBackend::ProcessDBTask, history_backend_, std::move(task),
          base::SingleThreadTaskRunner::GetCurrentDefault(), is_canceled));
  return task_id;
}

void HistoryService::FlushForTest(base::OnceClosure flushed) {
  backend_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         std::move(flushed));
}

void HistoryService::SetOnBackendDestroyTask(base::OnceClosure task) {
  TRACE_EVENT0("browser", "HistoryService::SetOnBackendDestroyTask");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(
      PRIORITY_NORMAL,
      base::BindOnce(&HistoryBackend::SetOnBackendDestroyTask, history_backend_,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     std::move(task)));
}

void HistoryService::GetCountsAndLastVisitForOriginsForTesting(
    const std::set<GURL>& origins,
    GetCountsAndLastVisitForOriginsCallback callback) const {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&HistoryBackend::GetCountsAndLastVisitForOrigins,
                     history_backend_, origins),
      std::move(callback));
}

void HistoryService::AddPage(const GURL& url,
                             Time time,
                             ContextID context_id,
                             int nav_entry_id,
                             const GURL& referrer,
                             const RedirectList& redirects,
                             ui::PageTransition transition,
                             VisitSource visit_source,
                             bool did_replace_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddPage(HistoryAddPageArgs(
      url, time, context_id, nav_entry_id, referrer, redirects, transition,
      !ui::PageTransitionIsMainFrame(transition), visit_source,
      did_replace_entry, /*consider_for_ntp_most_visited=*/true));
}

void HistoryService::AddPage(const GURL& url,
                             base::Time time,
                             VisitSource visit_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddPage(HistoryAddPageArgs(
      url, time, /*context_id=*/0, /*nav_entry_id=*/0,
      /*referrer=*/GURL(), RedirectList(), ui::PAGE_TRANSITION_LINK,
      /*hidden=*/false, visit_source,
      /*did_replace_entry=*/false, /*consider_for_ntp_most_visited=*/true));
}

void HistoryService::AddPage(const HistoryAddPageArgs& add_page_args) {
  TRACE_EVENT0("browser", "HistoryService::AddPage");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CanAddURL(add_page_args.url))
    return;

  // Inform VisitedDelegate of all links and redirects.
  if (visit_delegate_) {
    if (!add_page_args.redirects.empty()) {
      // We should not be asked to add a page in the middle of a redirect chain,
      // and thus add_page_args.url should be the last element in the array
      // add_page_args.redirects which mean we can use VisitDelegate::AddURLs()
      // with the whole array.
      DCHECK_EQ(add_page_args.url, add_page_args.redirects.back());
      visit_delegate_->AddURLs(add_page_args.redirects);
    } else {
      visit_delegate_->AddURL(add_page_args.url);
    }
  }

  // In extremely rare cases an in-flight clear history task posted to the UI
  // thread could cause this last used time to be dropped.
  if (add_page_args.bookmark_id.has_value()) {
    history_client_->UpdateBookmarkLastUsedTime(
        add_page_args.bookmark_id.value(), add_page_args.time);
  }

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::AddPage, history_backend_,
                              add_page_args));
}

void HistoryService::AddPageNoVisitForBookmark(const GURL& url,
                                               const std::u16string& title) {
  TRACE_EVENT0("browser", "HistoryService::AddPageNoVisitForBookmark");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanAddURL(url))
    return;

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::AddPageNoVisitForBookmark,
                              history_backend_, url, title));
}

void HistoryService::SetPageTitle(const GURL& url,
                                  const std::u16string& title) {
  TRACE_EVENT0("browser", "HistoryService::SetPageTitle");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL, base::BindOnce(&HistoryBackend::SetPageTitle,
                                               history_backend_, url, title));
}

void HistoryService::UpdateWithPageEndTime(ContextID context_id,
                                           int nav_entry_id,
                                           const GURL& url,
                                           Time end_ts) {
  TRACE_EVENT0("browser", "HistoryService::UpdateWithPageEndTime");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(
      PRIORITY_NORMAL,
      base::BindOnce(&HistoryBackend::UpdateWithPageEndTime, history_backend_,
                     context_id, nav_entry_id, url, end_ts));
}

void HistoryService::SetBrowsingTopicsAllowed(ContextID context_id,
                                              int nav_entry_id,
                                              const GURL& url) {
  TRACE_EVENT0("browser", "HistoryService::SetBrowsingTopicsAllowed");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::SetBrowsingTopicsAllowed,
                              history_backend_, context_id, nav_entry_id, url));
}

void HistoryService::SetPageLanguageForVisit(ContextID context_id,
                                             int nav_entry_id,
                                             const GURL& url,
                                             const std::string& page_language) {
  TRACE_EVENT0("browser", "HistoryService::SetPageLanguageForVisit");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(
      PRIORITY_NORMAL,
      base::BindOnce(&HistoryBackend::SetPageLanguageForVisit, history_backend_,
                     context_id, nav_entry_id, url, page_language));
}

void HistoryService::SetPasswordStateForVisit(
    ContextID context_id,
    int nav_entry_id,
    const GURL& url,
    VisitContentAnnotations::PasswordState password_state) {
  TRACE_EVENT0("browser", "HistoryService::SetPasswordStateForVisit");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::SetPasswordStateForVisit,
                              history_backend_, context_id, nav_entry_id, url,
                              password_state));
}

void HistoryService::AddContentModelAnnotationsForVisit(
    const VisitContentModelAnnotations& model_annotations,
    VisitID visit_id) {
  TRACE_EVENT0("browser", "HistoryService::AddContentModelAnnotationsForVisit");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(
      PRIORITY_NORMAL,
      base::BindOnce(&HistoryBackend::AddContentModelAnnotationsForVisit,
                     history_backend_, visit_id, model_annotations));
}

void HistoryService::AddRelatedSearchesForVisit(
    const std::vector<std::string>& related_searches,
    VisitID visit_id) {
  TRACE_EVENT0("browser", "HistoryService::AddRelatedSearchesForVisit");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::AddRelatedSearchesForVisit,
                              history_backend_, visit_id, related_searches));
}

void HistoryService::AddSearchMetadataForVisit(
    const GURL& search_normalized_url,
    const std::u16string& search_terms,
    VisitID visit_id) {
  TRACE_EVENT0("browser", "HistoryService::AddSearchMetadataForVisit");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::AddSearchMetadataForVisit,
                              history_backend_, visit_id, search_normalized_url,
                              search_terms));
}

void HistoryService::AddPageMetadataForVisit(
    const std::string& alternative_title,
    VisitID visit_id) {
  TRACE_EVENT0("browser", "HistoryService::AddPageMetadataForVisit");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::AddPageMetadataForVisit,
                              history_backend_, visit_id, alternative_title));
}

void HistoryService::AddPageWithDetails(const GURL& url,
                                        const std::u16string& title,
                                        int visit_count,
                                        int typed_count,
                                        Time last_visit,
                                        bool hidden,
                                        VisitSource visit_source) {
  TRACE_EVENT0("browser", "HistoryService::AddPageWithDetails");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Filter out unwanted URLs.
  if (!CanAddURL(url))
    return;

  // Inform VisitDelegate of the URL.
  if (visit_delegate_)
    visit_delegate_->AddURL(url);

  URLRow row(url);
  row.set_title(title);
  row.set_visit_count(visit_count);
  row.set_typed_count(typed_count);
  row.set_last_visit(last_visit);
  row.set_hidden(hidden);

  URLRows rows;
  rows.push_back(row);

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::AddPagesWithDetails,
                              history_backend_, rows, visit_source));
}

void HistoryService::AddPagesWithDetails(const URLRows& info,
                                         VisitSource visit_source) {
  TRACE_EVENT0("browser", "HistoryService::AddPagesWithDetails");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Inform the VisitDelegate of the URLs
  if (!info.empty() && visit_delegate_) {
    std::vector<GURL> urls;
    urls.reserve(info.size());
    for (const auto& row : info)
      urls.push_back(row.url());
    visit_delegate_->AddURLs(urls);
  }

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::AddPagesWithDetails,
                              history_backend_, info, visit_source));
}

base::CancelableTaskTracker::TaskId HistoryService::GetFavicon(
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const std::vector<int>& desired_sizes,
    favicon_base::FaviconResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "HistoryService::GetFavicons");
  CHECK(backend_task_runner_) << "History service being called after cleanup";
  // TODO(https://crbug.com/1024959): convert to DCHECK once crash is resolved.
  CHECK(tracker);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetFavicon, history_backend_, icon_url,
                     icon_type, desired_sizes),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::GetFaviconsForURL(
    const GURL& page_url,
    const favicon_base::IconTypeSet& icon_types,
    const std::vector<int>& desired_sizes,
    bool fallback_to_host,
    favicon_base::FaviconResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "HistoryService::GetFaviconsForURL");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetFaviconsForURL, history_backend_,
                     page_url, icon_types, desired_sizes, fallback_to_host),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::GetLargestFaviconForURL(
    const GURL& page_url,
    const std::vector<favicon_base::IconTypeSet>& icon_types,
    int minimum_size_in_pixels,
    favicon_base::FaviconRawBitmapCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetLargestFaviconForURL, history_backend_,
                     page_url, icon_types, minimum_size_in_pixels),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::GetFaviconForID(
    favicon_base::FaviconID favicon_id,
    int desired_size,
    favicon_base::FaviconResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "HistoryService::GetFaviconForID");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetFaviconForID, history_backend_,
                     favicon_id, desired_size),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId
HistoryService::UpdateFaviconMappingsAndFetch(
    const base::flat_set<GURL>& page_urls,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const std::vector<int>& desired_sizes,
    favicon_base::FaviconResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "HistoryService::UpdateFaviconMappingsAndFetch");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::UpdateFaviconMappingsAndFetch,
                     history_backend_, page_urls, icon_url, icon_type,
                     desired_sizes),
      std::move(callback));
}

void HistoryService::DeleteFaviconMappings(
    const base::flat_set<GURL>& page_urls,
    favicon_base::IconType icon_type) {
  TRACE_EVENT0("browser", "HistoryService::DeleteFaviconMappings");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::DeleteFaviconMappings,
                              history_backend_, page_urls, icon_type));
}

void HistoryService::MergeFavicon(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    scoped_refptr<base::RefCountedMemory> bitmap_data,
    const gfx::Size& pixel_size) {
  TRACE_EVENT0("browser", "HistoryService::MergeFavicon");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanAddURL(page_url))
    return;

  ScheduleTask(
      PRIORITY_NORMAL,
      base::BindOnce(&HistoryBackend::MergeFavicon, history_backend_, page_url,
                     icon_url, icon_type, bitmap_data, pixel_size));
}

void HistoryService::SetFavicons(const base::flat_set<GURL>& page_urls,
                                 favicon_base::IconType icon_type,
                                 const GURL& icon_url,
                                 const std::vector<SkBitmap>& bitmaps) {
  TRACE_EVENT0("browser", "HistoryService::SetFavicons");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::flat_set<GURL> page_urls_to_save;
  page_urls_to_save.reserve(page_urls.capacity());
  for (const GURL& page_url : page_urls) {
    if (CanAddURL(page_url))
      page_urls_to_save.insert(page_url);
  }

  if (page_urls_to_save.empty())
    return;

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::SetFavicons, history_backend_,
                              page_urls_to_save, icon_type, icon_url, bitmaps));
}

void HistoryService::CloneFaviconMappingsForPages(
    const GURL& page_url_to_read,
    const favicon_base::IconTypeSet& icon_types,
    const base::flat_set<GURL>& page_urls_to_write) {
  TRACE_EVENT0("browser", "HistoryService::CloneFaviconMappingsForPages");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::CloneFaviconMappingsForPages,
                              history_backend_, page_url_to_read, icon_types,
                              page_urls_to_write));
}

void HistoryService::CanSetOnDemandFavicons(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanAddURL(page_url)) {
    std::move(callback).Run(false);
    return;
  }

  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&HistoryBackend::CanSetOnDemandFavicons, history_backend_,
                     page_url, icon_type),
      std::move(callback));
}

void HistoryService::SetOnDemandFavicons(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    const GURL& icon_url,
    const std::vector<SkBitmap>& bitmaps,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanAddURL(page_url)) {
    std::move(callback).Run(false);
    return;
  }

  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&HistoryBackend::SetOnDemandFavicons, history_backend_,
                     page_url, icon_type, icon_url, bitmaps),
      std::move(callback));
}

void HistoryService::SetFaviconsOutOfDateForPage(const GURL& page_url) {
  TRACE_EVENT0("browser", "HistoryService::SetFaviconsOutOfDateForPage");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::SetFaviconsOutOfDateForPage,
                              history_backend_, page_url));
}

void HistoryService::SetFaviconsOutOfDateBetween(
    base::Time begin,
    base::Time end,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "HistoryService::SetFaviconsOutOfDateBetween");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::SetFaviconsOutOfDateBetween,
                     history_backend_, begin, end),
      std::move(callback));
}

void HistoryService::TouchOnDemandFavicon(const GURL& icon_url) {
  TRACE_EVENT0("browser", "HistoryService::TouchOnDemandFavicon");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::TouchOnDemandFavicon,
                              history_backend_, icon_url));
}

void HistoryService::SetImportedFavicons(
    const favicon_base::FaviconUsageDataList& favicon_usage) {
  TRACE_EVENT0("browser", "HistoryService::SetImportedFavicons");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::SetImportedFavicons,
                              history_backend_, favicon_usage));
}

// Querying --------------------------------------------------------------------

base::CancelableTaskTracker::TaskId HistoryService::QueryURL(
    const GURL& url,
    bool want_visits,
    QueryURLCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryURL, history_backend_, url,
                     want_visits),
      std::move(callback));
}

// Statistics ------------------------------------------------------------------

base::CancelableTaskTracker::TaskId HistoryService::GetHistoryCount(
    const Time& begin_time,
    const Time& end_time,
    GetHistoryCountCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetHistoryCount, history_backend_,
                     begin_time, end_time),
      std::move(callback));
}

void HistoryService::CountUniqueHostsVisitedLastMonth(
    GetHistoryCountCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::CountUniqueHostsVisitedLastMonth,
                     history_backend_),
      std::move(callback));
}

void HistoryService::GetDomainDiversity(
    base::Time report_time,
    int number_of_days_to_report,
    DomainMetricBitmaskType metric_type_bitmask,
    DomainDiversityCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetDomainDiversity, history_backend_,
                     report_time, number_of_days_to_report,
                     metric_type_bitmask),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::GetLastVisitToHost(
    const std::string& host,
    base::Time begin_time,
    base::Time end_time,
    GetLastVisitCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetLastVisitToHost, history_backend_,
                     host, begin_time, end_time),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::GetLastVisitToOrigin(
    const url::Origin& origin,
    base::Time begin_time,
    base::Time end_time,
    GetLastVisitCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetLastVisitToOrigin, history_backend_,
                     origin, begin_time, end_time),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::GetLastVisitToURL(
    const GURL& url,
    base::Time end_time,
    GetLastVisitCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetLastVisitToURL, history_backend_, url,
                     end_time),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::GetDailyVisitsToHost(
    const GURL& host,
    base::Time begin_time,
    base::Time end_time,
    GetDailyVisitsToHostCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetDailyVisitsToHost, history_backend_,
                     host, begin_time, end_time),
      std::move(callback));
}

// Downloads -------------------------------------------------------------------

// Handle creation of a download by creating an entry in the history service's
// 'downloads' table.
void HistoryService::CreateDownload(const DownloadRow& create_info,
                                    DownloadCreateCallback callback) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&HistoryBackend::CreateDownload, history_backend_,
                     create_info),
      std::move(callback));
}

void HistoryService::GetNextDownloadId(DownloadIdCallback callback) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&HistoryBackend::GetNextDownloadId, history_backend_),
      std::move(callback));
}

// Handle queries for a list of all downloads in the history database's
// 'downloads' table.
void HistoryService::QueryDownloads(DownloadQueryCallback callback) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryDownloads, history_backend_),
      std::move(callback));
}

// Handle updates for a particular download. This is a 'fire and forget'
// operation, so we don't need to be called back.
void HistoryService::UpdateDownload(const DownloadRow& data,
                                    bool should_commit_immediately) {
  TRACE_EVENT0("browser", "HistoryService::UpdateDownload");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::UpdateDownload, history_backend_,
                              data, should_commit_immediately));
}

void HistoryService::RemoveDownloads(const std::set<uint32_t>& ids) {
  TRACE_EVENT0("browser", "HistoryService::RemoveDownloads");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_NORMAL, base::BindOnce(&HistoryBackend::RemoveDownloads,
                                               history_backend_, ids));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryHistory(
    const std::u16string& text_query,
    const QueryOptions& options,
    QueryHistoryCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryHistory, history_backend_,
                     text_query, options),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryRedirectsFrom(
    const GURL& from_url,
    QueryRedirectsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryRedirectsFrom, history_backend_,
                     from_url),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryRedirectsTo(
    const GURL& to_url,
    QueryRedirectsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryRedirectsTo, history_backend_,
                     to_url),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::GetVisibleVisitCountToHost(
    const GURL& url,
    GetVisibleVisitCountToHostCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (origin_queried_closure_for_testing_) {
    callback = base::BindOnce(
        [](base::OnceClosure origin_queried_closure,
           GetVisibleVisitCountToHostCallback wrapped_callback,
           VisibleVisitCountToHostResult result) {
          std::move(wrapped_callback).Run(std::move(result));
          std::move(origin_queried_closure).Run();
        },
        std::move(origin_queried_closure_for_testing_), std::move(callback));
  }
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetVisibleVisitCountToHost,
                     history_backend_, url),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryMostVisitedURLs(
    int result_count,
    QueryMostVisitedURLsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryMostVisitedURLs, history_backend_,
                     result_count),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId
HistoryService::QueryMostRepeatedQueriesForKeyword(
    KeywordID keyword_id,
    size_t result_count,
    base::OnceCallback<void(KeywordSearchTermVisitList)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryMostRepeatedQueriesForKeyword,
                     history_backend_, keyword_id, result_count),
      std::move(callback));
}

void HistoryService::Cleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_task_runner_) {
    // We've already cleaned up.
    return;
  }

  NotifyHistoryServiceBeingDeleted();

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Inform the HistoryClient that we are shuting down.
  if (history_client_)
    history_client_->Shutdown();

  // Unload the backend.
  if (history_backend_) {
    // Get rid of the in-memory backend.
    in_memory_backend_.reset();

    ScheduleTask(PRIORITY_NORMAL, base::BindOnce(&HistoryBackend::Closing,
                                                 std::move(history_backend_)));
  }

  // Clear `backend_task_runner_` to make sure it's not used after Cleanup().
  backend_task_runner_ = nullptr;
}

bool HistoryService::Init(
    bool no_db,
    const HistoryDatabaseParams& history_database_params) {
  TRACE_EVENT0("browser,startup", "HistoryService::Init");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Unit tests can inject `backend_task_runner_` before this is called.
  if (!backend_task_runner_) {
    backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::WithBaseSyncPrimitives(),
         base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }

  // Create the history backend.
  scoped_refptr<HistoryBackend> backend(base::MakeRefCounted<HistoryBackend>(
      std::make_unique<BackendDelegate>(
          weak_ptr_factory_.GetWeakPtr(),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          history_client_ ? history_client_->GetThreadSafeCanAddURLCallback()
                          : CanAddURLCallback()),
      history_client_ ? history_client_->CreateBackendClient() : nullptr,
      backend_task_runner_));
  history_backend_.swap(backend);

  ScheduleTask(PRIORITY_UI,
               base::BindOnce(&HistoryBackend::Init, history_backend_, no_db,
                              history_database_params));

  delete_directive_handler_ = std::make_unique<DeleteDirectiveHandler>(
      base::BindRepeating(base::IgnoreResult(&HistoryService::ScheduleDBTask),
                          base::Unretained(this)));

  if (visit_delegate_ && !visit_delegate_->Init(this)) {
    // This is rare enough that it's worth logging.
    LOG(WARNING) << "HistoryService::Init() failed by way of "
                    "VisitDelegate::Init failing";
    return false;
  }

  if (history_client_)
    history_client_->OnHistoryServiceCreated(this);

  return true;
}

void HistoryService::ScheduleAutocomplete(
    base::OnceCallback<void(HistoryBackend*, URLDatabase*)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleTask(PRIORITY_UI,
               base::BindOnce(&HistoryBackend::ScheduleAutocomplete,
                              history_backend_, std::move(callback)));
}

void HistoryService::ScheduleTask(SchedulePriority priority,
                                  base::OnceClosure task) {
  TRACE_EVENT0("browser", "HistoryService::ScheduleTask");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_task_runner_);
  // TODO(brettw): Do prioritization.
  // NOTE(mastiz): If this implementation changes, be cautious with implications
  // for sync, because a) the sync engine (sync thread) post tasks directly to
  // the task runner via ModelTypeProcessorProxy (which is subtle); and b)
  // SyncServiceImpl (UI thread) does the same via
  // ProxyModelTypeControllerDelegate.
  backend_task_runner_->PostTask(FROM_HERE, std::move(task));
}

base::WeakPtr<HistoryService> HistoryService::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<syncer::SyncableService>
HistoryService::GetDeleteDirectivesSyncableService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delete_directive_handler_);
  return delete_directive_handler_->AsWeakPtr();
}

std::unique_ptr<syncer::ModelTypeControllerDelegate>
HistoryService::GetTypedURLSyncControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Note that a callback is bound for GetTypedURLSyncControllerDelegate()
  // because this getter itself must also run in the backend sequence, and the
  // proxy object below will take care of that.
  return std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
      backend_task_runner_,
      base::BindRepeating(&HistoryBackend::GetTypedURLSyncControllerDelegate,
                          base::Unretained(history_backend_.get())));
}

std::unique_ptr<syncer::ModelTypeControllerDelegate>
HistoryService::GetHistorySyncControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Note that a callback is bound for GetHistorySyncControllerDelegate()
  // because this getter itself must also run in the backend sequence, and the
  // proxy object below will take care of that.
  return std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
      backend_task_runner_,
      base::BindRepeating(&HistoryBackend::GetHistorySyncControllerDelegate,
                          base::Unretained(history_backend_.get())));
}

void HistoryService::SetSyncTransportState(
    syncer::SyncService::TransportState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::SetSyncTransportState,
                              history_backend_, state));
}

void HistoryService::ProcessLocalDeleteDirective(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delete_directive_handler_->ProcessLocalDeleteDirective(delete_directive);
}

void HistoryService::SetInMemoryBackend(
    std::unique_ptr<InMemoryHistoryBackend> mem_backend) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!in_memory_backend_) << "Setting mem DB twice";
  in_memory_backend_ = std::move(mem_backend);

  // The database requires additional initialization once we own it.
  in_memory_backend_->AttachToHistoryService(this);
}

void HistoryService::NotifyProfileError(sql::InitStatus init_status,
                                        const std::string& diagnostics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (history_client_)
    history_client_->NotifyProfileError(init_status, diagnostics);
}

void HistoryService::DeleteURLs(const std::vector<GURL>& urls) {
  TRACE_EVENT0("browser", "HistoryService::DeleteURLs");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We will update the visited links when we observe the delete notifications.
  ScheduleTask(PRIORITY_NORMAL, base::BindOnce(&HistoryBackend::DeleteURLs,
                                               history_backend_, urls));
}

void HistoryService::ExpireHistoryBetween(
    const std::set<GURL>& restrict_urls,
    Time begin_time,
    Time end_time,
    bool user_initiated,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::ExpireHistoryBetween, history_backend_,
                     restrict_urls, begin_time, end_time, user_initiated),
      std::move(callback));
}

void HistoryService::ExpireHistory(
    const std::vector<ExpireHistoryArgs>& expire_list,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tracker->PostTaskAndReply(backend_task_runner_.get(), FROM_HERE,
                            base::BindOnce(&HistoryBackend::ExpireHistory,
                                           history_backend_, expire_list),
                            std::move(callback));
}

void HistoryService::ExpireHistoryBeforeForTesting(
    base::Time end_time,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::ExpireHistoryBeforeForTesting,
                     history_backend_, end_time),
      std::move(callback));
}

void HistoryService::DeleteLocalAndRemoteHistoryBetween(
    WebHistoryService* web_history,
    Time begin_time,
    Time end_time,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  // TODO(crbug.com/929111): This should be factored out into a separate class
  // that dispatches deletions to the proper places.
  if (web_history) {
    delete_directive_handler_->CreateTimeRangeDeleteDirective(begin_time,
                                                              end_time);

    // Attempt online deletion from the history server, but ignore the result.
    // Deletion directives ensure that the results will eventually be deleted.
    //
    // TODO(davidben): `callback` should not run until this operation completes
    // too.
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
        net::DefinePartialNetworkTrafficAnnotation(
            "web_history_expire_between_dates", "web_history_service", R"(
          semantics {
            description:
              "If a user who syncs their browsing history deletes history "
              "items for a time range, Chrome sends a request to a google.com "
              "host to execute the corresponding deletion serverside."
            trigger:
              "Deleting browsing history for a given time range, e.g. from the "
              "Clear Browsing Data dialog, by an extension, or the "
              "Clear-Site-Data header."
            data:
              "The begin and end timestamps of the selected time range, a "
              "version info token to resolve transaction conflicts, and an "
              "OAuth2 token authenticating the user."
          }
          policy {
            chrome_policy {
              AllowDeletingBrowserHistory {
                AllowDeletingBrowserHistory: false
              }
            }
          })");
    web_history->ExpireHistoryBetween(
        /*restrict_urls=*/{}, begin_time, end_time, base::DoNothing(),
        partial_traffic_annotation);
  }
  ExpireHistoryBetween(/*restrict_urls=*/{}, begin_time, end_time,
                       /*user_initiated=*/true, std::move(callback), tracker);
}

void HistoryService::DeleteLocalAndRemoteUrl(WebHistoryService* web_history,
                                             const GURL& url) {
  DCHECK(url.is_valid());
  // TODO(crbug.com/929111): This should be factored out into a separate class
  // that dispatches deletions to the proper places.
  if (web_history) {
    delete_directive_handler_->CreateUrlDeleteDirective(url);

    // Attempt online deletion from the history server, but ignore the result.
    // Deletion directives ensure that the results will eventually be deleted.
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
        net::DefinePartialNetworkTrafficAnnotation("web_history_delete_url",
                                                   "web_history_service", R"(
          semantics {
            description:
              "If a user who syncs their browsing history deletes urls from  "
              "history, Chrome sends a request to a google.com "
              "host to execute the corresponding deletion serverside."
            trigger:
              "Deleting urls from browsing history, e.g. by an extension."
            data:
              "The selected urls, a version info token to resolve transaction "
              "conflicts, and an OAuth2 token authenticating the user."
          }
          policy {
            chrome_policy {
              AllowDeletingBrowserHistory {
                AllowDeletingBrowserHistory: false
              }
            }
          })");
    web_history->ExpireHistoryBetween(
        /*restrict_urls=*/{url}, base::Time(), base::Time::Max(),
        base::DoNothing(), partial_traffic_annotation);
  }
  DeleteURLs({url});
}

void HistoryService::OnDBLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_loaded_ = true;
  delete_directive_handler_->OnBackendLoaded();
  NotifyHistoryServiceLoaded();
}

void HistoryService::NotifyURLVisited(const URLRow& url_row,
                                      const VisitRow& new_visit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (HistoryServiceObserver& observer : observers_)
    observer.OnURLVisited(this, url_row, new_visit);
}

void HistoryService::NotifyURLsModified(const URLRows& changed_urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (HistoryServiceObserver& observer : observers_)
    observer.OnURLsModified(this, changed_urls);
}

void HistoryService::NotifyURLsDeleted(const DeletionInfo& deletion_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_task_runner_)
    return;

  // Inform the VisitDelegate of the deleted URLs. We will inform the delegate
  // of added URLs as soon as we get the add notification (we don't have to wait
  // for the backend, which allows us to be faster to update the state).
  //
  // For deleted URLs, we don't typically know what will be deleted since
  // delete notifications are by time. We would also like to be more
  // respectful of privacy and never tell the user something is gone when it
  // isn't. Therefore, we update the delete URLs after the fact.
  if (visit_delegate_) {
    if (deletion_info.IsAllHistory()) {
      visit_delegate_->DeleteAllURLs();
    } else {
      std::vector<GURL> urls;
      urls.reserve(deletion_info.deleted_rows().size());
      for (const auto& row : deletion_info.deleted_rows())
        urls.push_back(row.url());
      visit_delegate_->DeleteURLs(urls);
    }
  }

  for (HistoryServiceObserver& observer : observers_)
    observer.OnURLsDeleted(this, deletion_info);
}

void HistoryService::NotifyHistoryServiceLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (HistoryServiceObserver& observer : observers_)
    observer.OnHistoryServiceLoaded(this);
}

void HistoryService::NotifyHistoryServiceBeingDeleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (HistoryServiceObserver& observer : observers_)
    observer.HistoryServiceBeingDeleted(this);
}

void HistoryService::NotifyKeywordSearchTermUpdated(
    const URLRow& row,
    KeywordID keyword_id,
    const std::u16string& term) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (HistoryServiceObserver& observer : observers_)
    observer.OnKeywordSearchTermUpdated(this, row, keyword_id, term);
}

void HistoryService::NotifyKeywordSearchTermDeleted(URLID url_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (HistoryServiceObserver& observer : observers_)
    observer.OnKeywordSearchTermDeleted(this, url_id);
}

base::CallbackListSubscription HistoryService::AddFaviconsChangedCallback(
    const HistoryService::FaviconsChangedCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return favicons_changed_callback_list_.Add(callback);
}

void HistoryService::NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                                           const GURL& icon_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  favicons_changed_callback_list_.Notify(page_urls, icon_url);
}

bool HistoryService::CanAddURL(const GURL& url) {
  if (!history_client_) {
    return true;
  }
  return history_client_->GetThreadSafeCanAddURLCallback().Run(url);
}

}  // namespace history
