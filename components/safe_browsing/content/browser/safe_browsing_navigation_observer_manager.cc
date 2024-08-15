// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"

#include <iterator>
#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager_util.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "ui/base/clipboard/clipboard.h"

using content::WebContents;

namespace safe_browsing {

namespace {

// The expiration period of a user gesture. Any user gesture that happened 1.0
// second ago is considered as expired and not relevant to upcoming navigation
// events.
static constexpr base::TimeDelta kUserGestureTTL = base::Seconds(1);
// The expiration period of navigation events and resolved IP addresses. Any
// navigation related records that happened 2 minutes ago are considered as
// expired. So we clean up these navigation footprints every 2 minutes.
static constexpr base::TimeDelta kNavigationFootprintTTL = base::Minutes(2);
// The maximum number of latest NavigationEvent we keep. It is used to limit
// memory usage of navigation tracking. This number is picked based on UMA
// metric "SafeBrowsing.NavigationObserver.NavigationEventCleanUpCount".
// Lowering it could make room for abuse.
static const int kNavigationRecordMaxSize = 100;
// The maximum number of ReferrerChainEntry. It is used to limit the size of
// reports (e.g. ClientDownloadRequest) we send to SB server.
static const int kReferrerChainMaxLength = 10;

constexpr size_t kMaxNumberOfNavigationsToAppend = 5;

// Given when an event happened and its TTL, determine if it is already expired.
// Note, if for some reason this event's timestamp is in the future, this
// event's timestamp is invalid, hence we treat it as expired.
bool IsEventExpired(const base::Time& event_time, base::TimeDelta ttl) {
  base::Time now = base::Time::Now();
  return event_time > now || now - event_time > ttl;
}

// Helper function to determine if the URL type should be LANDING_REFERRER or
// LANDING_PAGE, and modify AttributionResult accordingly.
ReferrerChainEntry::URLType GetURLTypeAndAdjustAttributionResult(
    size_t user_gesture_count,
    SafeBrowsingNavigationObserverManager::AttributionResult* out_result) {
  // Landing page refers to the page user directly interacts with to trigger
  // this event (e.g. clicking on download button). Landing referrer page is the
  // one user interacts with right before navigating to the landing page.
  // Since we are tracing navigations backwards, if we've reached
  // user gesture limit before this navigation event, this is a navigation
  // leading to the landing referrer page, otherwise it leads to landing page.
  if (user_gesture_count == 0) {
    *out_result = SafeBrowsingNavigationObserverManager::SUCCESS;
    return ReferrerChainEntry::EVENT_URL;
  } else if (user_gesture_count == 2) {
    *out_result =
        SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_REFERRER;
    return ReferrerChainEntry::LANDING_REFERRER;
  } else if (user_gesture_count == 1) {
    *out_result = SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_PAGE;
    return ReferrerChainEntry::LANDING_PAGE;
  } else {
    *out_result = SafeBrowsingNavigationObserverManager::SUCCESS_REFERRER;
    return ReferrerChainEntry::REFERRER;
  }
}

std::string ShortOriginForReporting(const std::string& url) {
  GURL gurl(url);
  if (gurl.SchemeIsLocal()) {
    std::string sha_url = crypto::SHA256HashString(url);
    return gurl.scheme() + "://" + base::HexEncode(sha_url);
  }
  return gurl.DeprecatedGetOriginAsURL().spec();
}

}  // namespace

// -------------------------ReferrerChainData-----------------------

// String value of kDownloadReferrerChainDataKey is not used.
const char ReferrerChainData::kDownloadReferrerChainDataKey[] =
    "referrer_chain_data_key";

ReferrerChainData::ReferrerChainData(
    ReferrerChainProvider::AttributionResult attribution_result,
    std::unique_ptr<ReferrerChain> referrer_chain,
    size_t referrer_chain_length,
    size_t recent_navigations_to_collect)
    : attribution_result_(attribution_result),
      referrer_chain_(std::move(referrer_chain)),
      referrer_chain_length_(referrer_chain_length),
      recent_navigations_to_collect_(recent_navigations_to_collect) {}

ReferrerChainData::~ReferrerChainData() {}

ReferrerChain* ReferrerChainData::GetReferrerChain() {
  return referrer_chain_.get();
}

// -------------------------NavigationEventList---------------------
NavigationEventList::NavigationEventList(std::size_t size_limit)
    : size_limit_(size_limit) {
  DCHECK_GT(size_limit_, 0U);
}

NavigationEventList::~NavigationEventList() = default;

std::optional<size_t> NavigationEventList::FindNavigationEvent(
    const base::Time& last_event_timestamp,
    const GURL& target_url,
    const GURL& target_main_frame_url,
    SessionID target_tab_id,
    const content::GlobalRenderFrameHostId& outermost_main_frame_id,
    size_t start_index) {
  if (target_url.is_empty() && target_main_frame_url.is_empty())
    return std::nullopt;

  if (navigation_events_.size() == 0)
    return std::nullopt;

  // If target_url is empty, we should back trace navigation based on its
  // main frame URL instead.
  GURL search_url = target_url.is_empty() ? target_main_frame_url : target_url;

  for (int current_index = static_cast<int>(start_index); current_index >= 0;
       current_index--) {
    auto* nav_event = GetNavigationEvent(current_index);

    // The next event cannot come before the previous one.
    if (nav_event->last_updated > last_event_timestamp) {
      continue;
    }

    const bool valid_tab_id =
        !target_tab_id.is_valid() || nav_event->target_tab_id == target_tab_id;

    const bool potentially_valid_outermost_main_frame_id =
        !outermost_main_frame_id || !nav_event->outermost_main_frame_id ||
        nav_event->is_outermost_main_frame ||
        nav_event->outermost_main_frame_id == outermost_main_frame_id;

    const bool valid_outermost_main_frame_id =
        !outermost_main_frame_id ||
        nav_event->outermost_main_frame_id == outermost_main_frame_id;

    // If tab id is valid, we require it match the nav event (similar for frame
    // tree node id). In all cases, we require that the URLs match.
    if (nav_event->GetDestinationUrl() == search_url && valid_tab_id &&
        potentially_valid_outermost_main_frame_id) {
      size_t result_index = current_index;

      // If both source_url and source_main_frame_url are empty, we should check
      // if a retargeting navigation caused this navigation. In this case, we
      // skip this navigation event and looks for the retargeting navigation
      // event.
      if (nav_event->source_url.is_empty() &&
          nav_event->source_main_frame_url.is_empty()) {
        size_t retargeting_nav_event_index = FindRetargetingNavigationEvent(
            nav_event->last_updated, nav_event->target_tab_id, start_index);
        if (static_cast<int>(retargeting_nav_event_index) != -1) {
          // If there is a server redirection immediately after retargeting, we
          // need to adjust our search url to the original request.
          auto* retargeting_nav_event =
              GetNavigationEvent(retargeting_nav_event_index);
          // Adjust retargeting navigation event's attributes. The
          // retargeting_nav_event original request and redirects are
          // unreliable, since that navigation can be canceled.
          retargeting_nav_event->server_redirect_urls =
              nav_event->server_redirect_urls;
          retargeting_nav_event->original_request_url =
              nav_event->original_request_url;
          result_index = retargeting_nav_event_index;
        }
      }

      // We didn't have a strict main frame id match, so we will look for a
      // better match first.
      if (!valid_outermost_main_frame_id && current_index > 0) {
        auto alternate_index = FindNavigationEvent(
            last_event_timestamp, target_url, target_main_frame_url,
            target_tab_id, outermost_main_frame_id, current_index - 1);
        if (alternate_index) {
          auto* alternate_event = GetNavigationEvent(*alternate_index);
          // Found a strict match.
          if (alternate_event->outermost_main_frame_id ==
              outermost_main_frame_id) {
            result_index = *alternate_index;
          }
        }
      }

      return result_index;
    }
  }
  return std::nullopt;
}

NavigationEvent* NavigationEventList::FindPendingNavigationEvent(
    const GURL& target_url) {
  NavigationEvent* return_event = nullptr;
  for (auto& it : pending_navigation_events_) {
    // Returns the event that matches the target_url and also has the latest
    // updated timestamp.
    if (it.second->GetDestinationUrl() == target_url &&
        (!return_event ||
         return_event->last_updated < it.second.get()->last_updated)) {
      return_event = it.second.get();
    }
  }
  return return_event;
}

size_t NavigationEventList::FindRetargetingNavigationEvent(
    const base::Time& last_event_timestamp,
    SessionID target_tab_id,
    size_t start_index) {
  // Since navigation events are recorded in chronological order, we traverse
  // the vector in reverse order to get the latest match.
  for (int current_index = static_cast<int>(start_index); current_index >= 0;
       current_index--) {
    auto* nav_event = GetNavigationEvent(current_index);

    // The next event cannot come before the previous one.
    if (nav_event->last_updated > last_event_timestamp)
      continue;

    // In addition to url and tab_id checking, we need to compare the
    // source_tab_id and target_tab_id to make sure it is a retargeting event.
    if (nav_event->target_tab_id == target_tab_id &&
        nav_event->source_tab_id != nav_event->target_tab_id) {
      return current_index;
    }
  }
  return -1;
}

void NavigationEventList::RecordNavigationEvent(
    std::unique_ptr<NavigationEvent> nav_event,
    std::optional<CopyPasteEntry> last_copy_paste_entry) {
  // Skip page refresh and in-page navigation.
  if (nav_event->source_url == nav_event->GetDestinationUrl() &&
      nav_event->source_tab_id == nav_event->target_tab_id)
    return;

  // If the nav entry is without referrer, BROWSER_INITIATED
  // and there is a recent copy, guess this is a paste to
  // omnibox and modify nav event. This enables attribution
  // in referrer chain.
  if (nav_event->navigation_initiation ==
          ReferrerChainEntry::BROWSER_INITIATED &&
      nav_event->source_url.is_empty() &&
      nav_event->source_main_frame_url.is_empty() &&
      last_copy_paste_entry.has_value() &&
      nav_event->original_request_url ==
          last_copy_paste_entry.value().target_) {
    nav_event->navigation_initiation =
        ReferrerChainEntry::COPY_PASTE_USER_INITIATED;
    nav_event->source_url = last_copy_paste_entry.value().source_frame_url_;
    nav_event->source_main_frame_url =
        last_copy_paste_entry.value().source_main_frame_url_;
  }

  if (navigation_events_.size() == size_limit_)
    navigation_events_.pop_front();
  navigation_events_.push_back(std::move(nav_event));
}

void NavigationEventList::RecordPendingNavigationEvent(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<NavigationEvent> nav_event) {
  pending_navigation_events_[navigation_handle] = std::move(nav_event);
}

void NavigationEventList::AddRedirectUrlToPendingNavigationEvent(
    content::NavigationHandle* navigation_handle,
    const GURL& server_redirect_url) {
  if (!base::Contains(pending_navigation_events_, navigation_handle)) {
    return;
  }
  pending_navigation_events_[navigation_handle]->server_redirect_urls.push_back(
      SafeBrowsingNavigationObserverManager::ClearURLRef(server_redirect_url));
}

void NavigationEventList::RemovePendingNavigationEvent(
    content::NavigationHandle* navigation_handle) {
  if (base::Contains(pending_navigation_events_, navigation_handle)) {
    pending_navigation_events_.erase(navigation_handle);
  }
}

std::size_t NavigationEventList::CleanUpNavigationEvents() {
  // Remove any stale NavigationEnvent, if it is older than
  // `kNavigationFootprintTTL`.
  std::size_t removal_count = 0;
  while (!navigation_events_.empty() &&
         IsEventExpired(navigation_events_[0]->last_updated,
                        kNavigationFootprintTTL)) {
    navigation_events_.pop_front();
    removal_count++;
  }

  // Clean up expired pending navigation events.
  auto it = pending_navigation_events_.begin();
  while (it != pending_navigation_events_.end()) {
    if (IsEventExpired(it->second->last_updated, kNavigationFootprintTTL)) {
      it = pending_navigation_events_.erase(it);
    } else {
      ++it;
    }
  }

  return removal_count;
}

// -------------------------CopyPasteEntry---------------------
CopyPasteEntry::CopyPasteEntry(GURL target,
                               GURL source_frame_url,
                               GURL source_main_frame_url,
                               base::Time recorded_time)
    : target_(target),
      source_frame_url_(source_frame_url),
      source_main_frame_url_(source_main_frame_url),
      recorded_time_(recorded_time) {}

CopyPasteEntry::CopyPasteEntry(const CopyPasteEntry& other) = default;

// -----------------SafeBrowsingNavigationObserverManager-----------
// static
bool SafeBrowsingNavigationObserverManager::IsUserGestureExpired(
    const base::Time& timestamp) {
  return IsEventExpired(timestamp, kUserGestureTTL);
}

// static
GURL SafeBrowsingNavigationObserverManager::ClearURLRef(const GURL& url) {
  if (url.has_ref()) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    return url.ReplaceComponents(replacements);
  }
  return url;
}

// static
bool SafeBrowsingNavigationObserverManager::IsEnabledAndReady(
    PrefService* prefs,
    bool has_safe_browsing_service) {
  return IsSafeBrowsingEnabled(*prefs) && has_safe_browsing_service;
}

// static
void SafeBrowsingNavigationObserverManager::SanitizeReferrerChain(
    ReferrerChain* referrer_chain) {
  for (int i = 0; i < referrer_chain->size(); i++) {
    ReferrerChainEntry* entry = referrer_chain->Mutable(i);
    ReferrerChainEntry entry_copy(*entry);
    entry->Clear();
    if (entry_copy.has_url())
      entry->set_url(ShortOriginForReporting(entry_copy.url()));
    if (entry_copy.has_main_frame_url())
      entry->set_main_frame_url(
          ShortOriginForReporting(entry_copy.main_frame_url()));
    entry->set_type(entry_copy.type());
    for (int j = 0; j < entry_copy.ip_addresses_size(); j++)
      entry->add_ip_addresses(entry_copy.ip_addresses(j));
    if (entry_copy.has_referrer_url())
      entry->set_referrer_url(
          ShortOriginForReporting(entry_copy.referrer_url()));
    if (entry_copy.has_referrer_main_frame_url())
      entry->set_referrer_main_frame_url(
          ShortOriginForReporting(entry_copy.referrer_main_frame_url()));
    entry->set_is_retargeting(entry_copy.is_retargeting());
    entry->set_navigation_time_msec(entry_copy.navigation_time_msec());
    entry->set_navigation_initiation(entry_copy.navigation_initiation());
    for (int j = 0; j < entry_copy.server_redirect_chain_size(); j++) {
      ReferrerChainEntry::ServerRedirect* server_redirect_entry =
          entry->add_server_redirect_chain();
      if (entry_copy.server_redirect_chain(j).has_url()) {
        server_redirect_entry->set_url(
            ShortOriginForReporting(entry_copy.server_redirect_chain(j).url()));
      }
    }
  }
}

SafeBrowsingNavigationObserverManager::SafeBrowsingNavigationObserverManager(
    PrefService* pref_service,
    content::ServiceWorkerContext* context)
    : navigation_event_list_(kNavigationRecordMaxSize),
      pref_service_(pref_service),
      notification_context_(context) {
  ui::Clipboard::GetForCurrentThread()->AddObserver(this);
  notification_context_->AddObserver(this);
  // Schedule clean up in 2 minutes.
  ScheduleNextCleanUpAfterInterval(kNavigationFootprintTTL);
}

void SafeBrowsingNavigationObserverManager::RecordNavigationEvent(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<NavigationEvent> nav_event) {
  navigation_event_list_.RemovePendingNavigationEvent(navigation_handle);
  navigation_event_list_.RecordNavigationEvent(std::move(nav_event),
                                               last_copy_paste_entry_);
}

void SafeBrowsingNavigationObserverManager::RecordPendingNavigationEvent(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<NavigationEvent> nav_event) {
  navigation_event_list_.RecordPendingNavigationEvent(navigation_handle,
                                                      std::move(nav_event));
}

void SafeBrowsingNavigationObserverManager::
    AddRedirectUrlToPendingNavigationEvent(
        content::NavigationHandle* navigation_handle,
        const GURL& server_redirect_url) {
  navigation_event_list_.AddRedirectUrlToPendingNavigationEvent(
      navigation_handle, server_redirect_url);
}

void SafeBrowsingNavigationObserverManager::RecordUserGestureForWebContents(
    content::WebContents* web_contents) {
  const base::Time timestamp = base::Time::Now();
  auto insertion_result =
      user_gesture_map_.insert(std::make_pair(web_contents, timestamp));
  // Update the timestamp if entry already exists.
  if (!insertion_result.second)
    insertion_result.first->second = timestamp;
}

void SafeBrowsingNavigationObserverManager::OnUserGestureConsumed(
    content::WebContents* web_contents) {
  user_gesture_map_.erase(web_contents);
}

bool SafeBrowsingNavigationObserverManager::HasUserGesture(
    content::WebContents* web_contents) {
  if (!web_contents)
    return false;
  if (user_gesture_map_.find(web_contents) != user_gesture_map_.end())
    return true;
  return false;
}

bool SafeBrowsingNavigationObserverManager::HasUnexpiredUserGesture(
    content::WebContents* web_contents) {
  if (!web_contents)
    return false;
  auto it = user_gesture_map_.find(web_contents);
  if (it == user_gesture_map_.end())
    return false;
  return !IsUserGestureExpired(it->second);
}

void SafeBrowsingNavigationObserverManager::RecordHostToIpMapping(
    const std::string& host,
    const std::string& ip) {
  auto insert_result = host_to_ip_map_.insert(
      std::make_pair(host, std::vector<ResolvedIPAddress>()));
  if (!insert_result.second) {
    // host_to_ip_map already contains this key.
    // If this IP is already in the vector, we update its timestamp.
    for (auto& vector_entry : insert_result.first->second) {
      if (vector_entry.ip == ip) {
        vector_entry.timestamp = base::Time::Now();
        return;
      }
    }
  }
  // If this is a new IP of this host, and we added to the end of the vector.
  insert_result.first->second.push_back(
      ResolvedIPAddress(base::Time::Now(), ip));
}

void SafeBrowsingNavigationObserverManager::OnWebContentDestroyed(
    content::WebContents* web_contents) {
  user_gesture_map_.erase(web_contents);
}

void SafeBrowsingNavigationObserverManager::CleanUpStaleNavigationFootprints() {
  CleanUpNavigationEvents();
  CleanUpUserGestures();
  CleanUpIpAddresses();
  CleanUpCopyData();
  CleanUpNotificationNavigationEvents();
  ScheduleNextCleanUpAfterInterval(kNavigationFootprintTTL);
}

SafeBrowsingNavigationObserverManager::AttributionResult
SafeBrowsingNavigationObserverManager::IdentifyReferrerChainByEventURL(
    const GURL& event_url,
    SessionID event_tab_id,
    const content::GlobalRenderFrameHostId& outermost_main_frame_id,
    int user_gesture_count_limit,
    ReferrerChain* out_referrer_chain) {
  if (!event_url.is_valid())
    return INVALID_URL;

  auto nav_event_index = navigation_event_list_.FindNavigationEvent(
      base::Time::Now(), ClearURLRef(event_url), GURL(), event_tab_id,
      outermost_main_frame_id,
      navigation_event_list_.NavigationEventsSize() - 1);
  if (!nav_event_index) {
    // We cannot find a single navigation event related to this event.
    return NAVIGATION_EVENT_NOT_FOUND;
  }

  auto* nav_event = navigation_event_list_.GetNavigationEvent(*nav_event_index);
  AttributionResult result = SUCCESS;
  MaybeAddToReferrerChain(out_referrer_chain, nav_event, GURL(),
                          ReferrerChainEntry::EVENT_URL);
  int user_gesture_count = 0;
  GetRemainingReferrerChain(*nav_event_index, user_gesture_count,
                            user_gesture_count_limit, out_referrer_chain,
                            &result);
  MaybeRemoveNonUserGestureReferrerEntries(out_referrer_chain,
                                           kReferrerChainMaxLength);
  RemoveSafeBrowsingAllowlistDomains(out_referrer_chain);
  return result;
}

SafeBrowsingNavigationObserverManager::AttributionResult
SafeBrowsingNavigationObserverManager::IdentifyReferrerChainByEventURL(
    const GURL& event_url,
    SessionID event_tab_id,
    int user_gesture_count_limit,
    ReferrerChain* out_referrer_chain) {
  return IdentifyReferrerChainByEventURL(
      event_url, event_tab_id, content::GlobalRenderFrameHostId(),
      user_gesture_count_limit, out_referrer_chain);
}

SafeBrowsingNavigationObserverManager::AttributionResult
SafeBrowsingNavigationObserverManager::IdentifyReferrerChainByPendingEventURL(
    const GURL& event_url,
    int user_gesture_count_limit,
    ReferrerChain* out_referrer_chain) {
  if (!event_url.is_valid())
    return INVALID_URL;

  NavigationEvent* pending_nav_event =
      navigation_event_list_.FindPendingNavigationEvent(ClearURLRef(event_url));
  if (!pending_nav_event) {
    // We cannot find a single navigation event related to this event.
    return NAVIGATION_EVENT_NOT_FOUND;
  }

  AttributionResult result = SUCCESS;
  MaybeAddToReferrerChain(out_referrer_chain, pending_nav_event, GURL(),
                          ReferrerChainEntry::EVENT_URL);
  int user_gesture_count = 0;
  GetRemainingReferrerChainForNavEvent(
      pending_nav_event, navigation_event_list_.NavigationEventsSize(),
      user_gesture_count, user_gesture_count_limit, out_referrer_chain,
      &result);
  MaybeRemoveNonUserGestureReferrerEntries(out_referrer_chain,
                                           kReferrerChainMaxLength);
  RemoveSafeBrowsingAllowlistDomains(out_referrer_chain);
  return result;
}

SafeBrowsingNavigationObserverManager::AttributionResult
SafeBrowsingNavigationObserverManager::IdentifyReferrerChainByRenderFrameHost(
    content::RenderFrameHost* render_frame_host,
    int user_gesture_count_limit,
    ReferrerChain* out_referrer_chain) {
  if (!render_frame_host)
    return INVALID_URL;
  GURL last_committed_url = render_frame_host->GetLastCommittedURL();
  if (!last_committed_url.is_valid())
    return INVALID_URL;
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  bool has_user_gesture = HasUserGesture(web_contents);
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  return IdentifyReferrerChainByHostingPage(
      ClearURLRef(last_committed_url), GURL(),
      render_frame_host->GetOutermostMainFrame()->GetGlobalId(), tab_id,
      has_user_gesture, user_gesture_count_limit, out_referrer_chain);
}

SafeBrowsingNavigationObserverManager::AttributionResult
SafeBrowsingNavigationObserverManager::IdentifyReferrerChainByHostingPage(
    const GURL& initiating_frame_url,
    const GURL& initiating_main_frame_url,
    const content::GlobalRenderFrameHostId& initiating_outermost_main_frame_id,
    SessionID tab_id,
    bool has_user_gesture,
    int user_gesture_count_limit,
    ReferrerChain* out_referrer_chain) {
  if (!initiating_frame_url.is_valid())
    return INVALID_URL;

  auto* rfh =
      content::RenderFrameHost::FromID(initiating_outermost_main_frame_id);
  DCHECK(!initiating_outermost_main_frame_id ||
         (rfh && rfh->GetOutermostMainFrame() == rfh));

  auto nav_event_index = navigation_event_list_.FindNavigationEvent(
      base::Time::Now(), ClearURLRef(initiating_frame_url),
      ClearURLRef(initiating_main_frame_url), tab_id,
      initiating_outermost_main_frame_id,
      navigation_event_list_.NavigationEventsSize() - 1);
  if (!nav_event_index) {
    // We cannot find a single navigation event related to this hosting page.
    return NAVIGATION_EVENT_NOT_FOUND;
  }

  auto* nav_event = navigation_event_list_.GetNavigationEvent(*nav_event_index);

  AttributionResult result = SUCCESS;

  int user_gesture_count = 0;
  // If this initiating_frame has user gesture, we consider this as the landing
  // page of this event.
  if (has_user_gesture) {
    user_gesture_count = 1;
    MaybeAddToReferrerChain(
        out_referrer_chain, nav_event, initiating_main_frame_url,
        GetURLTypeAndAdjustAttributionResult(user_gesture_count, &result));
  } else {
    MaybeAddToReferrerChain(out_referrer_chain, nav_event,
                            initiating_main_frame_url,
                            ReferrerChainEntry::CLIENT_REDIRECT);
  }

  GetRemainingReferrerChain(*nav_event_index, user_gesture_count,
                            user_gesture_count_limit, out_referrer_chain,
                            &result);

  MaybeRemoveNonUserGestureReferrerEntries(out_referrer_chain,
                                           kReferrerChainMaxLength);
  RemoveSafeBrowsingAllowlistDomains(out_referrer_chain);
  return result;
}

SafeBrowsingNavigationObserverManager::
    ~SafeBrowsingNavigationObserverManager() {
  ui::Clipboard::GetForCurrentThread()->RemoveObserver(this);
  notification_context_->RemoveObserver(this);
}

void SafeBrowsingNavigationObserverManager::RecordNewWebContents(
    content::WebContents* source_web_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& target_url,
    ui::PageTransition page_transition,
    content::WebContents* target_web_contents,
    bool renderer_initiated) {
  DCHECK(source_web_contents);
  DCHECK(target_web_contents);

  // Remove the "#" at the end of URL, since it does not point to any actual
  // page fragment ID.
  GURL cleaned_target_url =
      SafeBrowsingNavigationObserverManager::ClearURLRef(target_url);

  std::unique_ptr<NavigationEvent> nav_event =
      std::make_unique<NavigationEvent>();
  if (source_render_frame_host) {
    nav_event->source_url = SafeBrowsingNavigationObserverManager::ClearURLRef(
        source_render_frame_host->GetLastCommittedURL());
    nav_event->source_main_frame_url =
        SafeBrowsingNavigationObserverManager::ClearURLRef(
            source_render_frame_host->GetOutermostMainFrame()
                ->GetLastCommittedURL());
  }

  if (source_render_frame_host) {
    nav_event->initiator_outermost_main_frame_id =
        source_render_frame_host->GetOutermostMainFrame()->GetGlobalId();
  }
  nav_event->outermost_main_frame_id =
      target_web_contents->GetPrimaryMainFrame()
          ->GetOutermostMainFrame()
          ->GetGlobalId();

  nav_event->source_tab_id =
      sessions::SessionTabHelper::IdForTab(source_web_contents);
  nav_event->original_request_url = cleaned_target_url;
  nav_event->target_tab_id =
      sessions::SessionTabHelper::IdForTab(target_web_contents);
  nav_event->maybe_launched_by_external_application =
      ui::PageTransitionCoreTypeIs(page_transition,
                                   ui::PAGE_TRANSITION_AUTO_TOPLEVEL);

  if (!renderer_initiated) {
    nav_event->navigation_initiation = ReferrerChainEntry::BROWSER_INITIATED;
  } else if (HasUnexpiredUserGesture(source_web_contents)) {
    OnUserGestureConsumed(source_web_contents);
    nav_event->navigation_initiation =
        ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  } else {
    nav_event->navigation_initiation =
        ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE;
  }

  navigation_event_list_.RecordNavigationEvent(std::move(nav_event),
                                               last_copy_paste_entry_);
}

// static
size_t SafeBrowsingNavigationObserverManager::CountOfRecentNavigationsToAppend(
    content::BrowserContext* browser_context,
    PrefService* prefs,
    AttributionResult result) {
  if (!IsExtendedReportingEnabled(*prefs) ||
      browser_context->IsOffTheRecord() || result == SUCCESS_LANDING_REFERRER) {
    return 0u;
  }
  return kMaxNumberOfNavigationsToAppend;
}

void SafeBrowsingNavigationObserverManager::AppendRecentNavigations(
    size_t recent_navigation_count,
    ReferrerChain* out_referrer_chain) {
  if (recent_navigation_count == 0u) {
    return;
  }
  int current_referrer_chain_size = out_referrer_chain->size();
  double last_navigation_time_msec =
      current_referrer_chain_size == 0
          ? base::Time::Now().InMillisecondsSinceUnixEpoch()
          : out_referrer_chain->Get(current_referrer_chain_size - 1)
                .navigation_time_msec();
  auto it = navigation_event_list_.navigation_events().rbegin();
  ReferrerChain navigation_chain;
  size_t user_gesture_cnt = 0;
  while (it != navigation_event_list_.navigation_events().rend()) {
    // Skip navigations that happened after |last_navigation_time_msec|.
    if (it->get()->last_updated.InMillisecondsSinceUnixEpoch() <
        last_navigation_time_msec) {
      MaybeAddToReferrerChain(&navigation_chain, it->get(), GURL(),
                              ReferrerChainEntry::RECENT_NAVIGATION);
      if (it->get()->IsUserInitiated()) {
        user_gesture_cnt++;
      }
      // If the number of user gesture entries has reached the upper bound, stop
      // adding new entries, since we can only omit non user gesture entries.
      if (user_gesture_cnt >= recent_navigation_count) {
        break;
      }
    }
    it++;
  }

  MaybeRemoveNonUserGestureReferrerEntries(&navigation_chain,
                                           recent_navigation_count);

  // Add the navigation entries into the referrer chain.
  for (ReferrerChainEntry entry : navigation_chain) {
    out_referrer_chain->Add()->Swap(&entry);
  }
  RemoveSafeBrowsingAllowlistDomains(out_referrer_chain);
}

void SafeBrowsingNavigationObserverManager::OnCopyURL(
    const GURL& url,
    const GURL& source_frame_url,
    const GURL& source_main_frame_url) {
  if (base::FeatureList::IsEnabled(
          kSafeBrowsingReferrerChainWithCopyPasteNavigation)) {
    last_copy_paste_entry_.emplace(url, source_frame_url, source_main_frame_url,
                                   base::Time::Now());
  }
}

void SafeBrowsingNavigationObserverManager::CleanUpNavigationEvents() {
  navigation_event_list_.CleanUpNavigationEvents();
}

void SafeBrowsingNavigationObserverManager::CleanUpUserGestures() {
  for (auto it = user_gesture_map_.begin(); it != user_gesture_map_.end();) {
    if (IsEventExpired(it->second, kNavigationFootprintTTL)) {
      it = user_gesture_map_.erase(it);
    } else {
      ++it;
    }
  }
}

void SafeBrowsingNavigationObserverManager::CleanUpIpAddresses() {
  for (auto it = host_to_ip_map_.begin(); it != host_to_ip_map_.end();) {
    std::erase_if(it->second, [](const ResolvedIPAddress& resolved_ip) {
      return IsEventExpired(resolved_ip.timestamp, kNavigationFootprintTTL);
    });
    if (it->second.empty())
      it = host_to_ip_map_.erase(it);
    else
      ++it;
  }
}

void SafeBrowsingNavigationObserverManager::CleanUpCopyData() {
  if (last_copy_paste_entry_.has_value()) {
    if (IsEventExpired(last_copy_paste_entry_.value().recorded_time_,
                       kNavigationFootprintTTL)) {
      last_copy_paste_entry_ = std::nullopt;
    }
  }
}

void SafeBrowsingNavigationObserverManager::
    CleanUpNotificationNavigationEvents() {
  auto it = notification_navigation_events_.begin();
  while (it != notification_navigation_events_.end()) {
    if (IsEventExpired(it->second->last_updated, kNavigationFootprintTTL)) {
      it = notification_navigation_events_.erase(it);
    } else {
      ++it;
    }
  }
}

bool SafeBrowsingNavigationObserverManager::IsCleanUpScheduled() const {
  return cleanup_timer_.IsRunning();
}

void SafeBrowsingNavigationObserverManager::ScheduleNextCleanUpAfterInterval(
    base::TimeDelta interval) {
  DCHECK_GT(interval, base::TimeDelta());
  cleanup_timer_.Stop();
  cleanup_timer_.Start(
      FROM_HERE, interval, this,
      &SafeBrowsingNavigationObserverManager::CleanUpStaleNavigationFootprints);
}

void SafeBrowsingNavigationObserverManager::OnClientNavigated(
    const GURL& script_url,
    const GURL& url) {
  RecordNotificationNavigationEvent(script_url, url);
}

void SafeBrowsingNavigationObserverManager::OnWindowOpened(
    const GURL& script_url,
    const GURL& url) {
  RecordNotificationNavigationEvent(script_url, url);
}

void SafeBrowsingNavigationObserverManager::RecordNotificationNavigationEvent(
    const GURL& script_url,
    const GURL& url) {
  // Push notifications are tied to the https scheme.
  // We also care about notifications from Chrome extensions.
  if (!script_url.SchemeIs(url::kHttpsScheme) &&
      !script_url.SchemeIs("chrome-extension")) {
    return;
  }
  // We only collect notification referrers for ESB users.
  if (!IsEnhancedProtectionEnabled(*pref_service_)) {
    return;
  }
  auto nav_event = std::make_unique<NavigationEvent>();
  nav_event->source_url =
      SafeBrowsingNavigationObserverManager::ClearURLRef(script_url);
  notification_navigation_events_
      [SafeBrowsingNavigationObserverManager::ClearURLRef(url)] =
          std::move(nav_event);
  UMA_HISTOGRAM_BOOLEAN(
      "SafeBrowsing.NavigationObserver.NotificationNavigationEventAdded", true);
}

void SafeBrowsingNavigationObserverManager::MaybeAddToReferrerChain(
    ReferrerChain* referrer_chain,
    NavigationEvent* nav_event,
    const GURL& destination_main_frame_url,
    ReferrerChainEntry::URLType type) {
  // For privacy reasons, we don't actually add the referrer chain events if
  // they are too old, no matter what `kReferrerChainEventMaximumAgeSeconds` is
  // set to. By bailing out early here, we still trace the referrer chain and
  // see if the older events improve the quality of the referrer chain.
  if (IsEventExpired(nav_event->last_updated, kNavigationFootprintTTL)) {
    return;
  }

  std::unique_ptr<ReferrerChainEntry> referrer_chain_entry =
      std::make_unique<ReferrerChainEntry>();
  referrer_chain_entry->set_navigation_initiation(
      nav_event->navigation_initiation);
  const GURL destination_url = nav_event->GetDestinationUrl();
  referrer_chain_entry->set_url(ShortURLForReporting(destination_url));
  if (destination_main_frame_url.is_valid() &&
      destination_url != destination_main_frame_url)
    referrer_chain_entry->set_main_frame_url(
        ShortURLForReporting(destination_main_frame_url));
  referrer_chain_entry->set_type(type);
  auto ip_it = host_to_ip_map_.find(destination_url.host());
  if (ip_it != host_to_ip_map_.end()) {
    for (const ResolvedIPAddress& entry : ip_it->second) {
      referrer_chain_entry->add_ip_addresses(entry.ip);
    }
  }
  // Since we only track navigation to landing referrer, we will not log the
  // referrer of the landing referrer page.
  if (type != ReferrerChainEntry::LANDING_REFERRER) {
    referrer_chain_entry->set_referrer_url(
        ShortURLForReporting(nav_event->source_url));
    // Only set |referrer_main_frame_url| if it is diff from |referrer_url|.
    if (nav_event->source_main_frame_url.is_valid() &&
        nav_event->source_url != nav_event->source_main_frame_url) {
      referrer_chain_entry->set_referrer_main_frame_url(
          ShortURLForReporting(nav_event->source_main_frame_url));
    }
  }
  // Update the referrer entry if we got here from a Push notification.
  if (base::Contains(notification_navigation_events_,
                     nav_event->original_request_url) &&
      (nav_event->navigation_initiation ==
           ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE ||
       nav_event->navigation_initiation ==
           ReferrerChainEntry::BROWSER_INITIATED) &&
      nav_event->source_url.is_empty()) {
    // The navigation event and the Push notification click should have happened
    // close in time to each other.
    if ((nav_event->last_updated -
         notification_navigation_events_[nav_event->original_request_url]
             ->last_updated) < base::Seconds(5)) {
      referrer_chain_entry->set_referrer_url(ShortURLForReporting(
          notification_navigation_events_[nav_event->original_request_url]
              ->source_url));
      referrer_chain_entry->set_navigation_initiation(
          ReferrerChainEntry::NOTIFICATION_INITIATED);
      UMA_HISTOGRAM_BOOLEAN(
          "SafeBrowsing.NavigationObserver."
          "NotificationOriginAddedToReferrerChain",
          true);
    }
  }
  referrer_chain_entry->set_is_retargeting(nav_event->source_tab_id !=
                                           nav_event->target_tab_id);
  referrer_chain_entry->set_navigation_time_msec(
      nav_event->last_updated.InMillisecondsSinceUnixEpoch());
  if (!nav_event->server_redirect_urls.empty()) {
    // The first entry in |server_redirect_chain| should be the original request
    // url.
    ReferrerChainEntry::ServerRedirect* server_redirect =
        referrer_chain_entry->add_server_redirect_chain();
    server_redirect->set_url(
        ShortURLForReporting(nav_event->original_request_url));
    for (const GURL& redirect : nav_event->server_redirect_urls) {
      server_redirect = referrer_chain_entry->add_server_redirect_chain();
      server_redirect->set_url(ShortURLForReporting(redirect));
    }
  }
  referrer_chain_entry->set_maybe_launched_by_external_application(
      nav_event->maybe_launched_by_external_application);
  referrer_chain->Add()->Swap(referrer_chain_entry.get());
}

void SafeBrowsingNavigationObserverManager::GetRemainingReferrerChain(
    size_t last_nav_event_traced_index,
    int current_user_gesture_count,
    int user_gesture_count_limit,
    ReferrerChain* out_referrer_chain,
    SafeBrowsingNavigationObserverManager::AttributionResult* out_result) {
  auto* last_nav_event_traced =
      navigation_event_list_.GetNavigationEvent(last_nav_event_traced_index);

  GetRemainingReferrerChainForNavEvent(
      last_nav_event_traced, last_nav_event_traced_index,
      current_user_gesture_count, user_gesture_count_limit, out_referrer_chain,
      out_result);
}

void SafeBrowsingNavigationObserverManager::
    GetRemainingReferrerChainForNavEvent(
        NavigationEvent* last_nav_event_traced,
        size_t last_nav_event_traced_index,
        int current_user_gesture_count,
        int user_gesture_count_limit,
        ReferrerChain* out_referrer_chain,
        SafeBrowsingNavigationObserverManager::AttributionResult* out_result) {
  GURL last_main_frame_url_traced(last_nav_event_traced->source_main_frame_url);
  while (current_user_gesture_count < user_gesture_count_limit) {
    // Back trace to the next nav_event that was initiated by the user.
    while (!last_nav_event_traced->IsUserInitiated()) {
      auto nav_event_index = navigation_event_list_.FindNavigationEvent(
          last_nav_event_traced->last_updated,
          last_nav_event_traced->source_url,
          last_nav_event_traced->source_main_frame_url,
          last_nav_event_traced->source_tab_id,
          last_nav_event_traced->initiator_outermost_main_frame_id,
          last_nav_event_traced_index - 1);
      if (!nav_event_index)
        return;

      last_nav_event_traced_index = *nav_event_index;
      last_nav_event_traced = navigation_event_list_.GetNavigationEvent(
          last_nav_event_traced_index);
      MaybeAddToReferrerChain(out_referrer_chain, last_nav_event_traced,
                              last_main_frame_url_traced,
                              ReferrerChainEntry::CLIENT_REDIRECT);
      last_main_frame_url_traced = last_nav_event_traced->source_main_frame_url;
    }

    current_user_gesture_count++;

    auto nav_event_index = navigation_event_list_.FindNavigationEvent(
        last_nav_event_traced->last_updated, last_nav_event_traced->source_url,
        last_nav_event_traced->source_main_frame_url,
        last_nav_event_traced->source_tab_id,
        last_nav_event_traced->initiator_outermost_main_frame_id,
        last_nav_event_traced_index - 1);
    if (!nav_event_index)
      return;

    last_nav_event_traced_index = *nav_event_index;
    last_nav_event_traced =
        navigation_event_list_.GetNavigationEvent(last_nav_event_traced_index);
    MaybeAddToReferrerChain(out_referrer_chain, last_nav_event_traced,
                            last_main_frame_url_traced,
                            GetURLTypeAndAdjustAttributionResult(
                                current_user_gesture_count, out_result));
    last_main_frame_url_traced = last_nav_event_traced->source_main_frame_url;
  }
}

void SafeBrowsingNavigationObserverManager::RemoveSafeBrowsingAllowlistDomains(
    ReferrerChain* out_referrer_chain) {
  bool is_url_removed_by_policy = false;
  for (ReferrerChainEntry& entry : *out_referrer_chain) {
    // entry can be empty if it is removed in
    // MaybeRemoveNonUserGestureReferrerEntries.
    if (!entry.has_url()) {
      continue;
    }
    if (IsURLAllowlistedByPolicy(GURL(entry.url()), *pref_service_)) {
      entry.clear_url();
      is_url_removed_by_policy = true;
    }
    if (IsURLAllowlistedByPolicy(GURL(entry.main_frame_url()),
                                 *pref_service_)) {
      entry.clear_main_frame_url();
      is_url_removed_by_policy = true;
    }
    if (IsURLAllowlistedByPolicy(GURL(entry.referrer_url()), *pref_service_)) {
      entry.clear_referrer_url();
      is_url_removed_by_policy = true;
    }
    if (IsURLAllowlistedByPolicy(GURL(entry.referrer_main_frame_url()),
                                 *pref_service_)) {
      entry.clear_referrer_main_frame_url();
      is_url_removed_by_policy = true;
    }
    for (ReferrerChainEntry::ServerRedirect& server_redirect_entry :
         *entry.mutable_server_redirect_chain()) {
      if (IsURLAllowlistedByPolicy(GURL(server_redirect_entry.url()),
                                   *pref_service_)) {
        server_redirect_entry.clear_url();
        is_url_removed_by_policy = true;
      }
    }

    entry.set_is_url_removed_by_policy(is_url_removed_by_policy);
  }
}

}  // namespace safe_browsing
