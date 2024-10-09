// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NewTabPageConcretePage {
  kOther = 0,
  k1PWebUiNtp = 1,
  k3PWebUiNtp = 2,
  k3PRemoteNtp = 3,
  kExtensionNtp = 4,
  kOffTheRecordNtp = 5,
  kMaxValue = kOffTheRecordNtp,
};

// Returns true if |contents| are rendered inside an Instant process.
bool InInstantProcess(const InstantService* instant_service,
                      content::WebContents* contents) {
  if (!instant_service || !contents)
    return false;

  return instant_service->IsInstantProcess(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID());
}

// Called when an NTP finishes loading. If the load start time was noted,
// calculates and logs the total load time.
void RecordNewTabLoadTime(content::WebContents* contents) {
  CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(contents);
  // CoreTabHelper can be null in unittests.
  if (!core_tab_helper)
    return;
  if (core_tab_helper->new_tab_start_time().is_null())
    return;

  core_tab_helper->set_new_tab_start_time(base::TimeTicks());
}

void RecordConcreteNtp(content::NavigationHandle* navigation_handle) {
  NewTabPageConcretePage concrete_page = NewTabPageConcretePage::kOther;
  if (navigation_handle->GetURL().DeprecatedGetOriginAsURL() ==
      GURL(chrome::kChromeUINewTabPageURL).DeprecatedGetOriginAsURL()) {
    concrete_page = NewTabPageConcretePage::k1PWebUiNtp;
  } else if (navigation_handle->GetURL().DeprecatedGetOriginAsURL() ==
             GURL(chrome::kChromeUINewTabPageThirdPartyURL)
                 .DeprecatedGetOriginAsURL()) {
    concrete_page = NewTabPageConcretePage::k3PWebUiNtp;
  } else if (search::IsInstantNTP(navigation_handle->GetWebContents())) {
    concrete_page = NewTabPageConcretePage::k3PRemoteNtp;
  } else if (navigation_handle->GetURL().SchemeIs(
                 extensions::kExtensionScheme)) {
    concrete_page = NewTabPageConcretePage::kExtensionNtp;
  } else if (Profile::FromBrowserContext(
                 navigation_handle->GetWebContents()->GetBrowserContext())
                 ->IsOffTheRecord() &&
             navigation_handle->GetURL().DeprecatedGetOriginAsURL() ==
                 GURL(chrome::kChromeUINewTabURL).DeprecatedGetOriginAsURL()) {
    concrete_page = NewTabPageConcretePage::kOffTheRecordNtp;
  }
  base::UmaHistogramEnumeration("NewTabPage.ConcretePage", concrete_page);
}

}  // namespace

SearchTabHelper::SearchTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<SearchTabHelper>(*web_contents),
      ipc_router_(web_contents,
                  this,
                  std::make_unique<SearchIPCRouterPolicyImpl>(web_contents)),
      instant_service_(nullptr) {
  DCHECK(search::IsInstantExtendedAPIEnabled());

  instant_service_ = InstantServiceFactory::GetForProfile(profile());
  if (instant_service_)
    instant_service_->AddObserver(this);

  OmniboxTabHelper::CreateForWebContents(web_contents);
  OmniboxTabHelper::FromWebContents(web_contents)->AddObserver(this);
}

SearchTabHelper::~SearchTabHelper() {
  if (instant_service_)
    instant_service_->RemoveObserver(this);
  if (auto* helper = OmniboxTabHelper::FromWebContents(&GetWebContents()))
    helper->RemoveObserver(this);
}

void SearchTabHelper::BindEmbeddedSearchConnecter(
    mojo::PendingAssociatedReceiver<search::mojom::EmbeddedSearchConnector>
        receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* tab_helper = SearchTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;
  tab_helper->ipc_router_.BindEmbeddedSearchConnecter(std::move(receiver), rfh);
}

void SearchTabHelper::OnTabActivated() {
  ipc_router_.OnTabActivated();

  if (search::IsInstantNTP(web_contents()) && instant_service_)
    instant_service_->OnNewTabPageOpened();

  CloseNTPCustomizeChromeFeaturePromo();
}

void SearchTabHelper::OnTabDeactivated() {
  ipc_router_.OnTabDeactivated();
}

void SearchTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  if (navigation_handle->IsSameDocument())
    return;

  if (web_contents()->GetVisibleURL().DeprecatedGetOriginAsURL() ==
      GURL(chrome::kChromeUINewTabURL).DeprecatedGetOriginAsURL()) {
    RecordConcreteNtp(navigation_handle);
  }

  if (search::IsNTPOrRelatedURL(navigation_handle->GetURL(), profile())) {
    // Set the title on any pending entry corresponding to the NTP. This
    // prevents any flickering of the tab title.
    content::NavigationEntry* entry =
        web_contents()->GetController().GetPendingEntry();
    if (entry) {
      web_contents()->UpdateTitleForEntry(
          entry, l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
    }
  }

  CloseNTPCustomizeChromeFeaturePromo();
}

void SearchTabHelper::TitleWasSet(content::NavigationEntry* entry) {
  if (is_setting_title_ || !entry)
    return;

  // Always set the title on the new tab page to be the one from our UI
  // resources. This check ensures that the title is properly set to the string
  // defined by the Chrome UI language (rather than the server language) in all
  // cases.
  //
  // We only override the title when it's nonempty to allow the page to set the
  // title if it really wants. An empty title means to use the default. There's
  // also a race condition between this code and the page's SetTitle call which
  // this rule avoids.
  if (entry->GetTitle().empty() &&
      search::NavEntryIsInstantNTP(web_contents(), entry)) {
    is_setting_title_ = true;
    web_contents()->UpdateTitleForEntry(
        entry, l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
    is_setting_title_ = false;
  }
}

void SearchTabHelper::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                    const GURL& /* validated_url */) {
  if (render_frame_host->IsInPrimaryMainFrame() &&
      search::IsInstantNTP(web_contents())) {
    RecordNewTabLoadTime(web_contents());
  }
}

void SearchTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!load_details.is_main_frame)
    return;

  if (search::IsInstantNTP(web_contents()))
    ipc_router_.SetInputInProgress(IsInputInProgress());

  if (InInstantProcess(instant_service_, web_contents()))
    ipc_router_.OnNavigationEntryCommitted();
}

void SearchTabHelper::NtpThemeChanged(NtpTheme theme) {
  // Populate theme colors for this tab.
  const auto& color_provider = web_contents()->GetColorProvider();
  theme.background_color = color_provider.GetColor(kColorNewTabPageBackground);
  theme.text_color = color_provider.GetColor(kColorNewTabPageText);
  theme.text_color_light = color_provider.GetColor(kColorNewTabPageTextLight);

  ipc_router_.SendNtpTheme(theme);
}

void SearchTabHelper::MostVisitedInfoChanged(
    const InstantMostVisitedInfo& most_visited_info) {
  ipc_router_.SendMostVisitedInfo(most_visited_info);
}

void SearchTabHelper::FocusOmnibox(bool focus) {
  search::FocusOmnibox(focus, web_contents());
}

void SearchTabHelper::OnDeleteMostVisitedItem(const GURL& url) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    instant_service_->DeleteMostVisitedItem(url);
}

void SearchTabHelper::OnUndoMostVisitedDeletion(const GURL& url) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    instant_service_->UndoMostVisitedDeletion(url);
}

void SearchTabHelper::OnUndoAllMostVisitedDeletions() {
  if (instant_service_)
    instant_service_->UndoAllMostVisitedDeletions();
}

void SearchTabHelper::OnOmniboxInputStateChanged() {
  ipc_router_.SetInputInProgress(IsInputInProgress());
}

void SearchTabHelper::OnOmniboxFocusChanged(OmniboxFocusState state,
                                            OmniboxFocusChangeReason reason) {
  ipc_router_.OmniboxFocusChanged(state, reason);

  // Don't send oninputstart/oninputend updates in response to focus changes
  // if there's a navigation in progress. This prevents Chrome from sending
  // a spurious oninputend when the user accepts a match in the omnibox.
  if (web_contents()->GetController().GetPendingEntry() == nullptr)
    ipc_router_.SetInputInProgress(IsInputInProgress());
}

Profile* SearchTabHelper::profile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

bool SearchTabHelper::IsInputInProgress() const {
  return search::IsOmniboxInputInProgress(web_contents());
}

void SearchTabHelper::CloseNTPCustomizeChromeFeaturePromo() {
  const base::Feature& customize_chrome_feature =
      feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature;
  if (web_contents()->GetController().GetVisibleEntry()->GetURL() ==
      GURL(chrome::kChromeUINewTabPageURL)) {
    return;
  }
  auto* const tab = tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (!tab || !tab->IsInForeground()) {
    return;
  }
  if (auto* const interface =
          BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
              web_contents())) {
    interface->AbortFeaturePromo(customize_chrome_feature);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchTabHelper);
