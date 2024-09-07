// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"

#include <cmath>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/blocked_content/popup_opener_tab_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/framebust_intervention/framebust_blocked_delegate_android.h"
#else
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

namespace {

void LogTabUnderAttempt(content::NavigationHandle* handle) {
  // The source id should generally be set, except for very rare circumstances
  // where the popup opener tab helper is not observing at the time the
  // previous navigation commit.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::SourceId opener_source_id =
      handle->GetWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  if (opener_source_id != ukm::kInvalidSourceId && ukm_recorder) {
    ukm::builders::AbusiveExperienceHeuristic_TabUnder(opener_source_id)
        .SetDidTabUnder(true)
        .Record(ukm_recorder);
  }
}

}  // namespace

BASE_FEATURE(kBlockTabUnders,
             "BlockTabUnders",
             base::FEATURE_DISABLED_BY_DEFAULT);

// static
std::unique_ptr<content::NavigationThrottle>
TabUnderNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  // TODO(crbug.com/40187173): TabUnderNavigationThrottle doesn't block
  // prerendering activations. However, currently prerender is same-origin only
  // so a prerendered activation could never be classified as a tab-under.
  // Otherwise, it should be safe to avoid creating a throttle in non primary
  // pages because prerendered pages should not be able to open popups. A
  // tab-under could therefore never occur within the non-primary page.
  if (handle->IsInPrimaryMainFrame())
    return base::WrapUnique(new TabUnderNavigationThrottle(handle));
  return nullptr;
}

TabUnderNavigationThrottle::~TabUnderNavigationThrottle() = default;

TabUnderNavigationThrottle::TabUnderNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle),
      block_(base::FeatureList::IsEnabled(kBlockTabUnders)),
      has_opened_popup_since_last_user_gesture_at_start_(
          HasOpenedPopupSinceLastUserGesture()),
      started_in_foreground_(handle->GetWebContents()->GetVisibility() ==
                             content::Visibility::VISIBLE) {}

bool TabUnderNavigationThrottle::IsSuspiciousClientRedirect() const {
  // This throttle is only created for primary main frame navigations. See
  // MaybeCreate().
  DCHECK(navigation_handle()->IsInPrimaryMainFrame());
  DCHECK(!navigation_handle()->HasCommitted());

  // Some browser initiated navigations have HasUserGesture set to false. This
  // should eventually be fixed in crbug.com/617904. In the meantime, just dont
  // block browser initiated ones.
  if (started_in_foreground_ || navigation_handle()->HasUserGesture() ||
      !navigation_handle()->IsRendererInitiated()) {
    return false;
  }

  // An empty previous URL indicates this was the first load. We filter these
  // out because we're primarily interested in sites which navigate themselves
  // away while in the background.
  content::WebContents* contents = navigation_handle()->GetWebContents();
  const GURL& previous_main_frame_url = contents->GetLastCommittedURL();
  if (previous_main_frame_url.is_empty())
    return false;

  // Same-site navigations are exempt from tab-under protection.
  const GURL& target_url = navigation_handle()->GetURL();
  if (net::registry_controlled_domains::SameDomainOrHost(
          previous_main_frame_url, target_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return false;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Exempt navigating to or from extension URLs, as they will redirect pages in
  // the background. By exempting in both directions, extensions can always
  // round-trip a page through an extension URL in order to perform arbitrary
  // redirections with content scripts.
  if (target_url.SchemeIs(extensions::kExtensionScheme) ||
      previous_main_frame_url.SchemeIs(extensions::kExtensionScheme)) {
    return false;
  }
#endif
  return true;
}

content::NavigationThrottle::ThrottleCheckResult
TabUnderNavigationThrottle::MaybeBlockNavigation() {
  if (seen_tab_under_ || !has_opened_popup_since_last_user_gesture_at_start_ ||
      !IsSuspiciousClientRedirect()) {
    return content::NavigationThrottle::PROCEED;
  }

  seen_tab_under_ = true;
  content::WebContents* contents = navigation_handle()->GetWebContents();

  LogTabUnderAttempt(navigation_handle());

  if (block_ && !TabUndersAllowedBySettings()) {
    const std::string error =
        base::StringPrintf(kBlockTabUnderFormatMessage,
                           navigation_handle()->GetURL().spec().c_str());
    contents->GetPrimaryMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError, error.c_str());
    ShowUI();
    return content::NavigationThrottle::CANCEL;
  }
  return content::NavigationThrottle::PROCEED;
}

void TabUnderNavigationThrottle::ShowUI() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  const GURL& url = navigation_handle()->GetURL();
#if BUILDFLAG(IS_ANDROID)
  blocked_content::FramebustBlockedMessageDelegate::CreateForWebContents(
      web_contents);
  blocked_content::FramebustBlockedMessageDelegate*
      framebust_blocked_message_delegate =
          blocked_content::FramebustBlockedMessageDelegate::FromWebContents(
              web_contents);
  framebust_blocked_message_delegate->ShowMessage(
      url,
      HostContentSettingsMapFactory::GetForProfile(
          web_contents->GetBrowserContext()),
      base::NullCallback());
#else
  if (auto* tab_helper =
          FramebustBlockTabHelper::FromWebContents(web_contents)) {
    tab_helper->AddBlockedUrl(url, base::NullCallback());
  }
#endif
}

bool TabUnderNavigationThrottle::HasOpenedPopupSinceLastUserGesture() const {
  content::WebContents* contents = navigation_handle()->GetWebContents();
  auto* popup_opener =
      blocked_content::PopupOpenerTabHelper::FromWebContents(contents);
  return popup_opener &&
         popup_opener->has_opened_popup_since_last_user_gesture();
}

bool TabUnderNavigationThrottle::TabUndersAllowedBySettings() const {
  content::WebContents* contents = navigation_handle()->GetWebContents();
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(contents->GetBrowserContext()));
  DCHECK(settings_map);
  return settings_map->GetContentSetting(contents->GetLastCommittedURL(),
                                         GURL(), ContentSettingsType::POPUPS) ==
         CONTENT_SETTING_ALLOW;
}

content::NavigationThrottle::ThrottleCheckResult
TabUnderNavigationThrottle::WillStartRequest() {
  return MaybeBlockNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
TabUnderNavigationThrottle::WillRedirectRequest() {
  return MaybeBlockNavigation();
}

const char* TabUnderNavigationThrottle::GetNameForLogging() {
  return "TabUnderNavigationThrottle";
}
