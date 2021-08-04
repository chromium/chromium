// Copyright 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/search/chrome_colors/chrome_colors_factory.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search/search_suggest/search_suggest_service.h"
#include "chrome/browser/search/search_suggest/search_suggest_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/search/ntp_user_data_logger.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
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
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace {

bool IsCacheableNTP(content::WebContents* contents) {
  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  return search::NavEntryIsInstantNTP(contents, entry);
}

// Returns true if |contents| are rendered inside an Instant process.
bool InInstantProcess(const InstantService* instant_service,
                      content::WebContents* contents) {
  if (!instant_service || !contents)
    return false;

  return instant_service->IsInstantProcess(
      contents->GetMainFrame()->GetProcess()->GetID());
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

  base::TimeDelta duration =
      base::TimeTicks::Now() - core_tab_helper->new_tab_start_time();
  if (IsCacheableNTP(contents)) {
    if (google_util::IsGoogleDomainUrl(
            contents->GetController().GetLastCommittedEntry()->GetURL(),
            google_util::ALLOW_SUBDOMAIN,
            google_util::DISALLOW_NON_STANDARD_PORTS)) {
      UMA_HISTOGRAM_TIMES("Tab.NewTabOnload.Google", duration);
    } else {
      UMA_HISTOGRAM_TIMES("Tab.NewTabOnload.Other", duration);
    }
  } else {
    UMA_HISTOGRAM_TIMES("Tab.NewTabOnload.Local", duration);
  }
  core_tab_helper->set_new_tab_start_time(base::TimeTicks());
}

}  // namespace

SearchTabHelper::SearchTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      web_contents_(web_contents),
      ipc_router_(web_contents,
                  this,
                  std::make_unique<SearchIPCRouterPolicyImpl>(web_contents)),
      instant_service_(nullptr) {
  DCHECK(search::IsInstantExtendedAPIEnabled());

  instant_service_ = InstantServiceFactory::GetForProfile(profile());
  if (instant_service_)
    instant_service_->AddObserver(this);

  search_suggest_service_ =
      SearchSuggestServiceFactory::GetForProfile(profile());

  chrome_colors_service_ =
      chrome_colors::ChromeColorsFactory::GetForProfile(profile());

  OmniboxTabHelper::CreateForWebContents(web_contents);
  OmniboxTabHelper::FromWebContents(web_contents_)->AddObserver(this);
}

SearchTabHelper::~SearchTabHelper() {
  if (instant_service_)
    instant_service_->RemoveObserver(this);
  if (auto* helper = OmniboxTabHelper::FromWebContents(web_contents_))
    helper->RemoveObserver(this);
  if (select_file_dialog_)
    select_file_dialog_->ListenerDestroyed();
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

  if (search::IsInstantNTP(web_contents_) && instant_service_)
    instant_service_->OnNewTabPageOpened();
}

void SearchTabHelper::OnTabDeactivated() {
  ipc_router_.OnTabDeactivated();
}

void SearchTabHelper::OnTabClosing() {
  if (search::IsInstantNTP(web_contents_) && chrome_colors_service_)
    chrome_colors_service_->RevertThemeChangesForTab(web_contents_);
}

void SearchTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  if (navigation_handle->IsSameDocument())
    return;

  // When navigating away from NTP we should revert all the unconfirmed state.
  if (search::IsInstantNTP(web_contents_) && chrome_colors_service_)
    chrome_colors_service_->RevertThemeChangesForTab(web_contents_);

  if (search::IsNTPOrRelatedURL(navigation_handle->GetURL(), profile())) {
    // Set the title on any pending entry corresponding to the NTP. This
    // prevents any flickering of the tab title.
    content::NavigationEntry* entry =
        web_contents_->GetController().GetPendingEntry();
    if (entry) {
      web_contents_->UpdateTitleForEntry(
          entry, l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
    }
  }
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
      search::NavEntryIsInstantNTP(web_contents_, entry)) {
    is_setting_title_ = true;
    web_contents_->UpdateTitleForEntry(
        entry, l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
    is_setting_title_ = false;
  }
}

void SearchTabHelper::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                    const GURL& /* validated_url */) {
  if (!render_frame_host->GetParent() && search::IsInstantNTP(web_contents_))
    RecordNewTabLoadTime(web_contents_);
}

void SearchTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!load_details.is_main_frame)
    return;

  if (search::IsInstantNTP(web_contents_)) {
    // We (re)create the logger here because
    // 1. The logger tries to detect whether the NTP is being created at startup
    //    or from the user opening a new tab, and if we wait until later, it
    //    won't correctly detect this case.
    // 2. There can be multiple navigations to NTPs in a single web contents.
    //    The navigations can be user-triggered or automatic, e.g. we fall back
    //    to the local NTP if a remote NTP fails to load. Since logging should
    //    be scoped to the life time of a single NTP we reset the logger every
    //    time we reach a new NTP.
    logger_ = std::make_unique<NTPUserDataLogger>(
        Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
        // We use the NavigationController's URL since it might differ from the
        // WebContents URL which is usually chrome://newtab/.
        web_contents_->GetController().GetVisibleEntry()->GetURL());
    ipc_router_.SetInputInProgress(IsInputInProgress());
  }

  if (InInstantProcess(instant_service_, web_contents_))
    ipc_router_.OnNavigationEntryCommitted();
}

void SearchTabHelper::NtpThemeChanged(const NtpTheme& theme) {
  ipc_router_.SendNtpTheme(theme);
}

void SearchTabHelper::MostVisitedInfoChanged(
    const InstantMostVisitedInfo& most_visited_info) {
  ipc_router_.SendMostVisitedInfo(most_visited_info);
}

void SearchTabHelper::FocusOmnibox(bool focus) {
  search::FocusOmnibox(focus, web_contents_);
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

void SearchTabHelper::OnLogEvent(NTPLoggingEventType event,
                                 base::TimeDelta time) {
  if (logger_)
    logger_->LogEvent(event, time);
}

void SearchTabHelper::OnLogSuggestionEventWithValue(
    NTPSuggestionsLoggingEventType event,
    int data,
    base::TimeDelta time) {
  if (logger_)
    logger_->LogSuggestionEventWithValue(event, data, time);
}

void SearchTabHelper::OnLogMostVisitedImpression(
    const ntp_tiles::NTPTileImpression& impression) {
  if (logger_)
    logger_->LogMostVisitedImpression(impression);
}

void SearchTabHelper::OnLogMostVisitedNavigation(
    const ntp_tiles::NTPTileImpression& impression) {
  if (logger_)
    logger_->LogMostVisitedNavigation(impression);
}

void SearchTabHelper::OnSetCustomBackgroundInfo(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url,
    const std::string& collection_id) {
  if (instant_service_) {
    instant_service_->SetCustomBackgroundInfo(
        background_url, attribution_line_1, attribution_line_2, action_url,
        collection_id);
  }
}

void SearchTabHelper::FileSelected(const base::FilePath& path,
                                   int index,
                                   void* params) {
  if (instant_service_) {
    profile()->set_last_selected_directory(path.DirName());
    instant_service_->SelectLocalBackgroundImage(path);
  }

  select_file_dialog_ = nullptr;
  // File selection can happen at any time after NTP load, and is not logged
  // with the event.
  if (logger_) {
    logger_->LogEvent(NTP_CUSTOMIZE_LOCAL_IMAGE_DONE,
                      base::TimeDelta::FromSeconds(0));
    logger_->LogEvent(NTP_BACKGROUND_UPLOAD_DONE,
                      base::TimeDelta::FromSeconds(0));
  }

  ipc_router_.SendLocalBackgroundSelected();
}

void SearchTabHelper::FileSelectionCanceled(void* params) {
  select_file_dialog_ = nullptr;
  // File selection can happen at any time after NTP load, and is not logged
  // with the event.
  if (logger_) {
    logger_->LogEvent(NTP_CUSTOMIZE_LOCAL_IMAGE_CANCEL,
                      base::TimeDelta::FromSeconds(0));
    logger_->LogEvent(NTP_BACKGROUND_UPLOAD_CANCEL,
                      base::TimeDelta::FromSeconds(0));
  }
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
  if (web_contents_->GetController().GetPendingEntry() == nullptr)
    ipc_router_.SetInputInProgress(IsInputInProgress());
}

void SearchTabHelper::OnSelectLocalBackgroundImage() {
  if (select_file_dialog_)
    return;

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents_));

  const base::FilePath directory = profile()->last_selected_directory();

  gfx::NativeWindow parent_window = web_contents_->GetTopLevelNativeWindow();

  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  file_types.extensions.resize(1);
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpeg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("png"));
  file_types.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_UPLOAD_IMAGE_FORMAT));

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(), directory,
      &file_types, 0, base::FilePath::StringType(), parent_window, nullptr);
}

void SearchTabHelper::OnBlocklistSearchSuggestion(int task_version,
                                                  long task_id) {
  if (search_suggest_service_)
    search_suggest_service_->BlocklistSearchSuggestion(task_version, task_id);
}

void SearchTabHelper::OnBlocklistSearchSuggestionWithHash(
    int task_version,
    long task_id,
    const uint8_t hash[4]) {
  if (search_suggest_service_)
    search_suggest_service_->BlocklistSearchSuggestionWithHash(task_version,
                                                               task_id, hash);
}

void SearchTabHelper::OnSearchSuggestionSelected(int task_version,
                                                 long task_id,
                                                 const uint8_t hash[4]) {
  if (search_suggest_service_)
    search_suggest_service_->SearchSuggestionSelected(task_version, task_id,
                                                      hash);
}

void SearchTabHelper::OnOptOutOfSearchSuggestions() {
  if (search_suggest_service_)
    search_suggest_service_->OptOutOfSearchSuggestions();
}

void SearchTabHelper::OnApplyDefaultTheme() {
  if (chrome_colors_service_ &&
      search::DefaultSearchProviderIsGoogle(profile())) {
    chrome_colors_service_->ApplyDefaultTheme(web_contents_);
  }
}

void SearchTabHelper::OnApplyAutogeneratedTheme(SkColor color) {
  if (chrome_colors_service_ &&
      search::DefaultSearchProviderIsGoogle(profile())) {
    chrome_colors_service_->ApplyAutogeneratedTheme(color, web_contents_);
  }
}

void SearchTabHelper::OnRevertThemeChanges() {
  if (chrome_colors_service_ &&
      search::DefaultSearchProviderIsGoogle(profile())) {
    chrome_colors_service_->RevertThemeChanges();
  }
}

void SearchTabHelper::OnConfirmThemeChanges() {
  if (chrome_colors_service_ &&
      search::DefaultSearchProviderIsGoogle(profile())) {
    chrome_colors_service_->ConfirmThemeChanges();
  }
}

Profile* SearchTabHelper::profile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

bool SearchTabHelper::IsInputInProgress() const {
  return search::IsOmniboxInputInProgress(web_contents_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchTabHelper)
