// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/top_sites_impl.h"

#include <stdint.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/hash/md5.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/history/core/browser/url_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

namespace history {
namespace {

void RunOrPostGetMostVisitedURLsCallback(
    base::TaskRunner* task_runner,
    const TopSitesImpl::GetMostVisitedURLsCallback& callback,
    const MostVisitedURLList& urls) {
  if (task_runner->RunsTasksInCurrentSequence())
    callback.Run(urls);
  else
    task_runner->PostTask(FROM_HERE, base::BindOnce(callback, urls));
}

// Checks if the titles stored in |old_list| and |new_list| have changes.
bool DoTitlesDiffer(const MostVisitedURLList& old_list,
                    const MostVisitedURLList& new_list) {
  // If the two lists have different sizes, the most visited titles are
  // considered to have changes.
  if (old_list.size() != new_list.size())
    return true;

  return !std::equal(std::begin(old_list), std::end(old_list),
                     std::begin(new_list),
                     [](const auto& old_item_ptr, const auto& new_item_ptr) {
                       return old_item_ptr.title == new_item_ptr.title;
                     });
}

// The delay for the first HistoryService query at startup.
constexpr base::TimeDelta kFirstDelayAtStartup =
    base::TimeDelta::FromSeconds(15);

// The delay for the all HistoryService queries other than the first one.
#if defined(OS_IOS) || defined(OS_ANDROID)
// On mobile, having the max at 60 minutes results in the topsites database
// being not updated often enough since the app isn't usually running for long
// stretches of time.
constexpr base::TimeDelta kDelayForUpdates = base::TimeDelta::FromMinutes(5);
#else
constexpr base::TimeDelta kDelayForUpdates = base::TimeDelta::FromMinutes(60);
#endif  // defined(OS_IOS) || defined(OS_ANDROID)

// Key for preference listing the URLs that should not be shown as most visited
// tiles.
const char kMostVisitedURLsBlacklist[] = "ntp.most_visited_blacklist";

}  // namespace

// Initially, histogram is not recorded.
bool TopSitesImpl::histogram_recorded_ = false;

TopSitesImpl::TopSitesImpl(PrefService* pref_service,
                           HistoryService* history_service,
                           const PrepopulatedPageList& prepopulated_pages,
                           const CanAddURLToHistoryFn& can_add_url_to_history)
    : backend_(nullptr),
      prepopulated_pages_(prepopulated_pages),
      pref_service_(pref_service),
      history_service_(history_service),
      can_add_url_to_history_(can_add_url_to_history),
      loaded_(false) {
  DCHECK(pref_service_);
  DCHECK(!can_add_url_to_history_.is_null());
}

void TopSitesImpl::Init(const base::FilePath& db_name) {
  // Create the backend here, rather than in the constructor, so unit tests that
  // do not need the backend can run without a problem.
  backend_ = new TopSitesBackend();
  backend_->Init(db_name);
  backend_->GetMostVisitedSites(
      base::BindOnce(&TopSitesImpl::OnGotMostVisitedURLs,
                     base::Unretained(this)),
      &cancelable_task_tracker_);
}

// WARNING: this function may be invoked on any thread.
void TopSitesImpl::GetMostVisitedURLs(
    const GetMostVisitedURLsCallback& callback) {
  MostVisitedURLList filtered_urls;
  {
    base::AutoLock lock(lock_);
    if (!loaded_) {
      // A request came in before we finished loading. Store the callback and
      // we'll run it on current thread when we finish loading.
      pending_callbacks_.push_back(base::Bind(
          &RunOrPostGetMostVisitedURLsCallback,
          base::RetainedRef(base::ThreadTaskRunnerHandle::Get()), callback));
      return;
    }
    filtered_urls = thread_safe_cache_;
  }
  callback.Run(filtered_urls);
}

static bool Contains(const MostVisitedURLList& urls, const GURL& url) {
  return std::find_if(urls.begin(), urls.end(),
                      [&url](const MostVisitedURL& item) {
                        return item.url == url;
                      }) != urls.end();
}

void TopSitesImpl::SyncWithHistory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (loaded_)
    StartQueryForMostVisited();
}

bool TopSitesImpl::HasBlacklistedItems() const {
  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(kMostVisitedURLsBlacklist);
  return blacklist && !blacklist->empty();
}

void TopSitesImpl::AddBlacklistedURL(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto dummy = std::make_unique<base::Value>();
  {
    DictionaryPrefUpdate update(pref_service_, kMostVisitedURLsBlacklist);
    base::DictionaryValue* blacklist = update.Get();
    blacklist->SetWithoutPathExpansion(GetURLHash(url), std::move(dummy));
  }

  ResetThreadSafeCache();
  NotifyTopSitesChanged(TopSitesObserver::ChangeReason::BLACKLIST);
}

void TopSitesImpl::RemoveBlacklistedURL(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    DictionaryPrefUpdate update(pref_service_, kMostVisitedURLsBlacklist);
    base::DictionaryValue* blacklist = update.Get();
    blacklist->RemoveWithoutPathExpansion(GetURLHash(url), nullptr);
  }
  ResetThreadSafeCache();
  NotifyTopSitesChanged(TopSitesObserver::ChangeReason::BLACKLIST);
}

bool TopSitesImpl::IsBlacklisted(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(kMostVisitedURLsBlacklist);
  return blacklist && blacklist->HasKey(GetURLHash(url));
}

void TopSitesImpl::ClearBlacklistedURLs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    DictionaryPrefUpdate update(pref_service_, kMostVisitedURLsBlacklist);
    base::DictionaryValue* blacklist = update.Get();
    blacklist->Clear();
  }
  ResetThreadSafeCache();
  NotifyTopSitesChanged(TopSitesObserver::ChangeReason::BLACKLIST);
}

bool TopSitesImpl::IsFull() {
  return loaded_ && top_sites_.size() >= kTopSitesNumber;
}

PrepopulatedPageList TopSitesImpl::GetPrepopulatedPages() {
  return prepopulated_pages_;
}

bool TopSitesImpl::loaded() const {
  return loaded_;
}

void TopSitesImpl::OnNavigationCommitted(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!loaded_)
    return;

  if (can_add_url_to_history_.Run(url))
    ScheduleUpdateTimer();
}

void TopSitesImpl::ShutdownOnUIThread() {
  history_service_ = nullptr;
  history_service_observer_.RemoveAll();
  // Cancel all requests so that the service doesn't callback to us after we've
  // invoked Shutdown (this could happen if we have a pending request and
  // Shutdown is invoked).
  cancelable_task_tracker_.TryCancelAll();
  if (backend_)
    backend_->Shutdown();
}

// static
void TopSitesImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kMostVisitedURLsBlacklist);
}

TopSitesImpl::~TopSitesImpl() = default;

void TopSitesImpl::StartQueryForMostVisited() {
  constexpr int kDaysOfHistory = 90;

  DCHECK(loaded_);
  timer_.Stop();

  if (!history_service_)
    return;

  history_service_->QueryMostVisitedURLs(
      num_results_to_request_from_history(), kDaysOfHistory,
      base::BindOnce(&TopSitesImpl::OnTopSitesAvailableFromHistory,
                     base::Unretained(this)),
      &cancelable_task_tracker_);
}

// static
void TopSitesImpl::DiffMostVisited(const MostVisitedURLList& old_list,
                                   const MostVisitedURLList& new_list,
                                   TopSitesDelta* delta) {
  // Add all the old URLs for quick lookup. This maps URLs to the corresponding
  // index in the input.
  std::map<GURL, size_t> all_old_urls;
  for (size_t i = 0; i < old_list.size(); i++)
    all_old_urls[old_list[i].url] = i;

  // Check all the URLs in the new set to see which ones are new or just moved.
  // When we find a match in the old set, we'll reset its index to our special
  // marker. This allows us to quickly identify the deleted ones in a later
  // pass.
  constexpr size_t kAlreadyFoundMarker = static_cast<size_t>(-1);
  int rank = -1;
  for (const auto& new_url : new_list) {
    rank++;
    auto found = all_old_urls.find(new_url.url);
    if (found == all_old_urls.end()) {
      delta->added.emplace_back(MostVisitedURLWithRank{new_url, rank});
    } else {
      DCHECK(found->second != kAlreadyFoundMarker)
          << "Same URL appears twice in the new list.";
      int old_rank = found->second;
      if (old_rank != rank)
        delta->moved.emplace_back(MostVisitedURLWithRank{new_url, rank});
      found->second = kAlreadyFoundMarker;
    }
  }

  // Any member without the special marker in the all_old_urls list means that
  // there wasn't a "new" URL that mapped to it, so it was deleted.
  for (const std::pair<GURL, size_t>& old_url : all_old_urls) {
    if (old_url.second != kAlreadyFoundMarker)
      delta->deleted.push_back(old_list[old_url.second]);
  }
}

bool TopSitesImpl::AddPrepopulatedPages(MostVisitedURLList* urls) const {
  bool added = false;
  for (const auto& prepopulated_page : prepopulated_pages_) {
    if (urls->size() >= kTopSitesNumber)
      break;
    if (!Contains(*urls, prepopulated_page.most_visited.url)) {
      urls->push_back(prepopulated_page.most_visited);
      added = true;
    }
  }
  return added;
}

MostVisitedURLList TopSitesImpl::ApplyBlacklist(
    const MostVisitedURLList& urls) {
  // Log the number of times ApplyBlacklist is called so we can compute the
  // average number of blacklisted items per user.
  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(kMostVisitedURLsBlacklist);
  UMA_HISTOGRAM_BOOLEAN("TopSites.NumberOfApplyBlacklist", true);
  UMA_HISTOGRAM_COUNTS_100("TopSites.NumberOfBlacklistedItems",
      (blacklist ? blacklist->size() : 0));
  MostVisitedURLList result;
  for (const auto& url : urls) {
    if (IsBlacklisted(url.url))
      continue;
    if (result.size() >= kTopSitesNumber)
      break;
    result.push_back(url);
  }
  return result;
}

// static
std::string TopSitesImpl::GetURLHash(const GURL& url) {
  // We don't use canonical URLs here to be able to blacklist only one of
  // the two 'duplicate' sites, e.g. 'gmail.com' and 'mail.google.com'.
  return base::MD5String(url.spec());
}

void TopSitesImpl::SetTopSites(MostVisitedURLList top_sites,
                               const CallLocation location) {
  DCHECK(thread_checker_.CalledOnValidThread());

  AddPrepopulatedPages(&top_sites);

  TopSitesDelta delta;
  DiffMostVisited(top_sites_, top_sites, &delta);

  TopSitesBackend::RecordHistogram record_or_not =
      TopSitesBackend::RECORD_HISTOGRAM_NO;

  // Record the delta size into a histogram if this function is called from
  // function OnGotMostVisitedURLs and no histogram value has been recorded
  // before.
  if (location == CALL_LOCATION_FROM_ON_GOT_MOST_VISITED_URLS &&
      !histogram_recorded_) {
    size_t delta_size =
        delta.deleted.size() + delta.added.size() + delta.moved.size();
    UMA_HISTOGRAM_COUNTS_100("History.FirstSetTopSitesDeltaSize", delta_size);
    // Will be passed to TopSitesBackend to let it record the histogram too.
    record_or_not = TopSitesBackend::RECORD_HISTOGRAM_YES;
    // Change it to true so that the histogram will not be recorded any more.
    histogram_recorded_ = true;
  }

  bool should_notify_observers = false;
  // If there is a change in urls, update the db and notify observers.
  if (!delta.deleted.empty() || !delta.added.empty() || !delta.moved.empty()) {
    backend_->UpdateTopSites(delta, record_or_not);
    should_notify_observers = true;
  }
  // If there is no url change in top sites, check if the titles have changes.
  // Notify observers if there's a change in titles.
  if (!should_notify_observers)
    should_notify_observers = DoTitlesDiffer(top_sites_, top_sites);

  // We always do the following steps (setting top sites in cache, and resetting
  // thread safe cache ...) as this method is invoked during startup at which
  // point the caches haven't been updated yet.
  top_sites_ = std::move(top_sites);

  ResetThreadSafeCache();

  if (should_notify_observers)
    NotifyTopSitesChanged(TopSitesObserver::ChangeReason::MOST_VISITED);
}

int TopSitesImpl::num_results_to_request_from_history() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(kMostVisitedURLsBlacklist);
  return kTopSitesNumber + (blacklist ? blacklist->size() : 0);
}

void TopSitesImpl::MoveStateToLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());

  MostVisitedURLList urls;
  PendingCallbacks pending_callbacks;
  {
    base::AutoLock lock(lock_);

    if (loaded_)
      return;  // Don't do anything if we're already loaded.
    loaded_ = true;

    // Now that we're loaded we can service the queued up callbacks. Copy them
    // here and service them outside the lock.
    if (!pending_callbacks_.empty()) {
      urls = thread_safe_cache_;
      pending_callbacks.swap(pending_callbacks_);
    }
  }

  for (auto& callback : pending_callbacks)
    callback.Run(urls);

  if (history_service_)
    history_service_observer_.Add(history_service_);

  NotifyTopSitesLoaded();
}

void TopSitesImpl::ResetThreadSafeCache() {
  base::AutoLock lock(lock_);
  thread_safe_cache_ = ApplyBlacklist(top_sites_);
}

void TopSitesImpl::ScheduleUpdateTimer() {
  if (timer_.IsRunning())
    return;

  timer_.Start(FROM_HERE, kDelayForUpdates, this,
               &TopSitesImpl::StartQueryForMostVisited);
}

void TopSitesImpl::OnGotMostVisitedURLs(MostVisitedURLList sites) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Set |top_sites_| directly so that SetTopSites() diffs correctly.
  top_sites_ = sites;
  SetTopSites(std::move(sites), CALL_LOCATION_FROM_ON_GOT_MOST_VISITED_URLS);

  MoveStateToLoaded();

  // Start a timer that refreshes top sites from history.
  timer_.Start(FROM_HERE, kFirstDelayAtStartup, this,
               &TopSitesImpl::StartQueryForMostVisited);
}

void TopSitesImpl::OnTopSitesAvailableFromHistory(MostVisitedURLList pages) {
  SetTopSites(std::move(pages), CALL_LOCATION_FROM_OTHER_PLACES);
}

void TopSitesImpl::OnURLsDeleted(HistoryService* history_service,
                                 const DeletionInfo& deletion_info) {
  if (!loaded_)
    return;

  if (deletion_info.IsAllHistory()) {
    SetTopSites(MostVisitedURLList(), CALL_LOCATION_FROM_OTHER_PLACES);
    backend_->ResetDatabase();
  }
  StartQueryForMostVisited();
}

}  // namespace history
