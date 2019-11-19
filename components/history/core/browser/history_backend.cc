// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_backend.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/files/file_enumerator.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_backend_observer.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/browser/sync/typed_url_sync_bridge.h"
#include "components/history/core/browser/url_utils.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "sql/error_delegate_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if defined(OS_IOS)
#include "base/ios/scoped_critical_action.h"
#endif

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using syncer::ClientTagBasedModelTypeProcessor;

/* The HistoryBackend consists of two components:

    HistoryDatabase (stores past 3 months of history)
      URLDatabase (stores a list of URLs)
      DownloadDatabase (stores a list of downloads)
      VisitDatabase (stores a list of visits for the URLs)
      VisitSegmentDatabase (stores groups of URLs for the most visited view).

    ExpireHistoryBackend (manages deleting things older than 3 months)
*/

namespace history {

namespace {

void RunUnlessCanceled(
    const base::Closure& closure,
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled) {
  if (!is_canceled.Run())
    closure.Run();
}

// How long we'll wait to do a commit, so that things are batched together.
const int kCommitIntervalSeconds = 10;

// The amount of time before we re-fetch the favicon.
const int kFaviconRefetchDays = 7;

// The maximum number of items we'll allow in the redirect list before
// deleting some.
const int kMaxRedirectCount = 32;

// The number of days old a history entry can be before it is considered "old"
// and is deleted.
const int kExpireDaysThreshold = 90;

bool IsFaviconBitmapExpired(base::Time last_updated) {
  return (Time::Now() - last_updated) >
         TimeDelta::FromDays(kFaviconRefetchDays);
}

bool AreIconTypesEquivalent(favicon_base::IconType type_a,
                            favicon_base::IconType type_b) {
  if (type_a == type_b)
    return true;

  // Two icon types are considered 'equivalent' if both types are one of
  // kTouchIcon, kTouchPrecomposedIcon or kWebManifestIcon.
  const favicon_base::IconTypeSet equivalent_types = {
      favicon_base::IconType::kTouchIcon,
      favicon_base::IconType::kTouchPrecomposedIcon,
      favicon_base::IconType::kWebManifestIcon};

  if (equivalent_types.count(type_a) != 0 &&
      equivalent_types.count(type_b) != 0) {
    return true;
  }
  return false;
}

}  // namespace

base::string16 FormatUrlForRedirectComparison(const GURL& url) {
  url::Replacements<char> remove_port;
  remove_port.ClearPort();
  return url_formatter::FormatUrl(
      url.ReplaceComponents(remove_port),
      url_formatter::kFormatUrlOmitHTTP | url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitUsernamePassword |
          url_formatter::kFormatUrlOmitTrivialSubdomains,
      net::UnescapeRule::NONE, nullptr, nullptr, nullptr);
}

QueuedHistoryDBTask::QueuedHistoryDBTask(
    std::unique_ptr<HistoryDBTask> task,
    scoped_refptr<base::SingleThreadTaskRunner> origin_loop,
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled)
    : task_(std::move(task)),
      origin_loop_(origin_loop),
      is_canceled_(is_canceled) {
  DCHECK(task_);
  DCHECK(origin_loop_);
  DCHECK(!is_canceled_.is_null());
}

QueuedHistoryDBTask::~QueuedHistoryDBTask() {
  // Ensure that |task_| is destroyed on its origin thread.
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
      FROM_HERE, base::BindOnce(&RunUnlessCanceled,
                                base::Bind(&HistoryDBTask::DoneRunOnMainThread,
                                           base::Unretained(task_.get())),
                                is_canceled_));
}

// HistoryBackendHelper --------------------------------------------------------

// Wrapper around base::SupportsUserData with a public destructor.
class HistoryBackendHelper : public base::SupportsUserData {
 public:
  HistoryBackendHelper();
  ~HistoryBackendHelper() override;
};

HistoryBackendHelper::HistoryBackendHelper() = default;

HistoryBackendHelper::~HistoryBackendHelper() = default;

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
      scheduled_kill_db_(false),
      expirer_(this, backend_client.get(), task_runner),
      recent_redirects_(kMaxRedirectCount),
      segment_queried_(false),
      backend_client_(std::move(backend_client)),
      task_runner_(task_runner) {
  DCHECK(delegate_);
}

HistoryBackend::~HistoryBackend() {
  DCHECK(scheduled_commit_.IsCancelled()) << "Deleting without cleanup";
  queued_history_db_tasks_.clear();

  // Release stashed embedder object before cleaning up the databases.
  supports_user_data_helper_.reset();

  // First close the databases before optionally running the "destroy" task.
  CloseAllDatabases();

  if (!backend_destroy_task_.is_null()) {
    // Notify an interested party (typically a unit test) that we're done.
    DCHECK(backend_destroy_task_runner_);
    backend_destroy_task_runner_->PostTask(FROM_HERE, backend_destroy_task_);
  }

#if defined(OS_ANDROID)
  if (backend_client_ && !history_dir_.empty())
    backend_client_->OnHistoryBackendDestroyed(this, history_dir_);
#endif
}

void HistoryBackend::Init(
    bool force_fail,
    const HistoryDatabaseParams& history_database_params) {
  TRACE_EVENT0("browser", "HistoryBackend::Init");
  // HistoryBackend is created on the UI thread by HistoryService, then the
  // HistoryBackend::Init() method is called on the DB thread. Create the
  // base::SupportsUserData on the DB thread since it is not thread-safe.
  supports_user_data_helper_.reset(new HistoryBackendHelper);

  if (!force_fail)
    InitImpl(history_database_params);
  delegate_->DBLoaded();

  typed_url_sync_bridge_ = std::make_unique<TypedURLSyncBridge>(
      this, db_.get(),
      std::make_unique<ClientTagBasedModelTypeProcessor>(
          syncer::TYPED_URLS, /*dump_stack=*/base::RepeatingClosure()));
  typed_url_sync_bridge_->Init();

  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::Bind(&HistoryBackend::OnMemoryPressure, base::Unretained(this))));
}

void HistoryBackend::SetOnBackendDestroyTask(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Closure& task) {
  TRACE_EVENT0("browser", "HistoryBackend::SetOnBackendDestroyTask");
  if (!backend_destroy_task_.is_null())
    DLOG(WARNING) << "Setting more than one destroy task, overriding";
  backend_destroy_task_runner_ = std::move(task_runner);
  backend_destroy_task_ = task;
}

void HistoryBackend::Closing() {
  TRACE_EVENT0("browser", "HistoryBackend::Closing");
  // Any scheduled commit will have a reference to us, we must make it
  // release that reference before we can be destroyed.
  CancelScheduledCommit();
}

#if defined(OS_IOS)
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
    if (!db_->GetRowForVisit(visit_id, &row))
      return 0;
    if (row.segment_id)
      return row.segment_id;  // Found a visit in this change with a segment.

    // Check the referrer of this visit, if any.
    visit_id = row.referring_visit;

    if (visit_set.find(visit_id) != visit_set.end()) {
      NOTREACHED() << "Loop in referer chain, giving up";
      break;
    }
    visit_set.insert(visit_id);
  }
  return 0;
}

SegmentID HistoryBackend::UpdateSegments(const GURL& url,
                                         VisitID from_visit,
                                         VisitID visit_id,
                                         ui::PageTransition transition_type,
                                         const Time ts) {
  if (!db_)
    return 0;

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
        NOTREACHED();
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
    if (!segment_id)
      return 0;
  }

  // Set the segment in the visit.
  if (!db_->SetSegmentID(visit_id, segment_id)) {
    NOTREACHED();
    return 0;
  }

  // Finally, increase the counter for that segment / day.
  if (!db_->IncreaseSegmentVisitCount(segment_id, ts, 1)) {
    NOTREACHED();
    return 0;
  }
  return segment_id;
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

void HistoryBackend::UpdateVisitDuration(VisitID visit_id, const Time end_ts) {
  if (!db_)
    return;

  // Get the starting visit_time for visit_id.
  VisitRow visit_row;
  if (db_->GetRowForVisit(visit_id, &visit_row)) {
    // We should never have a negative duration time even when time is skewed.
    visit_row.visit_duration = end_ts > visit_row.visit_time
                                   ? end_ts - visit_row.visit_time
                                   : TimeDelta::FromMicroseconds(0);
    db_->UpdateVisitRow(visit_row);
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
    GURL origin = row.url().GetOrigin();
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

  if (!db_)
    return;

  // Will be filled with the URL ID and the visit ID of the last addition.
  std::pair<URLID, VisitID> last_ids(
      0, tracker_.GetLastVisit(request.context_id, request.nav_entry_id,
                               request.referrer));

  VisitID from_visit_id = last_ids.second;

  // If a redirect chain is given, we expect the last item in that chain to be
  // the final URL.
  DCHECK(request.redirects.empty() || request.redirects.back() == request.url);

  // If the user is adding older history, we need to make sure our times
  // are correct.
  if (request.time < first_recorded_time_)
    first_recorded_time_ = request.time;

  ui::PageTransition request_transition = request.transition;
  bool is_keyword_generated = ui::PageTransitionCoreTypeIs(
      request_transition, ui::PAGE_TRANSITION_KEYWORD_GENERATED);

  // If the user is navigating to a not-previously-typed intranet hostname,
  // change the transition to TYPED so that the omnibox will learn that this is
  // a known host.
  bool has_redirects = request.redirects.size() > 1;
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

  if (!has_redirects) {
    // The single entry is both a chain start and end.
    ui::PageTransition t = ui::PageTransitionFromInt(
        request_transition | ui::PAGE_TRANSITION_CHAIN_START |
        ui::PAGE_TRANSITION_CHAIN_END);

    // No redirect case (one element means just the page itself).
    last_ids = AddPageVisit(request.url, request.time, last_ids.second, t,
                            request.hidden, request.visit_source,
                            IsTypedIncrement(t), request.title);

    // Update the segment for this visit. KEYWORD_GENERATED visits should not
    // result in changing most visited, so we don't update segments (most
    // visited db).
    if (!is_keyword_generated && request.consider_for_ntp_most_visited) {
      UpdateSegments(request.url, from_visit_id, last_ids.second, t,
                     request.time);

      // Update the referrer's duration.
      UpdateVisitDuration(from_visit_id, request.time);
    }
  } else {
    // Redirect case. Add the redirect chain.

    ui::PageTransition redirect_info = ui::PAGE_TRANSITION_CHAIN_START;

    RedirectList redirects = request.redirects;
    // In the presence of client redirects, |request.redirects| can be a partial
    // chain because previous calls to this function may have reported a
    // redirect chain already. This is fine for the visits database where we'll
    // just append data but insufficient for |recent_redirects_|
    // (backpropagation of favicons and titles), where we'd like the full
    // (extended) redirect chain. We use |extended_redirect_chain| to represent
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
        DCHECK_EQ(request.referrer, redirects[0]);
        redirects.erase(redirects.begin());

        // If the navigation entry for this visit has replaced that for the
        // first visit, remove the CHAIN_END marker from the first visit. This
        // can be called a lot, for example, the page cycler, and most of the
        // time we won't have changed anything.
        VisitRow visit_row;
        if (request.did_replace_entry) {
          if (db_->GetRowForVisit(last_ids.second, &visit_row) &&
              visit_row.transition & ui::PAGE_TRANSITION_CHAIN_END) {
            visit_row.transition = ui::PageTransitionFromInt(
                visit_row.transition & ~ui::PAGE_TRANSITION_CHAIN_END);
            db_->UpdateVisitRow(visit_row);
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
      }
    }

    for (size_t redirect_index = 0; redirect_index < redirects.size();
         redirect_index++) {
      ui::PageTransition t = ui::PageTransitionFromInt(
          ui::PageTransitionStripQualifier(request_transition) | redirect_info);

      // If this is the last transition, add a CHAIN_END marker
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
      // from the redirect chain.
      last_ids =
          AddPageVisit(redirects[redirect_index], request.time, last_ids.second,
                       t, request.hidden, request.visit_source,
                       should_increment_typed_count, request.title);

      if (t & ui::PAGE_TRANSITION_CHAIN_START) {
        if (request.consider_for_ntp_most_visited) {
          UpdateSegments(redirects[redirect_index], from_visit_id,
                         last_ids.second, t, request.time);
        }

        // Update the visit_details for this visit.
        UpdateVisitDuration(from_visit_id, request.time);
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

  // TODO(brettw) bug 1140015: Add an "add page" notification so the history
  // views can keep in sync.

  // Add the last visit to the tracker so we can get outgoing transitions.
  // TODO(evanm): Due to http://b/1194536 we lose the referrers of a subframe
  // navigation anyway, so last_visit_id is always zero for them.  But adding
  // them here confuses main frame history, so we skip them for now.
  if (!ui::PageTransitionCoreTypeIs(request_transition,
                                    ui::PAGE_TRANSITION_AUTO_SUBFRAME) &&
      !ui::PageTransitionCoreTypeIs(request_transition,
                                    ui::PAGE_TRANSITION_MANUAL_SUBFRAME) &&
      !is_keyword_generated) {
    tracker_.AddVisit(request.context_id, request.nav_entry_id, request.url,
                      last_ids.second);
  }

  ScheduleCommit();
}

void HistoryBackend::InitImpl(
    const HistoryDatabaseParams& history_database_params) {
  DCHECK(!db_) << "Initializing HistoryBackend twice";
  // In the rare case where the db fails to initialize a dialog may get shown
  // the blocks the caller, yet allows other messages through. For this reason
  // we only set db_ to the created database if creation is successful. That
  // way other methods won't do anything as db_ is still null.

  TimeTicks beginning_time = TimeTicks::Now();

  // Compute the file names.
  history_dir_ = history_database_params.history_dir;
  base::FilePath history_name = history_dir_.Append(kHistoryFilename);
  base::FilePath thumbnail_name = GetFaviconsFileName();

  // Delete the old index database files which are no longer used.
  DeleteFTSIndexDatabases();

  // History database.
  db_.reset(new HistoryDatabase(
      history_database_params.download_interrupt_reason_none,
      history_database_params.download_interrupt_reason_crash));

  // Unretained to avoid a ref loop with db_.
  db_->set_error_callback(base::Bind(&HistoryBackend::DatabaseErrorCallback,
                                     base::Unretained(this)));

  db_diagnostics_.clear();
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
      FALLTHROUGH;
    }
    case sql::INIT_TOO_NEW: {
      db_diagnostics_ += sql::GetCorruptFileDiagnosticsInfo(history_name);
      delegate_->NotifyProfileError(status, db_diagnostics_);
      db_.reset();
      return;
    }
    default:
      NOTREACHED();
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

  // Thumbnail database.
  // TODO(shess): "thumbnail database" these days only stores
  // favicons.  Thumbnails are stored in "top sites".  Consider
  // renaming "thumbnail" references to "favicons" or something of the
  // sort.
  thumbnail_db_.reset(new ThumbnailDatabase(backend_client_.get()));
  if (thumbnail_db_->Init(thumbnail_name) != sql::INIT_OK) {
    // Unlike the main database, we don't error out when the database is too
    // new because this error is much less severe. Generally, this shouldn't
    // happen since the thumbnail and main database versions should be in sync.
    // We'll just continue without thumbnails & favicons in this case or any
    // other error.
    LOG(WARNING) << "Could not initialize the thumbnail database.";
    thumbnail_db_.reset();
  }

  // Generate the history and thumbnail database metrics only after performing
  // any migration work.
  if (base::RandInt(1, 100) == 50) {
    // Only do this computation sometimes since it can be expensive.
    db_->ComputeDatabaseMetrics(history_name);
    if (thumbnail_db_)
      thumbnail_db_->ComputeDatabaseMetrics();
  }

  expirer_.SetDatabases(db_.get(), thumbnail_db_.get());

  // Open the long-running transaction.
  db_->BeginTransaction();
  if (thumbnail_db_)
    thumbnail_db_->BeginTransaction();

  // Get the first item in our database.
  db_->GetStartDate(&first_recorded_time_);

  // Start expiring old stuff.
  expirer_.StartExpiringOldStuff(TimeDelta::FromDays(kExpireDaysThreshold));

#if defined(OS_ANDROID)
  if (backend_client_) {
    backend_client_->OnHistoryBackendInitialized(
        this, db_.get(), thumbnail_db_.get(), history_dir_);
  }
#endif

  LOCAL_HISTOGRAM_TIMES("History.InitTime", TimeTicks::Now() - beginning_time);
}

void HistoryBackend::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (db_)
    db_->TrimMemory();
  if (thumbnail_db_)
    thumbnail_db_->TrimMemory();
}

void HistoryBackend::CloseAllDatabases() {
  if (db_) {
    // Commit the long-running transaction.
    db_->CommitTransaction();
    db_.reset();
    // Forget the first recorded time since the database is closed.
    first_recorded_time_ = base::Time();
  }
  if (thumbnail_db_) {
    thumbnail_db_->CommitTransaction();
    thumbnail_db_.reset();
  }
}

std::pair<URLID, VisitID> HistoryBackend::AddPageVisit(
    const GURL& url,
    Time time,
    VisitID referring_visit,
    ui::PageTransition transition,
    bool hidden,
    VisitSource visit_source,
    bool should_increment_typed_count,
    base::Optional<base::string16> title) {
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
      NOTREACHED() << "Adding URL failed.";
      return std::make_pair(0, 0);
    }
    url_info.set_id(url_id);
  }

  // Add the visit with the time to the database.
  VisitRow visit_info(url_id, time, referring_visit, transition, 0,
                      should_increment_typed_count);
  VisitID visit_id = db_->AddVisit(&visit_info, visit_source);

  if (visit_info.visit_time < first_recorded_time_)
    first_recorded_time_ = visit_info.visit_time;

  // Broadcast a notification of the visit.
  if (visit_id) {
    RedirectList redirects;
    // TODO(meelapshah) Disabled due to potential PageCycler regression.
    // Re-enable this.
    // QueryRedirectsTo(url, &redirects);
    NotifyURLVisited(transition, url_info, redirects, time);
  } else {
    DVLOG(0) << "Failed to build visit insert statement:  "
             << "url_id = " << url_id;
  }

  return std::make_pair(url_id, visit_id);
}

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
        NOTREACHED() << "Could not add row to DB";
        return;
      }

      changed_urls.push_back(*i);
      changed_urls.back().set_id(url_id);  // i->id_ is likely 0.
    }

    // Sync code manages the visits itself.
    if (visit_source != SOURCE_SYNCED) {
      // Make up a visit to correspond to the last visit to the page.
      VisitRow visit_info(
          url_id, i->last_visit(), 0,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                    ui::PAGE_TRANSITION_CHAIN_START |
                                    ui::PAGE_TRANSITION_CHAIN_END),
          0, false);
      if (!db_->AddVisit(&visit_info, visit_source)) {
        NOTREACHED() << "Adding visit failed.";
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

void HistoryBackend::SetTypedURLSyncBridgeForTest(
    std::unique_ptr<TypedURLSyncBridge> bridge) {
  typed_url_sync_bridge_ = std::move(bridge);
}

bool HistoryBackend::IsExpiredVisitTime(const base::Time& time) {
  return time < expirer_.GetCurrentExpirationTime();
}

void HistoryBackend::SetPageTitle(const GURL& url,
                                  const base::string16& title) {
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
  for (size_t i = 0; i < redirects->size(); i++) {
    URLRow row;
    URLID row_id = db_->GetRowForURL(redirects->at(i), &row);
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
                                               const base::string16& title) {
  TRACE_EVENT0("browser", "HistoryBackend::AddPageNoVisitForBookmark");

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

bool HistoryBackend::GetMostRecentVisitsForURL(URLID id,
                                               int max_visits,
                                               VisitVector* visits) {
  if (db_)
    return db_->GetMostRecentVisitsForURL(id, max_visits, visits);
  return false;
}

size_t HistoryBackend::UpdateURLs(const URLRows& urls) {
  if (!db_)
    return 0;

  URLRows changed_urls;
  for (auto it = urls.begin(); it != urls.end(); ++it) {
    DCHECK(it->id());
    if (db_->UpdateURLRow(it->id(), *it))
      changed_urls.push_back(*it);
  }

  // Broadcast notifications for any URLs that have actually been changed. This
  // will update the in-memory database and the InMemoryURLIndex.
  size_t num_updated_records = changed_urls.size();
  if (num_updated_records) {
    NotifyURLsModified(changed_urls, /*is_from_expiration=*/false);
    ScheduleCommit();
  }
  return num_updated_records;
}

bool HistoryBackend::AddVisits(const GURL& url,
                               const std::vector<VisitInfo>& visits,
                               VisitSource visit_source) {
  if (db_) {
    for (auto visit = visits.begin(); visit != visits.end(); ++visit) {
      if (!AddPageVisit(url, visit->first, 0, visit->second,
                        !ui::PageTransitionIsMainFrame(visit->second),
                        visit_source, IsTypedIncrement(visit->second))
               .first) {
        return false;
      }
    }
    ScheduleCommit();
    return true;
  }
  return false;
}

bool HistoryBackend::RemoveVisits(const VisitVector& visits) {
  if (!db_)
    return false;

  expirer_.ExpireVisits(visits);
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

QueryURLResult HistoryBackend::QueryURL(const GURL& url, bool want_visits) {
  QueryURLResult result;
  result.success = db_ && db_->GetRowForURL(url, &result.row);
  // Optionally query the visits.
  if (result.success && want_visits)
    db_->GetVisitsForURL(result.row.id(), &result.visits);
  return result;
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
HistoryBackend::GetTypedURLSyncControllerDelegate() {
  DCHECK(typed_url_sync_bridge_);
  return typed_url_sync_bridge_->change_processor()->GetControllerDelegate();
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

HistoryLastVisitToHostResult HistoryBackend::GetLastVisitToHost(
    const GURL& host,
    base::Time begin_time,
    base::Time end_time) {
  base::Time last_visit;
  return {
      db_ && db_->GetLastVisitToHost(host, begin_time, end_time, &last_visit),
      last_visit};
}

// Keyword visits --------------------------------------------------------------

void HistoryBackend::SetKeywordSearchTermsForURL(const GURL& url,
                                                 KeywordID keyword_id,
                                                 const base::string16& term) {
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
                                                  const base::string16& term) {
  TRACE_EVENT0("browser", "HistoryBackend::DeleteMatchingURLsForKeyword");

  if (!db_)
    return;

  std::vector<KeywordSearchTermRow> rows;
  if (db_->GetKeywordSearchTermRows(term, &rows)) {
    std::vector<GURL> items_to_delete;
    URLRow row;
    for (auto it = rows.begin(); it != rows.end(); ++it) {
      if ((it->keyword_id == keyword_id) && db_->GetURLRow(it->url_id, &row))
        items_to_delete.push_back(row.url());
    }
    DeleteURLs(items_to_delete);
  }
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
#if defined(OS_ANDROID)
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
  for (auto it = ids.begin(); it != ids.end(); ++it) {
    db_->RemoveDownload(*it);
  }
  ScheduleCommit();
  size_t downloads_count_after = db_->CountDownloads();

  DCHECK_LE(downloads_count_after, downloads_count_before);
  if (downloads_count_after > downloads_count_before)
    return;
  size_t num_downloads_deleted = downloads_count_before - downloads_count_after;
  DCHECK_GE(ids.size(), num_downloads_deleted);
}

QueryResults HistoryBackend::QueryHistory(const base::string16& text_query,
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
  for (size_t i = 0; i < visits.size(); i++) {
    const VisitRow visit = visits[i];

    // Add a result row for this visit, get the URL info from the DB.
    if (!db_->GetURLRow(visit.url_id, &url_result)) {
      DVLOG(0) << "Failed to get id " << visit.url_id << " from history.urls.";
      continue;  // DB out of sync and URL doesn't exist, try to recover.
    }

    if (!url_result.url().is_valid()) {
      DVLOG(0) << "Got invalid URL from history.urls with id " << visit.url_id
               << ":  " << url_result.url().possibly_invalid_spec();
      continue;  // Don't report invalid URLs in case of corruption.
    }

    url_result.set_visit_time(visit.visit_time);

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
void HistoryBackend::QueryHistoryText(const base::string16& text_query,
                                      const QueryOptions& options,
                                      QueryResults* result) {
  URLRows text_matches;
  db_->GetTextMatchesWithAlgorithm(text_query, options.matching_algorithm,
                                   &text_matches);

  std::vector<URLResult> matching_visits;
  VisitVector visits;  // Declare outside loop to prevent re-construction.
  for (size_t i = 0; i < text_matches.size(); i++) {
    const URLRow& text_match = text_matches[i];
    // Get all visits for given URL match.
    db_->GetVisibleVisitsForURL(text_match.id(), options, &visits);
    for (size_t j = 0; j < visits.size(); j++) {
      URLResult url_result(text_match);
      url_result.set_visit_time(visits[j].visit_time);
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

MostVisitedURLList HistoryBackend::QueryMostVisitedURLs(int result_count,
                                                        int days_back) {
  if (!db_)
    return {};

  base::TimeTicks begin_time = base::TimeTicks::Now();

  auto url_filter = backend_client_
                        ? base::Bind(&HistoryBackendClient::IsWebSafe,
                                     base::Unretained(backend_client_.get()))
                        : base::Callback<bool(const GURL&)>();
  std::vector<std::unique_ptr<PageUsageData>> data = db_->QuerySegmentUsage(
      base::Time::Now() - base::TimeDelta::FromDays(days_back), result_count,
      url_filter);

  MostVisitedURLList result;
  for (const std::unique_ptr<PageUsageData>& current_data : data)
    result.emplace_back(current_data->GetURL(), current_data->GetTitle());

  UMA_HISTOGRAM_TIMES("History.QueryMostVisitedURLsTime",
                      base::TimeTicks::Now() - begin_time);

  return result;
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
      NOTREACHED() << "Loop in visit chain, giving up";
      return;
    }
    visit_set.insert(cur_visit);
    redirects->push_back(cur_url);
  }
}

void HistoryBackend::GetRedirectsToSpecificVisit(VisitID cur_visit,
                                                 RedirectList* redirects) {
  // Follow redirects going to cur_visit. These are added to |redirects| in
  // the order they are found. If a redirect chain looks like A -> B -> C and
  // |cur_visit| = C, redirects will be {B, A} in that order.
  if (!db_)
    return;

  GURL cur_url;
  std::set<VisitID> visit_set;
  visit_set.insert(cur_visit);
  while (db_->GetRedirectToVisit(cur_visit, &cur_visit, &cur_url)) {
    if (visit_set.find(cur_visit) != visit_set.end()) {
      NOTREACHED() << "Loop in visit chain, giving up";
      return;
    }
    visit_set.insert(cur_visit);
    redirects->push_back(cur_url);
  }
}

void HistoryBackend::ScheduleAutocomplete(
    base::OnceCallback<void(HistoryBackend*, URLDatabase*)> callback) {
  std::move(callback).Run(this, db_.get());
}

void HistoryBackend::DeleteFTSIndexDatabases() {
  // Find files on disk matching the text databases file pattern so we can
  // quickly test for and delete them.
  base::FilePath::StringType filepattern = FILE_PATH_LITERAL("History Index *");
  base::FileEnumerator enumerator(history_dir_, false,
                                  base::FileEnumerator::FILES, filepattern);
  int num_databases_deleted = 0;
  base::FilePath current_file;
  while (!(current_file = enumerator.Next()).empty()) {
    if (sql::Database::Delete(current_file))
      num_databases_deleted++;
  }
  UMA_HISTOGRAM_COUNTS_1M("History.DeleteFTSIndexDatabases",
                          num_databases_deleted);
}

std::vector<favicon_base::FaviconRawBitmapResult> HistoryBackend::GetFavicon(
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const std::vector<int>& desired_sizes) {
  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;
  UpdateFaviconMappingsAndFetchImpl(base::flat_set<GURL>(), icon_url, icon_type,
                                    desired_sizes, &bitmap_results);
  return bitmap_results;
}

favicon_base::FaviconRawBitmapResult HistoryBackend::GetLargestFaviconForURL(
    const GURL& page_url,
    const std::vector<favicon_base::IconTypeSet>& icon_types_list,
    int minimum_size_in_pixels) {
  if (!db_ || !thumbnail_db_)
    return {};

  TimeTicks beginning_time = TimeTicks::Now();

  std::vector<IconMapping> icon_mappings;
  if (!thumbnail_db_->GetIconMappingsForPageURL(page_url, &icon_mappings) ||
      icon_mappings.empty())
    return {};

  favicon_base::IconTypeSet required_icon_types;
  for (const favicon_base::IconTypeSet& icon_types : icon_types_list)
    required_icon_types.insert(icon_types.begin(), icon_types.end());

  // Find the largest bitmap for each IconType placing in
  // |largest_favicon_bitmaps|.
  std::map<favicon_base::IconType, FaviconBitmap> largest_favicon_bitmaps;
  for (std::vector<IconMapping>::const_iterator i = icon_mappings.begin();
       i != icon_mappings.end(); ++i) {
    if (required_icon_types.count(i->icon_type) == 0)
      continue;
    std::vector<FaviconBitmapIDSize> bitmap_id_sizes;
    thumbnail_db_->GetFaviconBitmapIDSizes(i->icon_id, &bitmap_id_sizes);
    FaviconBitmap& largest = largest_favicon_bitmaps[i->icon_type];
    for (std::vector<FaviconBitmapIDSize>::const_iterator j =
             bitmap_id_sizes.begin();
         j != bitmap_id_sizes.end(); ++j) {
      if (largest.bitmap_id == 0 ||
          (largest.pixel_size.width() < j->pixel_size.width() &&
           largest.pixel_size.height() < j->pixel_size.height())) {
        largest.icon_id = i->icon_id;
        largest.bitmap_id = j->bitmap_id;
        largest.pixel_size = j->pixel_size;
      }
    }
  }
  if (largest_favicon_bitmaps.empty())
    return {};

  // Find an icon which is larger than minimum_size_in_pixels in the order of
  // icon_types.
  FaviconBitmap largest_icon;
  for (const favicon_base::IconTypeSet& icon_types : icon_types_list) {
    for (std::map<favicon_base::IconType, FaviconBitmap>::const_iterator f =
             largest_favicon_bitmaps.begin();
         f != largest_favicon_bitmaps.end(); ++f) {
      if (icon_types.count(f->first) != 0 &&
          (largest_icon.bitmap_id == 0 ||
           (largest_icon.pixel_size.height() < f->second.pixel_size.height() &&
            largest_icon.pixel_size.width() < f->second.pixel_size.width()))) {
        largest_icon = f->second;
      }
    }
    if (largest_icon.pixel_size.width() > minimum_size_in_pixels &&
        largest_icon.pixel_size.height() > minimum_size_in_pixels)
      break;
  }

  GURL icon_url;
  favicon_base::IconType icon_type;
  if (!thumbnail_db_->GetFaviconHeader(largest_icon.icon_id, &icon_url,
                                       &icon_type)) {
    return {};
  }

  base::Time last_updated;
  base::Time last_requested;
  favicon_base::FaviconRawBitmapResult bitmap_result;
  bitmap_result.icon_url = icon_url;
  bitmap_result.icon_type = icon_type;
  if (!thumbnail_db_->GetFaviconBitmap(
          largest_icon.bitmap_id, &last_updated, &last_requested,
          &bitmap_result.bitmap_data, &bitmap_result.pixel_size)) {
    return {};
  }

  bitmap_result.expired = IsFaviconBitmapExpired(last_updated);
  bitmap_result.fetched_because_of_page_visit = last_requested.is_null();

  LOCAL_HISTOGRAM_TIMES("History.GetLargestFaviconForURL",
                        TimeTicks::Now() - beginning_time);

  if (!bitmap_result.is_valid())
    return {};

  return bitmap_result;
}

std::vector<favicon_base::FaviconRawBitmapResult>
HistoryBackend::GetFaviconsForURL(const GURL& page_url,
                                  const favicon_base::IconTypeSet& icon_types,
                                  const std::vector<int>& desired_sizes,
                                  bool fallback_to_host) {
  TRACE_EVENT0("browser", "HistoryBackend::GetFaviconsForURL");

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;
  GetFaviconsFromDB(page_url, icon_types, desired_sizes, fallback_to_host,
                    &bitmap_results);

  if (desired_sizes.size() == 1)
    bitmap_results.assign(1, favicon_base::ResizeFaviconBitmapResult(
                                 bitmap_results, desired_sizes[0]));
  return bitmap_results;
}

std::vector<favicon_base::FaviconRawBitmapResult>
HistoryBackend::GetFaviconForID(favicon_base::FaviconID favicon_id,
                                int desired_size) {
  TRACE_EVENT0("browser", "HistoryBackend::GetFaviconForID");
  std::vector<favicon_base::FaviconID> favicon_ids;
  favicon_ids.push_back(favicon_id);
  std::vector<int> desired_sizes;
  desired_sizes.push_back(desired_size);

  // Get results from DB.
  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;
  GetFaviconBitmapResultsForBestMatch(favicon_ids, desired_sizes,
                                      &bitmap_results);

  bitmap_results.assign(
      1, favicon_base::ResizeFaviconBitmapResult(bitmap_results, desired_size));

  return bitmap_results;
}

std::vector<favicon_base::FaviconRawBitmapResult>
HistoryBackend::UpdateFaviconMappingsAndFetch(
    const base::flat_set<GURL>& page_urls,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const std::vector<int>& desired_sizes) {
  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;
  UpdateFaviconMappingsAndFetchImpl(page_urls, icon_url, icon_type,
                                    desired_sizes, &bitmap_results);
  return bitmap_results;
}

void HistoryBackend::DeleteFaviconMappings(
    const base::flat_set<GURL>& page_urls,
    favicon_base::IconType icon_type) {
  TRACE_EVENT0("browser", "HistoryBackend::DeleteFaviconMappings");

  if (!thumbnail_db_ || !db_)
    return;

  for (const GURL& page_url : page_urls) {
    bool mapping_changed = SetFaviconMappingsForPageAndRedirects(
        page_url, icon_type, /*icon_id=*/0);

    if (mapping_changed) {
      // Notify the UI that this function changed an icon mapping.
      SendFaviconChangedNotificationForPageAndRedirects(page_url);
      ScheduleCommit();
    }
  }
}

void HistoryBackend::MergeFavicon(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    scoped_refptr<base::RefCountedMemory> bitmap_data,
    const gfx::Size& pixel_size) {
  TRACE_EVENT0("browser", "HistoryBackend::MergeFavicon");

  if (!thumbnail_db_ || !db_)
    return;

  favicon_base::FaviconID favicon_id =
      thumbnail_db_->GetFaviconIDForFaviconURL(icon_url, icon_type);

  if (!favicon_id) {
    // There is no favicon at |icon_url|, create it.
    favicon_id = thumbnail_db_->AddFavicon(icon_url, icon_type);
  }

  std::vector<FaviconBitmapIDSize> bitmap_id_sizes;
  thumbnail_db_->GetFaviconBitmapIDSizes(favicon_id, &bitmap_id_sizes);

  // If there is already a favicon bitmap of |pixel_size| at |icon_url|,
  // replace it.
  bool bitmap_identical = false;
  bool replaced_bitmap = false;
  for (size_t i = 0; i < bitmap_id_sizes.size(); ++i) {
    if (bitmap_id_sizes[i].pixel_size == pixel_size) {
      if (IsFaviconBitmapDataEqual(bitmap_id_sizes[i].bitmap_id, bitmap_data)) {
        // Sync calls MergeFavicon() for all of the favicons that it manages at
        // startup. Do not update the "last updated" time if the favicon bitmap
        // data matches that in the database.
        // TODO(pkotwicz): Pass in boolean to MergeFavicon() if any users of
        // MergeFavicon() want the last_updated time to be updated when the new
        // bitmap data is identical to the old.
        bitmap_identical = true;
      } else {
        // Expire the favicon bitmap because sync can provide incorrect
        // |bitmap_data|. See crbug.com/474421 for more details. Expiring the
        // favicon bitmap causes it to be redownloaded the next time that the
        // user visits any page which uses |icon_url|. It also allows storing an
        // on-demand icon along with the icon from sync.
        thumbnail_db_->SetFaviconBitmap(bitmap_id_sizes[i].bitmap_id,
                                        bitmap_data, base::Time());
        replaced_bitmap = true;
      }
      break;
    }
  }

  // Create a vector of the pixel sizes of the favicon bitmaps currently at
  // |icon_url|.
  std::vector<gfx::Size> favicon_sizes;
  for (size_t i = 0; i < bitmap_id_sizes.size(); ++i)
    favicon_sizes.push_back(bitmap_id_sizes[i].pixel_size);

  if (!replaced_bitmap && !bitmap_identical) {
    // Set the preexisting favicon bitmaps as expired as the preexisting favicon
    // bitmaps are not consistent with the merged in data.
    thumbnail_db_->SetFaviconOutOfDate(favicon_id);

    // Delete an arbitrary favicon bitmap to avoid going over the limit of
    // |kMaxFaviconBitmapsPerIconURL|.
    if (bitmap_id_sizes.size() >= kMaxFaviconBitmapsPerIconURL) {
      thumbnail_db_->DeleteFaviconBitmap(bitmap_id_sizes[0].bitmap_id);
      favicon_sizes.erase(favicon_sizes.begin());
    }
    // Set the new bitmap as expired because the bitmaps from sync/profile
    // import/etc. are not authoritative. Expiring the favicon bitmap causes the
    // bitmaps to be redownloaded the next time that the user visits any page
    // which uses |icon_url|. It also allows storing an on-demand icon along
    // with the icon from sync.
    thumbnail_db_->AddFaviconBitmap(favicon_id, bitmap_data,
                                    FaviconBitmapType::ON_VISIT, base::Time(),
                                    pixel_size);
    favicon_sizes.push_back(pixel_size);
  }

  // A site may have changed the favicons that it uses for |page_url|.
  // Example Scenario:
  //   page_url = news.google.com
  //   Initial State: www.google.com/favicon.ico 16x16, 32x32
  //   MergeFavicon(news.google.com, news.google.com/news_specific.ico, ...,
  //                ..., 16x16)
  //
  // Difficulties:
  // 1. Sync requires that a call to GetFaviconsForURL() returns the
  //    |bitmap_data| passed into MergeFavicon().
  //    - It is invalid for the 16x16 bitmap for www.google.com/favicon.ico to
  //      stay mapped to news.google.com because it would be unclear which 16x16
  //      bitmap should be returned via GetFaviconsForURL().
  //
  // 2. www.google.com/favicon.ico may be mapped to more than just
  //    news.google.com (eg www.google.com).
  //    - The 16x16 bitmap cannot be deleted from www.google.com/favicon.ico
  //
  // To resolve these problems, we copy all of the favicon bitmaps previously
  // mapped to news.google.com (|page_url|) and add them to the favicon at
  // news.google.com/news_specific.ico (|icon_url|). The favicon sizes for
  // |icon_url| are set to default to indicate that |icon_url| has incomplete
  // / incorrect data.
  // Difficulty 1: All but news.google.com/news_specific.ico are unmapped from
  //              news.google.com
  // Difficulty 2: The favicon bitmaps for www.google.com/favicon.ico are not
  //               modified.

  std::vector<IconMapping> icon_mappings;
  thumbnail_db_->GetIconMappingsForPageURL(page_url, {icon_type},
                                           &icon_mappings);

  // Copy the favicon bitmaps mapped to |page_url| to the favicon at |icon_url|
  // till the limit of |kMaxFaviconBitmapsPerIconURL| is reached.
  bool favicon_bitmaps_copied = false;
  for (size_t i = 0; i < icon_mappings.size(); ++i) {
    if (favicon_sizes.size() >= kMaxFaviconBitmapsPerIconURL)
      break;

    if (icon_mappings[i].icon_url == icon_url)
      continue;

    std::vector<FaviconBitmap> bitmaps_to_copy;
    thumbnail_db_->GetFaviconBitmaps(icon_mappings[i].icon_id,
                                     &bitmaps_to_copy);
    for (size_t j = 0; j < bitmaps_to_copy.size(); ++j) {
      // Do not add a favicon bitmap at a pixel size for which there is already
      // a favicon bitmap mapped to |icon_url|. The one there is more correct
      // and having multiple equally sized favicon bitmaps for |page_url| is
      // ambiguous in terms of GetFaviconsForURL().
      if (base::Contains(favicon_sizes, bitmaps_to_copy[j].pixel_size))
        continue;

      // Add the favicon bitmap as expired as it is not consistent with the
      // merged in data.
      thumbnail_db_->AddFaviconBitmap(favicon_id,
                                      bitmaps_to_copy[j].bitmap_data,
                                      FaviconBitmapType::ON_VISIT, base::Time(),
                                      bitmaps_to_copy[j].pixel_size);
      favicon_sizes.push_back(bitmaps_to_copy[j].pixel_size);
      favicon_bitmaps_copied = true;

      if (favicon_sizes.size() >= kMaxFaviconBitmapsPerIconURL)
        break;
    }
  }

  // Update the favicon mappings such that only |icon_url| is mapped to
  // |page_url|.
  if (icon_mappings.size() != 1 || icon_mappings[0].icon_url != icon_url) {
    SetFaviconMappingsForPageAndRedirects(page_url, icon_type, favicon_id);
    SendFaviconChangedNotificationForPageAndRedirects(page_url);
  }

  if (!bitmap_identical || favicon_bitmaps_copied) {
    // Notify the UI that the favicon has changed for |icon_url|.
    SendFaviconChangedNotificationForIconURL(icon_url);
  }

  ScheduleCommit();
}

void HistoryBackend::SetFavicons(const base::flat_set<GURL>& page_urls,
                                 favicon_base::IconType icon_type,
                                 const GURL& icon_url,
                                 const std::vector<SkBitmap>& bitmaps) {
  TRACE_EVENT0("browser", "HistoryBackend::SetFavicons");
  SetFaviconsImpl(page_urls, icon_type, icon_url, bitmaps,
                  FaviconBitmapType::ON_VISIT);
}

void HistoryBackend::CloneFaviconMappingsForPages(
    const GURL& page_url_to_read,
    const favicon_base::IconTypeSet& icon_types,
    const base::flat_set<GURL>& page_urls_to_write) {
  TRACE_EVENT0("browser", "HistoryBackend::CloneFaviconMappingsForPages");

  if (!db_ || !thumbnail_db_)
    return;

  // Update mappings including redirects for each entry in |page_urls_to_write|.
  base::flat_set<GURL> page_urls_to_update_mappings;
  for (const GURL& update_mappings_for_page : page_urls_to_write) {
    RedirectList redirects = GetCachedRecentRedirects(update_mappings_for_page);
    page_urls_to_update_mappings.insert(redirects.begin(), redirects.end());
  }

  // No need to update mapping for |page_url_to_read|, because this is where
  // we're getting the mappings from.
  page_urls_to_update_mappings.erase(page_url_to_read);

  if (page_urls_to_update_mappings.empty())
    return;

  // Get FaviconIDs for |page_url_to_read| and one of |icon_types|.
  std::vector<IconMapping> icon_mappings;
  thumbnail_db_->GetIconMappingsForPageURL(page_url_to_read, icon_types,
                                           &icon_mappings);
  if (icon_mappings.empty())
    return;

  std::set<GURL> changed_page_urls;
  for (const IconMapping& icon_mapping : icon_mappings) {
    std::vector<GURL> v = SetFaviconMappingsForPages(
        page_urls_to_update_mappings, icon_mapping.icon_type,
        icon_mapping.icon_id);
    changed_page_urls.insert(std::make_move_iterator(v.begin()),
                             std::make_move_iterator(v.end()));
  }

  if (!changed_page_urls.empty()) {
    NotifyFaviconsChanged(changed_page_urls, GURL());
    ScheduleCommit();
  }
}

bool HistoryBackend::CanSetOnDemandFavicons(const GURL& page_url,
                                            favicon_base::IconType icon_type) {
  if (!thumbnail_db_ || !db_)
    return false;

  // We allow writing an on demand favicon of type |icon_type| only if there is
  // no icon of such type in the DB (so that we never overwrite anything) and if
  // all other icons are expired. This in particular allows writing an on-demand
  // icon if there is only an icon from sync (icons from sync are immediately
  // set as expired).
  std::vector<IconMapping> mapping_data;
  thumbnail_db_->GetIconMappingsForPageURL(page_url, &mapping_data);

  for (const IconMapping& mapping : mapping_data) {
    if (AreIconTypesEquivalent(mapping.icon_type, icon_type))
      return false;

    base::Time last_updated;
    if (thumbnail_db_->GetFaviconLastUpdatedTime(mapping.icon_id,
                                                 &last_updated) &&
        !IsFaviconBitmapExpired(last_updated)) {
      return false;
    }
  }
  return true;
}

bool HistoryBackend::SetOnDemandFavicons(const GURL& page_url,
                                         favicon_base::IconType icon_type,
                                         const GURL& icon_url,
                                         const std::vector<SkBitmap>& bitmaps) {
  if (!CanSetOnDemandFavicons(page_url, icon_type))
    return false;

  return SetFaviconsImpl({page_url}, icon_type, icon_url, bitmaps,
                         FaviconBitmapType::ON_DEMAND);
}

void HistoryBackend::SetFaviconsOutOfDateForPage(const GURL& page_url) {
  TRACE_EVENT0("browser", "HistoryBackend::SetFaviconsOutOfDateForPage");

  std::vector<IconMapping> icon_mappings;

  if (!thumbnail_db_ ||
      !thumbnail_db_->GetIconMappingsForPageURL(page_url, &icon_mappings))
    return;

  for (auto m = icon_mappings.begin(); m != icon_mappings.end(); ++m) {
    thumbnail_db_->SetFaviconOutOfDate(m->icon_id);
  }
  ScheduleCommit();
}

void HistoryBackend::TouchOnDemandFavicon(const GURL& icon_url) {
  TRACE_EVENT0("browser", "HistoryBackend::TouchOnDemandFavicon");

  if (!thumbnail_db_)
    return;

  thumbnail_db_->TouchOnDemandFavicon(icon_url, Time::Now());
  ScheduleCommit();
}

void HistoryBackend::SetImportedFavicons(
    const favicon_base::FaviconUsageDataList& favicon_usage) {
  TRACE_EVENT0("browser", "HistoryBackend::SetImportedFavicons");

  if (!db_ || !thumbnail_db_)
    return;

  Time now = Time::Now();

  // Track all URLs that had their favicons set or updated.
  std::set<GURL> favicons_changed;

  for (size_t i = 0; i < favicon_usage.size(); i++) {
    favicon_base::FaviconID favicon_id =
        thumbnail_db_->GetFaviconIDForFaviconURL(
            favicon_usage[i].favicon_url, favicon_base::IconType::kFavicon);
    if (!favicon_id) {
      // This favicon doesn't exist yet, so we create it using the given data.
      // TODO(pkotwicz): Pass in real pixel size.
      favicon_id = thumbnail_db_->AddFavicon(
          favicon_usage[i].favicon_url, favicon_base::IconType::kFavicon,
          new base::RefCountedBytes(favicon_usage[i].png_data),
          FaviconBitmapType::ON_VISIT, now, gfx::Size());
    }

    // Save the mapping from all the URLs to the favicon.
    for (auto url = favicon_usage[i].urls.begin();
         url != favicon_usage[i].urls.end(); ++url) {
      URLRow url_row;
      if (!db_->GetRowForURL(*url, &url_row)) {
        // If the URL is present as a bookmark, add the url in history to
        // save the favicon mapping. This will match with what history db does
        // for regular bookmarked URLs with favicons - when history db is
        // cleaned, we keep an entry in the db with 0 visits as long as that
        // url is bookmarked. The same is applicable to the saved credential's
        // URLs.
        if (backend_client_ && backend_client_->IsPinnedURL(*url)) {
          URLRow url_info(*url);
          url_info.set_visit_count(0);
          url_info.set_typed_count(0);
          url_info.set_last_visit(base::Time());
          url_info.set_hidden(false);
          db_->AddURL(url_info);
          thumbnail_db_->AddIconMapping(*url, favicon_id);
          favicons_changed.insert(*url);
        }
      } else {
        if (!thumbnail_db_->GetIconMappingsForPageURL(
                *url, {favicon_base::IconType::kFavicon},
                /*mapping_data=*/nullptr)) {
          // URL is present in history, update the favicon *only* if it is not
          // set already.
          thumbnail_db_->AddIconMapping(*url, favicon_id);
          favicons_changed.insert(*url);
        }
      }
    }
  }

  if (!favicons_changed.empty()) {
    // Send the notification about the changed favicon URLs.
    NotifyFaviconsChanged(favicons_changed, GURL());
  }
}

bool HistoryBackend::SetFaviconsImpl(const base::flat_set<GURL>& page_urls,
                                     favicon_base::IconType icon_type,
                                     const GURL& icon_url,
                                     const std::vector<SkBitmap>& bitmaps,
                                     FaviconBitmapType type) {
  DCHECK(!page_urls.empty());

  if (!thumbnail_db_ || !db_)
    return false;

  DCHECK_GE(kMaxFaviconBitmapsPerIconURL, bitmaps.size());

  favicon_base::FaviconID icon_id =
      thumbnail_db_->GetFaviconIDForFaviconURL(icon_url, icon_type);

  if (type == FaviconBitmapType::ON_DEMAND) {
    UMA_HISTOGRAM_BOOLEAN("Favicon.OnDemandIconExistsInDb",
                          static_cast<bool>(icon_id));
  }

  bool favicon_created = false;
  if (!icon_id) {
    icon_id = thumbnail_db_->AddFavicon(icon_url, icon_type);
    favicon_created = true;
  }

  bool favicon_data_modified = false;
  if (favicon_created || type == FaviconBitmapType::ON_VISIT) {
    favicon_data_modified = SetFaviconBitmaps(icon_id, bitmaps, type);
  }

  for (const GURL& page_url : page_urls) {
    bool mapping_changed =
        SetFaviconMappingsForPageAndRedirects(page_url, icon_type, icon_id);

    if (mapping_changed) {
      // Notify the UI that this function changed an icon mapping.
      SendFaviconChangedNotificationForPageAndRedirects(page_url);
    }
  }

  if (favicon_data_modified) {
    // Notify the UI that the favicon has changed for |icon_url|.
    SendFaviconChangedNotificationForIconURL(icon_url);
  }
  ScheduleCommit();
  return favicon_data_modified;
}

void HistoryBackend::UpdateFaviconMappingsAndFetchImpl(
    const base::flat_set<GURL>& page_urls,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const std::vector<int>& desired_sizes,
    std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results) {
  bitmap_results->clear();

  if (!thumbnail_db_)
    return;

  const favicon_base::FaviconID favicon_id =
      thumbnail_db_->GetFaviconIDForFaviconURL(icon_url, icon_type);
  if (!favicon_id)
    return;

  for (const GURL& page_url : page_urls) {
    bool mappings_updated =
        SetFaviconMappingsForPageAndRedirects(page_url, icon_type, favicon_id);
    if (mappings_updated) {
      SendFaviconChangedNotificationForPageAndRedirects(page_url);
      ScheduleCommit();
    }
  }

  GetFaviconBitmapResultsForBestMatch({favicon_id}, desired_sizes,
                                      bitmap_results);
}

bool HistoryBackend::SetFaviconBitmaps(favicon_base::FaviconID icon_id,
                                       const std::vector<SkBitmap>& bitmaps,
                                       FaviconBitmapType type) {
  std::vector<FaviconBitmapIDSize> bitmap_id_sizes;
  thumbnail_db_->GetFaviconBitmapIDSizes(icon_id, &bitmap_id_sizes);

  using PNGEncodedBitmap =
      std::pair<scoped_refptr<base::RefCountedBytes>, gfx::Size>;
  std::vector<PNGEncodedBitmap> to_add;
  for (size_t i = 0; i < bitmaps.size(); ++i) {
    scoped_refptr<base::RefCountedBytes> bitmap_data(new base::RefCountedBytes);
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmaps[i], false,
                                           &bitmap_data->data())) {
      continue;
    }
    to_add.push_back(std::make_pair(
        bitmap_data, gfx::Size(bitmaps[i].width(), bitmaps[i].height())));
  }

  bool favicon_bitmaps_changed = false;
  for (size_t i = 0; i < bitmap_id_sizes.size(); ++i) {
    const gfx::Size& pixel_size = bitmap_id_sizes[i].pixel_size;
    auto match_it = to_add.end();
    for (auto it = to_add.begin(); it != to_add.end(); ++it) {
      if (it->second == pixel_size) {
        match_it = it;
        break;
      }
    }

    FaviconBitmapID bitmap_id = bitmap_id_sizes[i].bitmap_id;
    if (match_it == to_add.end()) {
      thumbnail_db_->DeleteFaviconBitmap(bitmap_id);

      favicon_bitmaps_changed = true;
    } else {
      if (!favicon_bitmaps_changed &&
          IsFaviconBitmapDataEqual(bitmap_id, match_it->first)) {
        thumbnail_db_->SetFaviconBitmapLastUpdateTime(
            bitmap_id, base::Time::Now() /* new last updated time */);
      } else {
        thumbnail_db_->SetFaviconBitmap(
            bitmap_id, match_it->first,
            base::Time::Now() /* new last updated time */);
        favicon_bitmaps_changed = true;
      }
      to_add.erase(match_it);
    }
  }

  for (size_t i = 0; i < to_add.size(); ++i) {
    thumbnail_db_->AddFaviconBitmap(
        icon_id, to_add[i].first, type,
        base::Time::Now() /* new last updated / last requested time */,
        to_add[i].second);

    favicon_bitmaps_changed = true;
  }
  return favicon_bitmaps_changed;
}

bool HistoryBackend::IsFaviconBitmapDataEqual(
    FaviconBitmapID bitmap_id,
    const scoped_refptr<base::RefCountedMemory>& new_bitmap_data) {
  if (!new_bitmap_data)
    return false;

  scoped_refptr<base::RefCountedMemory> original_bitmap_data;
  thumbnail_db_->GetFaviconBitmap(bitmap_id, nullptr, nullptr,
                                  &original_bitmap_data, nullptr);
  return new_bitmap_data->Equals(original_bitmap_data);
}

bool HistoryBackend::GetFaviconsFromDB(
    const GURL& page_url,
    const favicon_base::IconTypeSet& icon_types,
    const std::vector<int>& desired_sizes,
    bool fallback_to_host,
    std::vector<favicon_base::FaviconRawBitmapResult>* favicon_bitmap_results) {
  DCHECK(favicon_bitmap_results);
  favicon_bitmap_results->clear();

  if (!db_ || !thumbnail_db_)
    return false;

  // Time the query.
  TimeTicks beginning_time = TimeTicks::Now();

  // Get FaviconIDs for |page_url| and one of |icon_types|.
  std::vector<IconMapping> icon_mappings;
  thumbnail_db_->GetIconMappingsForPageURL(page_url, icon_types,
                                           &icon_mappings);

  if (icon_mappings.empty() && fallback_to_host &&
      page_url.SchemeIsHTTPOrHTTPS()) {
    // We didn't find any matches, and the caller requested falling back to the
    // host of |page_url| for fuzzy matching. Query the database for a page_url
    // that is known to exist and matches the host of |page_url|. Do this only
    // if we have a HTTP/HTTPS url.
    base::Optional<GURL> fallback_page_url =
        thumbnail_db_->FindFirstPageURLForHost(page_url, icon_types);

    if (fallback_page_url) {
      thumbnail_db_->GetIconMappingsForPageURL(fallback_page_url.value(),
                                               icon_types, &icon_mappings);
    }
  }

  std::vector<favicon_base::FaviconID> favicon_ids;
  for (size_t i = 0; i < icon_mappings.size(); ++i)
    favicon_ids.push_back(icon_mappings[i].icon_id);

  // Populate |favicon_bitmap_results| and |icon_url_sizes|.
  bool success = GetFaviconBitmapResultsForBestMatch(favicon_ids, desired_sizes,
                                                     favicon_bitmap_results);
  UMA_HISTOGRAM_TIMES("History.GetFavIconFromDB",  // historical name
                      TimeTicks::Now() - beginning_time);
  return success && !favicon_bitmap_results->empty();
}

bool HistoryBackend::GetFaviconBitmapResultsForBestMatch(
    const std::vector<favicon_base::FaviconID>& candidate_favicon_ids,
    const std::vector<int>& desired_sizes,
    std::vector<favicon_base::FaviconRawBitmapResult>* favicon_bitmap_results) {
  favicon_bitmap_results->clear();

  if (candidate_favicon_ids.empty())
    return true;

  // Find the FaviconID and the FaviconBitmapIDs which best match
  // |desired_size_in_dip| and |desired_scale_factors|.
  // TODO(pkotwicz): Select bitmap results from multiple favicons once
  // content::FaviconStatus supports multiple icon URLs.
  favicon_base::FaviconID best_favicon_id = 0;
  std::vector<FaviconBitmapID> best_bitmap_ids;
  float highest_score = kSelectFaviconFramesInvalidScore;
  for (size_t i = 0; i < candidate_favicon_ids.size(); ++i) {
    std::vector<FaviconBitmapIDSize> bitmap_id_sizes;
    thumbnail_db_->GetFaviconBitmapIDSizes(candidate_favicon_ids[i],
                                           &bitmap_id_sizes);

    // Build vector of gfx::Size from |bitmap_id_sizes|.
    std::vector<gfx::Size> sizes;
    for (size_t j = 0; j < bitmap_id_sizes.size(); ++j)
      sizes.push_back(bitmap_id_sizes[j].pixel_size);

    std::vector<size_t> candidate_bitmap_indices;
    float score = 0;
    SelectFaviconFrameIndices(sizes, desired_sizes, &candidate_bitmap_indices,
                              &score);
    if (score > highest_score) {
      highest_score = score;
      best_favicon_id = candidate_favicon_ids[i];
      best_bitmap_ids.clear();
      for (size_t j = 0; j < candidate_bitmap_indices.size(); ++j) {
        size_t candidate_index = candidate_bitmap_indices[j];
        best_bitmap_ids.push_back(bitmap_id_sizes[candidate_index].bitmap_id);
      }
    }
  }

  // Construct FaviconRawBitmapResults from |best_favicon_id| and
  // |best_bitmap_ids|.
  GURL icon_url;
  favicon_base::IconType icon_type;
  if (!thumbnail_db_->GetFaviconHeader(best_favicon_id, &icon_url,
                                       &icon_type)) {
    return false;
  }

  for (size_t i = 0; i < best_bitmap_ids.size(); ++i) {
    base::Time last_updated;
    base::Time last_requested;
    favicon_base::FaviconRawBitmapResult bitmap_result;
    bitmap_result.icon_url = icon_url;
    bitmap_result.icon_type = icon_type;
    if (!thumbnail_db_->GetFaviconBitmap(
            best_bitmap_ids[i], &last_updated, &last_requested,
            &bitmap_result.bitmap_data, &bitmap_result.pixel_size)) {
      return false;
    }

    bitmap_result.expired =
        (Time::Now() - last_updated) > TimeDelta::FromDays(kFaviconRefetchDays);
    bitmap_result.fetched_because_of_page_visit = last_requested.is_null();
    if (bitmap_result.is_valid())
      favicon_bitmap_results->push_back(bitmap_result);
  }
  return true;
}

bool HistoryBackend::SetFaviconMappingsForPageAndRedirects(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    favicon_base::FaviconID icon_id) {
  if (!thumbnail_db_)
    return false;

  // Find all the pages whose favicons we should set, we want to set it for
  // all the pages in the redirect chain if it redirected.
  RedirectList redirects = GetCachedRecentRedirects(page_url);
  bool mappings_changed =
      !SetFaviconMappingsForPages(base::flat_set<GURL>(redirects), icon_type,
                                  icon_id)
           .empty();
  return mappings_changed;
}

std::vector<GURL> HistoryBackend::SetFaviconMappingsForPages(
    const base::flat_set<GURL>& page_urls,
    favicon_base::IconType icon_type,
    favicon_base::FaviconID icon_id) {
  std::vector<GURL> changed_page_urls;
  for (const GURL& page_url : page_urls) {
    if (SetFaviconMappingsForPage(page_url, icon_type, icon_id))
      changed_page_urls.push_back(page_url);
  }
  return changed_page_urls;
}

bool HistoryBackend::SetFaviconMappingsForPage(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    favicon_base::FaviconID icon_id) {
  bool mappings_changed = false;
  // Sets the icon mappings from |page_url| for |icon_type| to the favicon
  // with |icon_id|. Mappings for |page_url| to favicons of type |icon_type|
  // with FaviconID other than |icon_id| are removed. All icon mappings for
  // |page_url| to favicons of a type equivalent to |icon_type| are removed.
  // Remove any favicons which are orphaned as a result of the removal of the
  // icon mappings.

  favicon_base::FaviconID unmapped_icon_id = icon_id;

  std::vector<IconMapping> icon_mappings;
  thumbnail_db_->GetIconMappingsForPageURL(page_url, &icon_mappings);

  for (auto m = icon_mappings.begin(); m != icon_mappings.end(); ++m) {
    if (unmapped_icon_id == m->icon_id) {
      unmapped_icon_id = 0;
      continue;
    }

    if (AreIconTypesEquivalent(icon_type, m->icon_type)) {
      thumbnail_db_->DeleteIconMapping(m->mapping_id);

      // Removing the icon mapping may have orphaned the associated favicon so
      // we must recheck it. This is not super fast, but this case will get
      // triggered rarely, since normally a page will always map to the same
      // favicon IDs. It will mostly happen for favicons we import.
      if (!thumbnail_db_->HasMappingFor(m->icon_id))
        thumbnail_db_->DeleteFavicon(m->icon_id);
      mappings_changed = true;
    }
  }

  if (unmapped_icon_id) {
    thumbnail_db_->AddIconMapping(page_url, unmapped_icon_id);
    mappings_changed = true;
  }
  return mappings_changed;
}

RedirectList HistoryBackend::GetCachedRecentRedirects(const GURL& page_url) {
  auto iter = recent_redirects_.Get(page_url);
  if (iter != recent_redirects_.end()) {
    // The redirect chain should have the destination URL as the last item.
    DCHECK(!iter->second.empty());
    DCHECK_EQ(iter->second.back(), page_url);
    return iter->second;
  }
  // No known redirects, construct mock redirect chain containing |page_url|.
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
  if (!db_)
    return;

#if defined(OS_IOS)
  // Attempts to get the application running long enough to commit the database
  // transaction if it is currently being backgrounded.
  base::ios::ScopedCriticalAction scoped_critical_action;
#endif

  // Note that a commit may not actually have been scheduled if a caller
  // explicitly calls this instead of using ScheduleCommit. Likewise, we
  // may reset the flag written by a pending commit. But this is OK! It
  // will merely cause extra commits (which is kind of the idea). We
  // could optimize more for this case (we may get two extra commits in
  // some cases) but it hasn't been important yet.
  CancelScheduledCommit();

  db_->CommitTransaction();
  DCHECK_EQ(db_->transaction_nesting(), 0)
      << "Somebody left a transaction open";
  db_->BeginTransaction();

  if (thumbnail_db_) {
    thumbnail_db_->CommitTransaction();
    DCHECK_EQ(thumbnail_db_->transaction_nesting(), 0)
        << "Somebody left a transaction open";
    thumbnail_db_->BeginTransaction();
  }
}

void HistoryBackend::ScheduleCommit() {
  // Non-cancelled means there's an already scheduled commit. Note that
  // CancelableClosure starts cancelled with the default constructor.
  if (!scheduled_commit_.IsCancelled())
    return;

  scheduled_commit_.Reset(
      base::Bind(&HistoryBackend::Commit, base::Unretained(this)));

  task_runner_->PostDelayedTask(
      FROM_HERE, scheduled_commit_.callback(),
      base::TimeDelta::FromSeconds(kCommitIntervalSeconds));
}

void HistoryBackend::CancelScheduledCommit() {
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
    // tasks, and process it after an invoke later.
    queued_history_db_tasks_.push_back(std::move(task));
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryBackend::ProcessDBTaskImpl, this));
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// Generic operations
//
////////////////////////////////////////////////////////////////////////////////

void HistoryBackend::DeleteURLs(const std::vector<GURL>& urls) {
  TRACE_EVENT0("browser", "HistoryBackend::DeleteURLs");

  expirer_.DeleteURLs(urls, base::Time::Max());

  db_->GetStartDate(&first_recorded_time_);
  // Force a commit, if the user is deleting something for privacy reasons, we
  // want to get it on disk ASAP.
  Commit();
}

void HistoryBackend::DeleteURL(const GURL& url) {
  TRACE_EVENT0("browser", "HistoryBackend::DeleteURL");

  expirer_.DeleteURL(url, base::Time::Max());

  db_->GetStartDate(&first_recorded_time_);
  // Force a commit, if the user is deleting something for privacy reasons, we
  // want to get it on disk ASAP.
  Commit();
}

void HistoryBackend::DeleteURLsUntil(
    const std::vector<std::pair<GURL, base::Time>>& urls_and_timestamps) {
  TRACE_EVENT0("browser", "HistoryBackend::DeleteURLsUntil");

  for (const auto& pair : urls_and_timestamps) {
    expirer_.DeleteURL(pair.first, pair.second);
  }
  db_->GetStartDate(&first_recorded_time_);
  // Force a commit, if the user is deleting something for privacy reasons, we
  // want to get it on disk ASAP.
  Commit();
}

void HistoryBackend::ExpireHistoryBetween(const std::set<GURL>& restrict_urls,
                                          Time begin_time,
                                          Time end_time,
                                          bool user_initiated) {
  if (!db_)
    return;

  if (begin_time.is_null() && (end_time.is_null() || end_time.is_max()) &&
      restrict_urls.empty()) {
    // Special case deleting all history so it can be faster and to reduce the
    // possibility of an information leak.
    DeleteAllHistory();
  } else {
    // Clearing parts of history, have the expirer do the depend
    expirer_.ExpireHistoryBetween(restrict_urls, begin_time, end_time,
                                  user_initiated);

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

  // 1st pass: find URLs that are visited at one of |times|.
  std::set<GURL> urls;
  for (size_t i = 0; i < results.size(); ++i) {
    if (times.count(results[i].visit_time()) > 0)
      urls.insert(results[i].url());
  }
  if (urls.empty())
    return;

  // 2nd pass: collect all visit times of those URLs.
  std::vector<base::Time> times_to_expire;
  for (size_t i = 0; i < results.size(); ++i) {
    if (urls.count(results[i].url()))
      times_to_expire.push_back(results[i].visit_time());
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
  // Update |first_recorded_time_| if we expired it.
  if (times_to_expire.back() == first_recorded_time_)
    db_->GetStartDate(&first_recorded_time_);
}

void HistoryBackend::ExpireHistory(
    const std::vector<ExpireHistoryArgs>& expire_list) {
  if (db_) {
    bool update_first_recorded_time = false;

    for (auto it = expire_list.begin(); it != expire_list.end(); ++it) {
      expirer_.ExpireHistoryBetween(it->urls, it->begin_time, it->end_time,
                                    true);

      if (it->begin_time < first_recorded_time_)
        update_first_recorded_time = true;
    }
    Commit();

    // Update |first_recorded_time_| if any deletion might have affected it.
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

  for (auto i = urls.begin(); i != urls.end(); ++i) {
    VisitVector visits;
    URLRow url_row;
    if (db_->GetRowForURL(*i, &url_row))
      db_->GetVisitsForURL(url_row.id(), &visits);
    // We need to call DeleteURL() even if the DB didn't contain this URL, so
    // that we can delete all associated icons in the case of deleting an
    // unvisited bookmarked URL.
    if (visits.empty())
      expirer_.DeleteURL(*i, base::Time::Max());
  }
}

void HistoryBackend::DatabaseErrorCallback(int error, sql::Statement* stmt) {
  if (!scheduled_kill_db_ && sql::IsErrorCatastrophic(error)) {
    scheduled_kill_db_ = true;

    db_diagnostics_ = db_->GetDiagnosticInfo(error, stmt);

    // Don't just do the close/delete here, as we are being called by |db| and
    // that seems dangerous.
    // TODO(https://crbug.com/854258): It is also dangerous to kill the database
    // by a posted task: tasks that run before KillHistoryDatabase still can try
    // to use the broken database. Consider protecting against other tasks using
    // the DB or consider changing KillHistoryDatabase() to use RazeAndClose()
    // (then it can be cleared immediately).
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryBackend::KillHistoryDatabase, this));
  }
}

void HistoryBackend::KillHistoryDatabase() {
  scheduled_kill_db_ = false;
  if (!db_)
    return;

  // Notify SyncBridge about storage error. It will report failure to sync
  // engine and stop accepting remote updates.
  if (typed_url_sync_bridge_)
    typed_url_sync_bridge_->OnDatabaseError();

  // Rollback transaction because Raze() cannot be called from within a
  // transaction.
  db_->RollbackTransaction();
  bool success = db_->Raze();
  UMA_HISTOGRAM_BOOLEAN("History.KillHistoryDatabaseResult", success);

  // Release stashed embedder object before cleaning up the databases.
  supports_user_data_helper_.reset();

  // The expirer keeps tabs on the active databases. Tell it about the
  // databases which will be closed.
  expirer_.SetDatabases(nullptr, nullptr);

  // Reopen a new transaction for |db_| for the sake of CloseAllDatabases().
  db_->BeginTransaction();
  CloseAllDatabases();
}

base::SupportsUserData::Data* HistoryBackend::GetUserData(
    const void* key) const {
  DCHECK(supports_user_data_helper_);
  return supports_user_data_helper_->GetUserData(key);
}

void HistoryBackend::SetUserData(
    const void* key,
    std::unique_ptr<base::SupportsUserData::Data> data) {
  DCHECK(supports_user_data_helper_);
  supports_user_data_helper_->SetUserData(key, std::move(data));
}

void HistoryBackend::ProcessDBTask(
    std::unique_ptr<HistoryDBTask> task,
    scoped_refptr<base::SingleThreadTaskRunner> origin_loop,
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled) {
  TRACE_EVENT0("browser", "HistoryBackend::ProcessDBTask");
  bool scheduled = !queued_history_db_tasks_.empty();
  queued_history_db_tasks_.push_back(std::make_unique<QueuedHistoryDBTask>(
      std::move(task), origin_loop, is_canceled));
  if (!scheduled)
    ProcessDBTaskImpl();
}

void HistoryBackend::NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                                           const GURL& icon_url) {
  delegate_->NotifyFaviconsChanged(page_urls, icon_url);
}

void HistoryBackend::NotifyURLVisited(ui::PageTransition transition,
                                      const URLRow& row,
                                      const RedirectList& redirects,
                                      base::Time visit_time) {
  for (HistoryBackendObserver& observer : observers_)
    observer.OnURLVisited(this, transition, row, redirects, visit_time);

  delegate_->NotifyURLVisited(transition, row, redirects, visit_time);
}

void HistoryBackend::NotifyURLsModified(const URLRows& changed_urls,
                                        bool is_from_expiration) {
  for (HistoryBackendObserver& observer : observers_)
    observer.OnURLsModified(this, changed_urls, is_from_expiration);

  delegate_->NotifyURLsModified(changed_urls);
}

void HistoryBackend::NotifyURLsDeleted(DeletionInfo deletion_info) {
  std::set<GURL> origins;
  for (const history::URLRow& row : deletion_info.deleted_rows())
    origins.insert(row.url().GetOrigin());

  deletion_info.set_deleted_urls_origin_map(
      GetCountsAndLastVisitForOrigins(origins));

  for (HistoryBackendObserver& observer : observers_) {
    observer.OnURLsDeleted(
        this, deletion_info.IsAllHistory(), deletion_info.is_from_expiration(),
        deletion_info.deleted_rows(), deletion_info.favicon_urls());
  }

  delegate_->NotifyURLsDeleted(std::move(deletion_info));
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
  if (!ClearAllThumbnailHistory(starred_urls)) {
    LOG(ERROR) << "Thumbnail history could not be cleared";
    // We continue in this error case. If the user wants to delete their
    // history, we should delete as much as we can.
  }

  // ClearAllMainHistory will change the IDs of the URLs in kept_urls.
  // Therefore, we clear the list afterwards to make sure nobody uses this
  // invalid data.
  if (!ClearAllMainHistory(kept_url_rows))
    LOG(ERROR) << "Main history could not be cleared";
  kept_url_rows.clear();

  db_->GetStartDate(&first_recorded_time_);

  // Send out the notification that history is cleared. The in-memory database
  // will pick this up and clear itself.
  NotifyURLsDeleted(DeletionInfo::ForAllHistory());
}

bool HistoryBackend::ClearAllThumbnailHistory(
    const std::vector<GURL>& kept_urls) {
  if (!thumbnail_db_) {
    // When we have no reference to the thumbnail database, maybe there was an
    // error opening it. In this case, we just try to blow it away to try to
    // fix the error if it exists. This may fail, in which case either the
    // file doesn't exist or there's no more we can do.
    sql::Database::Delete(GetFaviconsFileName());
    return true;
  }

  // Isolate from any long-running transaction.
  thumbnail_db_->CommitTransaction();
  thumbnail_db_->BeginTransaction();

  // TODO(shess): If this fails, perhaps the database should be razed
  // or deleted.
  if (!thumbnail_db_->RetainDataForPageUrls(kept_urls)) {
    thumbnail_db_->RollbackTransaction();
    thumbnail_db_->BeginTransaction();
    return false;
  }

#if defined(OS_ANDROID)
  // TODO(michaelbai): Add the unit test once AndroidProviderBackend is
  // available in HistoryBackend.
  db_->ClearAndroidURLRows();
#endif

  // Vacuum to remove all the pages associated with the dropped tables. There
  // must be no transaction open on the table when we do this. We assume that
  // our long-running transaction is open, so we complete it and start it again.
  DCHECK_EQ(thumbnail_db_->transaction_nesting(), 1);
  thumbnail_db_->CommitTransaction();
  thumbnail_db_->Vacuum();
  thumbnail_db_->BeginTransaction();
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
  for (auto i = kept_urls.begin(); i != kept_urls.end(); ++i) {
    db_->AddTemporaryURL(*i);
  }

  // Replace the original URL table with the temporary one.
  if (!db_->CommitTemporaryURLTable())
    return false;

  // Delete the old tables and recreate them empty.
  db_->RecreateAllTablesButURL();

  // Vacuum to reclaim the space from the dropped tables. This must be done
  // when there is no transaction open, and we assume that our long-running
  // transaction is currently open.
  db_->CommitTransaction();
  db_->Vacuum();
  db_->BeginTransaction();
  db_->GetStartDate(&first_recorded_time_);

  return true;
}

}  // namespace history
