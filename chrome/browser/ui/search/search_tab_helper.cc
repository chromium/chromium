// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/search/omnibox_utils.h"
#endif

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
  if (!instant_service || !contents) {
    return false;
  }

  return instant_service->IsInstantProcess(
      contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID());
}

void RecordConcreteNtp(content::NavigationHandle* navigation_handle) {
  NewTabPageConcretePage concrete_page = NewTabPageConcretePage::kOther;
  if (navigation_handle->GetURL().DeprecatedGetOriginAsURL() ==
      chrome::ChromeUINewTabPageURLAsGURL().DeprecatedGetOriginAsURL()) {
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
                 chrome::ChromeUINewTabURLAsGURL().DeprecatedGetOriginAsURL()) {
    concrete_page = NewTabPageConcretePage::kOffTheRecordNtp;
  }
  base::UmaHistogramEnumeration("NewTabPage.ConcretePage", concrete_page);
}

}  // namespace

DEFINE_USER_DATA(SearchTabHelper);

SearchTabHelper::SearchTabHelper(tabs::TabInterface& tab,
                                 content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      ipc_router_(web_contents,
                  this,
                  std::make_unique<SearchIPCRouterPolicyImpl>(web_contents)),
      instant_service_(nullptr),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  DCHECK(search::IsInstantExtendedAPIEnabled());

  instant_service_ = InstantServiceFactory::GetForProfile(profile());
  if (instant_service_) {
    instant_service_->AddObserver(this);
  }

#if !BUILDFLAG(IS_ANDROID)
  OmniboxTabHelper::CreateForWebContents(web_contents, profile());
  OmniboxTabHelper::FromWebContents(web_contents)->AddObserver(this);
#endif

  tab_subscriptions_.push_back(tab.RegisterDidActivate(
      base::BindRepeating([](SearchTabHelper* self,
                             tabs::TabInterface*) { self->OnTabActivated(); },
                          base::Unretained(this))));
  tab_subscriptions_.push_back(tab.RegisterWillDeactivate(
      base::BindRepeating([](SearchTabHelper* self,
                             tabs::TabInterface*) { self->OnTabDeactivated(); },
                          base::Unretained(this))));
  if (tab.IsActivated()) {
    OnTabActivated();
  }
}

SearchTabHelper::~SearchTabHelper() {
  if (instant_service_) {
    instant_service_->RemoveObserver(this);
  }
#if !BUILDFLAG(IS_ANDROID)
  if (web_contents()) {
    if (auto* helper = OmniboxTabHelper::FromWebContents(web_contents())) {
      helper->RemoveObserver(this);
    }
  }
#endif
}

// static
SearchTabHelper* SearchTabHelper::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

// static
SearchTabHelper* SearchTabHelper::FromWebContents(
    content::WebContents* web_contents) {
  return web_contents
             ? From(tabs::TabInterface::MaybeGetFromContents(web_contents))
             : nullptr;
}

void SearchTabHelper::BindEmbeddedSearchConnecter(
    mojo::PendingAssociatedReceiver<search::mojom::EmbeddedSearchConnector>
        receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return;
  }
  auto* tab_helper = SearchTabHelper::FromWebContents(web_contents);
  if (!tab_helper) {
    return;
  }
  tab_helper->ipc_router_.BindEmbeddedSearchConnecter(std::move(receiver), rfh);
}

void SearchTabHelper::OnTabActivated() {
  ipc_router_.OnTabActivated();

  if (search::IsInstantNTP(web_contents()) && instant_service_) {
    instant_service_->OnNewTabPageOpened();
  }
}

void SearchTabHelper::OnTabDeactivated() {
  ipc_router_.OnTabDeactivated();
}

void SearchTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (navigation_handle->IsSameDocument()) {
    return;
  }

  if (web_contents()->GetVisibleURL().DeprecatedGetOriginAsURL() ==
      chrome::ChromeUINewTabURLAsGURL().DeprecatedGetOriginAsURL()) {
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
}

void SearchTabHelper::TitleWasSet(content::NavigationEntry* entry) {
  if (is_setting_title_ || !entry) {
    return;
  }

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

void SearchTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!load_details.is_main_frame) {
    return;
  }

  if (search::IsInstantNTP(web_contents())) {
    ipc_router_.SetInputInProgress(IsInputInProgress());
  }

  if (InInstantProcess(instant_service_, web_contents())) {
    ipc_router_.OnNavigationEntryCommitted();
  }
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
#if !BUILDFLAG(IS_ANDROID)
  search::FocusOmnibox(focus, web_contents());
#endif
}

void SearchTabHelper::OnDeleteMostVisitedItem(const GURL& url) {
  DCHECK(!url.is_empty());
  if (instant_service_) {
    instant_service_->DeleteMostVisitedItem(url);
  }
}

void SearchTabHelper::OnUndoMostVisitedDeletion(const GURL& url) {
  DCHECK(!url.is_empty());
  if (instant_service_) {
    instant_service_->UndoMostVisitedDeletion(url);
  }
}

void SearchTabHelper::OnUndoAllMostVisitedDeletions() {
  if (instant_service_) {
    instant_service_->UndoAllMostVisitedDeletions();
  }
}

#if !BUILDFLAG(IS_ANDROID)
void SearchTabHelper::OnOmniboxInputStateChanged() {
  ipc_router_.SetInputInProgress(IsInputInProgress());
}

void SearchTabHelper::OnOmniboxFocusChanged(OmniboxFocusState state,
                                            OmniboxFocusChangeReason reason) {
  ipc_router_.OmniboxFocusChanged(state, reason);

  // Don't send oninputstart/oninputend updates in response to focus changes
  // if there's a navigation in progress. This prevents Chrome from sending
  // a spurious oninputend when the user accepts a match in the omnibox.
  if (web_contents()->GetController().GetPendingEntry() == nullptr) {
    ipc_router_.SetInputInProgress(IsInputInProgress());
  }
}
#endif

Profile* SearchTabHelper::profile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

bool SearchTabHelper::IsInputInProgress() const {
#if !BUILDFLAG(IS_ANDROID)
  return search::IsOmniboxInputInProgress(web_contents());
#else
  return false;
#endif
}
