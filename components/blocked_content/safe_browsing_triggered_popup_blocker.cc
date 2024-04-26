// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/blocked_content/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/content/browser/safe_browsing_page_activation_throttle.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"

namespace blocked_content {
namespace {

void LogAction(SafeBrowsingTriggeredPopupBlocker::Action action) {
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.Popups.StrongBlockerActions",
                            action,
                            SafeBrowsingTriggeredPopupBlocker::Action::kCount);
}

}  // namespace

using safe_browsing::SubresourceFilterLevel;

BASE_FEATURE(kAbusiveExperienceEnforce,
             "AbusiveExperienceEnforce",
             base::FEATURE_ENABLED_BY_DEFAULT);

SafeBrowsingTriggeredPopupBlocker::PageData::PageData(content::Page& page)
    : PageUserData(page) {}

SafeBrowsingTriggeredPopupBlocker::PageData::~PageData() = default;

SafeBrowsingTriggeredPopupBlocker::NavigationHandleData::NavigationHandleData(
    content::NavigationHandle&) {}
SafeBrowsingTriggeredPopupBlocker::NavigationHandleData::
    ~NavigationHandleData() = default;

// static
void SafeBrowsingTriggeredPopupBlocker::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kAbusiveExperienceInterventionEnforce,
                                true /* default_value */);
}

// static
void SafeBrowsingTriggeredPopupBlocker::MaybeCreate(
    content::WebContents* web_contents) {
  if (!IsEnabled(web_contents))
    return;

  auto* observer_manager =
      subresource_filter::SubresourceFilterObserverManager::FromWebContents(
          web_contents);
  if (!observer_manager)
    return;

  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(
      UserDataKey(), base::WrapUnique(new SafeBrowsingTriggeredPopupBlocker(
                         web_contents, observer_manager)));
}

SafeBrowsingTriggeredPopupBlocker::~SafeBrowsingTriggeredPopupBlocker() =
    default;

bool SafeBrowsingTriggeredPopupBlocker::ShouldApplyAbusivePopupBlocker(
    content::Page& page) {
  LogAction(Action::kConsidered);
  PageData& page_data = GetPageData(page);

  if (!page_data.is_triggered())
    return false;

  if (!IsEnabled(web_contents()))
    return false;

  LogAction(Action::kBlocked);
  page_data.inc_num_popups_blocked();
  page.GetMainDocument().AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, kAbusiveEnforceMessage);
  return true;
}

SafeBrowsingTriggeredPopupBlocker::SafeBrowsingTriggeredPopupBlocker(
    content::WebContents* web_contents,
    subresource_filter::SubresourceFilterObserverManager* observer_manager)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SafeBrowsingTriggeredPopupBlocker>(
          *web_contents) {
  DCHECK(observer_manager);
  scoped_observation_.Observe(observer_manager);
}

void SafeBrowsingTriggeredPopupBlocker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->GetNavigatingFrameType() ==
          content::FrameType::kFencedFrameRoot) {
    return;
  }

  std::optional<SubresourceFilterLevel> level;
  NavigationHandleData* data =
      NavigationHandleData::GetOrCreateForNavigationHandle(*navigation_handle);
  data->level_for_next_committed_navigation().swap(level);

  // Only care about main frame navigations that commit.
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (navigation_handle->IsErrorPage())
    return;

  // Log a warning only if we've matched a warn-only safe browsing list.
  if (level == SubresourceFilterLevel::ENFORCE) {
    GetPageData(navigation_handle->GetRenderFrameHost()->GetPage())
        .set_is_triggered(true);
    LogAction(Action::kEnforcedSite);
    // When a page is restored from back-forward cache, we don't get
    // OnSafeBrowsingChecksComplete callback, so |level| will always
    // be empty.
    // To work around this, we disable back-forward cache if the original
    // page load had abusive enforcement - this means that not doing checks on
    // back-forward navigation is fine as it's guaranteed that
    // the original page load didn't have enforcement.
    // Note that it's possible for the safe browsing list to update while
    // the page is in the cache, the risk of this is mininal due to
    // having a time limit for how long pages are allowed to be in the
    // cache.
    content::BackForwardCache::DisableForRenderFrameHost(
        navigation_handle->GetRenderFrameHost(),
        back_forward_cache::DisabledReason(
            back_forward_cache::DisabledReasonId::
                kSafeBrowsingTriggeredPopupBlocker));
  } else if (level == SubresourceFilterLevel::WARN) {
    navigation_handle->GetRenderFrameHost()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning, kAbusiveWarnMessage);
    LogAction(Action::kWarningSite);
  }
  LogAction(Action::kNavigation);
}

// This method will always be called before the DidFinishNavigation associated
// with this handle.
// The exception is a navigation restoring a page from back-forward cache --
// in that case don't issue any requests, therefore we don't get any
// safe browsing callbacks. See the comment above for the mitigation.
void SafeBrowsingTriggeredPopupBlocker::OnSafeBrowsingChecksComplete(
    content::NavigationHandle* navigation_handle,
    const subresource_filter::SubresourceFilterSafeBrowsingClient::CheckResult&
        result) {
  DCHECK(navigation_handle->IsInMainFrame());
  // TODO(crbug.com/40202987): Replace it with DCHECK.
  if (navigation_handle->GetNavigatingFrameType() ==
      content::FrameType::kFencedFrameRoot) {
    return;
  }
  std::optional<safe_browsing::SubresourceFilterLevel> match_level;
  if (result.threat_type ==
      safe_browsing::SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER) {
    auto abusive = result.threat_metadata.subresource_filter_match.find(
        safe_browsing::SubresourceFilterType::ABUSIVE);
    if (abusive != result.threat_metadata.subresource_filter_match.end())
      match_level = abusive->second;
  }

  if (match_level.has_value()) {
    NavigationHandleData* data =
        NavigationHandleData::GetOrCreateForNavigationHandle(
            *navigation_handle);
    data->level_for_next_committed_navigation() = match_level;
  }
}

void SafeBrowsingTriggeredPopupBlocker::OnSubresourceFilterGoingAway() {
  DCHECK(scoped_observation_.IsObserving());
  scoped_observation_.Reset();
}

bool SafeBrowsingTriggeredPopupBlocker::IsEnabled(
    content::WebContents* web_contents) {
  // If feature is disabled, return false. This is done so that if the feature
  // is broken it can be disabled irrespective of the policy.
  if (!base::FeatureList::IsEnabled(kAbusiveExperienceEnforce))
    return false;

  // If enterprise policy is not set, this will return true which is the default
  // preference value.
  return user_prefs::UserPrefs::Get(web_contents->GetBrowserContext())
      ->GetBoolean(prefs::kAbusiveExperienceInterventionEnforce);
}

SafeBrowsingTriggeredPopupBlocker::PageData&
SafeBrowsingTriggeredPopupBlocker::GetPageData(content::Page& page) {
  return *PageData::GetOrCreateForPage(page);
}

PAGE_USER_DATA_KEY_IMPL(SafeBrowsingTriggeredPopupBlocker::PageData);
NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(
    SafeBrowsingTriggeredPopupBlocker::NavigationHandleData);
WEB_CONTENTS_USER_DATA_KEY_IMPL(SafeBrowsingTriggeredPopupBlocker);

}  // namespace blocked_content
