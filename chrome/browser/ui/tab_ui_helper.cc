// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_ui_helper.h"

#include <optional>

#include "base/byte_size.h"
#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/process/kill.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/performance_controls/memory_saver_utils.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#endif

namespace {

bool IsNTP(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         (url.GetHost() == chrome::kChromeUINewTabHost ||
#if !BUILDFLAG(IS_ANDROID)
          url.GetHost() == chrome::kChromeUITabSearchHost ||
#endif  // !BUILDFLAG(IS_ANDROID)
          url.GetHost() == chrome::kChromeUINewTabPageHost);
}

#if !BUILDFLAG(IS_ANDROID)
web_app::WebAppBrowserController* GetWebAppBrowserController(
    tabs::TabInterface* tab_interface) {
  // The browser window interface can be null during unit tests.
  BrowserWindowInterface* const browser_window_interface =
      tab_interface->GetBrowserWindowInterface();
  return browser_window_interface
             ? web_app::WebAppBrowserController::From(browser_window_interface)
             : nullptr;
}

bool ShouldShowAppIcon(web_app::WebAppBrowserController* app_controller,
                       tabs::TabInterface* tab_interface) {
  if (!app_controller) {
    return false;
  }

  BrowserWindowInterface* const browser_window_interface =
      tab_interface->GetBrowserWindowInterface();
  CHECK(browser_window_interface);
  const int index = browser_window_interface->GetTabStripModel()->GetIndexOfTab(
      tab_interface);
  return app_controller->ShouldShowAppIconOnTab(index);
}
#endif  // !BUILDFLAG(IS_ANDROID)
}  // namespace

DEFINE_USER_DATA(TabUIHelper);

TabUIHelper::TabUIHelper(tabs::TabInterface& tab_interface)
    : ContentsObservingTabFeature(tab_interface),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
  // Register for tab pin state change because pin state affects whether the
  // favicon should show or not.
  pin_tab_subscription_ = tab().RegisterPinnedStateChanged(base::BindRepeating(
      &TabUIHelper::OnTabPinnedStatusChange, base::Unretained(this)));
}

TabUIHelper::~TabUIHelper() = default;

// static
const TabUIHelper* TabUIHelper::From(const tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

// static
TabUIHelper* TabUIHelper::From(tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

base::CallbackListSubscription TabUIHelper::AddTabUIChangeCallback(
    base::RepeatingClosure callback) {
  return tab_ui_change_callbacks_.Add(std::move(callback));
}

std::u16string TabUIHelper::GetTitle() const {
  const tab_groups::SavedTabGroupWebContentsListener* wc_listener =
      tab().GetTabFeatures()->saved_tab_group_web_contents_listener();
  if (wc_listener) {
    if (const std::optional<tab_groups::DeferredTabState>& deferred_tab_state =
            wc_listener->deferred_tab_state()) {
      return deferred_tab_state.value().title();
    }
  }

  const std::u16string& contents_title = web_contents()->GetTitle();
  if (!contents_title.empty()) {
    return contents_title;
  }

#if BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED);
#else
  return std::u16string();
#endif
}

bool TabUIHelper::ShouldRenderLoadingTitle() {
  return GetTitle().empty() &&
         !GetVisibleURL().SchemeIs(content::kChromeUIUntrustedScheme);
}

bool TabUIHelper::ShouldThemifyFavicon() {
  content::NavigationEntry* const entry =
      tab().GetContents()->GetController().GetLastCommittedEntry();
  return entry && favicon::ShouldThemifyFaviconForEntry(entry);
}

#if !BUILDFLAG(IS_ANDROID)
bool TabUIHelper::ShouldDisplayFavicon() {
  // BrowserWindowInterface can be null during unit tests
  BrowserWindowInterface* const browser_window_interface =
      tab().GetBrowserWindowInterface();
  if (browser_window_interface) {
    // Remove for all tabbed web apps.
    web_app::AppBrowserController* const app_browser_controller =
        web_app::AppBrowserController::From(browser_window_interface);
    if (app_browser_controller && app_browser_controller->has_tab_strip()) {
      return false;
    }
  }

  if (tab().IsPinned()) {
    return true;
  }

  // Don't show favicon when on an interstitial.
  security_interstitials::SecurityInterstitialTabHelper* const
      security_interstitial_tab_helper = security_interstitials::
          SecurityInterstitialTabHelper::FromWebContents(tab().GetContents());
  if (security_interstitial_tab_helper &&
      security_interstitial_tab_helper->IsDisplayingInterstitial()) {
    return false;
  }

  // Otherwise, always display the favicon.
  return true;
}

bool TabUIHelper::IsMonochromeFavicon() {
  web_app::WebAppBrowserController* const web_app_browser_controller =
      GetWebAppBrowserController(&tab());
  return ShouldShowAppIcon(web_app_browser_controller, &tab()) &&
         !web_app_browser_controller->GetHomeTabIcon().isNull();
}
#endif

ui::ImageModel TabUIHelper::GetFavicon() {
  const tab_groups::SavedTabGroupWebContentsListener* wc_listener =
      tab().GetTabFeatures()->saved_tab_group_web_contents_listener();
  if (wc_listener) {
    if (const std::optional<tab_groups::DeferredTabState>& deferred_tab_state =
            wc_listener->deferred_tab_state()) {
      return deferred_tab_state.value().favicon();
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  web_app::WebAppBrowserController* const web_app_browser_controller =
      GetWebAppBrowserController(&tab());
  if (ShouldShowAppIcon(web_app_browser_controller, &tab())) {
    const gfx::ImageSkia home_tab_icon =
        web_app_browser_controller->GetHomeTabIcon();
    if (!home_tab_icon.isNull()) {
      return ui::ImageModel::FromImageSkia(home_tab_icon);
    } else {
      gfx::ImageSkia fallback_home_icon =
          web_app_browser_controller->GetFallbackHomeTabIcon();
      if (!fallback_home_icon.isNull()) {
        return ui::ImageModel::FromImageSkia(fallback_home_icon);
      }
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return ui::ImageModel::FromImage(
      favicon::TabFaviconFromWebContents(web_contents()));
}

bool TabUIHelper::ShouldHideThrobber() const {
  // We want to hide a background tab's throbber during page load if it is
  // created by session restore. A restored tab's favicon is already fetched
  // by |SessionRestoreDelegate|.
  return created_by_session_restore_ && !was_active_at_least_once_;
}

void TabUIHelper::SetWasActiveAtLeastOnce() {
  const bool was_hiding_throbber = ShouldHideThrobber();
  was_active_at_least_once_ = true;
  if (was_hiding_throbber != ShouldHideThrobber()) {
    tab_ui_change_callbacks_.Notify();
  }
}

bool TabUIHelper::IsCrashed() {
  const base::TerminationStatus crashed_status =
      web_contents()->GetCrashedStatus();
  return (crashed_status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ||
#if BUILDFLAG(IS_CHROMEOS)
          crashed_status ==
              base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM ||
#endif
          crashed_status == base::TERMINATION_STATUS_PROCESS_CRASHED ||
          crashed_status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
          crashed_status == base::TERMINATION_STATUS_LAUNCH_FAILED);
}

bool TabUIHelper::ShouldDisplayURL() {
  content::WebContents* const web_contents = tab().GetContents();
  // If the tab is showing a lookalike interstitial ("Did you mean example.com"
  // on éxample.com), don't show the URL in the hover card because it's
  // misleading.
  security_interstitials::SecurityInterstitialTabHelper*
      security_interstitial_tab_helper = security_interstitials::
          SecurityInterstitialTabHelper::FromWebContents(web_contents);
  // NTP URLs are hidden to match the omnibox behavior.
  return !IsNTP(web_contents->GetVisibleURL()) &&
         (!security_interstitial_tab_helper ||
          !security_interstitial_tab_helper->IsDisplayingInterstitial() ||
          security_interstitial_tab_helper->ShouldDisplayURL());
}

GURL TabUIHelper::GetVisibleURL() {
  content::WebContents* const contents = tab().GetContents();
  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  const bool missing_navigation_entry = !entry || entry->IsInitialEntry();
  // In the case of reverted uncommitted navigations, there might not be a valid
  // NavigationEntry. In that case, show about:blank to match the omnibox.
  return missing_navigation_entry ? GURL(url::kAboutBlankURL)
                                  : contents->GetVisibleURL();
}

void TabUIHelper::TitleWasSet(content::NavigationEntry* entry) {
  tab_ui_change_callbacks_.Notify();
}

void TabUIHelper::DidStopLoading() {
  // Reset the properties after the initial navigation finishes loading, so that
  // latter navigations are not affected. Note that the prerendered page won't
  // reset the properties because DidStopLoading is not called for prerendering.
  created_by_session_restore_ = false;
}

void TabUIHelper::OnVisibilityChanged(content::Visibility visiblity) {
  if (base::FeatureList::IsEnabled(
          tabs::kSessionRestoreShowThrobberOnVisible) &&
      visiblity == content::Visibility::VISIBLE) {
    SetWasActiveAtLeastOnce();
  }
}

void TabUIHelper::WasDiscarded() {
  // Notify observers that the tab should update its UI to show discard status.
  if (ShouldShowDiscardStatus()) {
    tab_ui_change_callbacks_.Notify();
  }
}

void TabUIHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Navigation committed so the visible URL might have changed.
  tab_ui_change_callbacks_.Notify();
}

#if !BUILDFLAG(IS_ANDROID)
void TabUIHelper::PrimaryPageChanged(content::Page& page) {
  if (tab().IsSplit()) {
    split_tabs::LogSplitViewUpdatedUKM(
        tab().GetBrowserWindowInterface()->GetTabStripModel(),
        tab().GetSplit().value());
  }
}
#endif

void TabUIHelper::SetNeedsAttention(bool needs_attention) {
  if (needs_attention == needs_attention_) {
    return;
  }

  needs_attention_ = needs_attention;
  tab_ui_change_callbacks_.Notify();
}

bool TabUIHelper::ShouldShowDiscardStatus() {
  content::WebContents* const web_contents = tab().GetContents();
  std::optional<mojom::LifecycleUnitDiscardReason> discard_reason =
      memory_saver::GetDiscardReason(web_contents);

  // Only show discard status for tabs that were proactively discarded or
  // suggested by the PerformanceDetectionManager to prevent confusion to users
  // on why a tab was discarded. Also, the favicon discard animation may use
  // resources so the animation should be limited to prevent performance issues.
  return memory_saver::IsURLSupported(web_contents->GetURL()) &&
         web_contents->WasDiscarded() && discard_reason.has_value() &&
         (discard_reason.value() ==
              mojom::LifecycleUnitDiscardReason::PROACTIVE ||
          discard_reason.value() ==
              mojom::LifecycleUnitDiscardReason::SUGGESTED);
}

std::optional<base::ByteSize> TabUIHelper::GetDiscardedMemorySavings() {
  content::WebContents* const web_contents = tab().GetContents();
  return web_contents->WasDiscarded()
             ? std::make_optional(
                   memory_saver::GetDiscardedMemorySavings(web_contents))
             : std::nullopt;
}

void TabUIHelper::OnTabPinnedStatusChange(tabs::TabInterface* tab_interface,
                                          bool new_pinned_state) {
  CHECK_EQ(&tab(), tab_interface);
  tab_ui_change_callbacks_.Notify();
}
