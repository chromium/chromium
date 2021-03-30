// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include <memory>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/extension_checkup.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/search/chrome_colors/chrome_colors_factory.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/search/promos/promo_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search/search_suggest/search_suggest_service.h"
#include "chrome/browser/search/search_suggest/search_suggest_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/search/ntp_user_data_logger.h"
#include "chrome/browser/ui/search/omnibox_mojo_utils.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/search/omnibox.mojom.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/favicon/core/favicon_service.h"
#include "components/google/core/common/google_util.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_controller_emitter.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "components/search_engines/omnibox_focus_type.h"
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
#include "extensions/common/extension_features.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace {

// Converts an in-memory bitmap data to a base64 data url.
std::string GetBitmapDataUrl(const char* data, size_t size) {
  std::string base_64;
  base::Base64Encode(base::StringPiece(data, size), &base_64);
  return "data:image/png;base64," + base_64;
}

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
      instant_service_(nullptr),
      favicon_cache_(FaviconServiceFactory::GetForProfile(
                         profile(),
                         ServiceAccessType::EXPLICIT_ACCESS),
                     HistoryServiceFactory::GetForProfile(
                         profile(),
                         ServiceAccessType::EXPLICIT_ACCESS)) {
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
  if (!navigation_handle->IsInMainFrame())
    return;

  if (navigation_handle->GetReloadType() != content::ReloadType::NONE)
    time_of_first_autocomplete_query_ = base::TimeTicks();

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

bool SearchTabHelper::OnAddCustomLink(const GURL& url,
                                      const std::string& title) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    return instant_service_->AddCustomLink(url, title);
  return false;
}

bool SearchTabHelper::OnUpdateCustomLink(const GURL& url,
                                         const GURL& new_url,
                                         const std::string& new_title) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    return instant_service_->UpdateCustomLink(url, new_url, new_title);
  return false;
}

bool SearchTabHelper::OnReorderCustomLink(const GURL& url, int new_pos) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    return instant_service_->ReorderCustomLink(url, new_pos);
  return false;
}

bool SearchTabHelper::OnDeleteCustomLink(const GURL& url) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    return instant_service_->DeleteCustomLink(url);
  return false;
}

void SearchTabHelper::OnUndoCustomLinkAction() {
  if (instant_service_)
    instant_service_->UndoCustomLinkAction();
}

void SearchTabHelper::OnResetCustomLinks() {
  if (instant_service_)
    instant_service_->ResetCustomLinks();
}

void SearchTabHelper::OnToggleMostVisitedOrCustomLinks() {
  if (instant_service_)
    instant_service_->ToggleMostVisitedOrCustomLinks();
}

void SearchTabHelper::OnToggleShortcutsVisibility(bool do_notify) {
  if (instant_service_)
    instant_service_->ToggleShortcutsVisibility(do_notify);
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

void SearchTabHelper::PasteIntoOmnibox(const std::u16string& text) {
  search::PasteIntoOmnibox(text, web_contents_);
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

void SearchTabHelper::OnResultChanged(AutocompleteController* controller,
                                      bool default_result_changed) {
  DCHECK(controller == autocomplete_controller_.get());

  if (!autocomplete_controller_) {
    NOTREACHED();
    return;
  }

  if (!search::DefaultSearchProviderIsGoogle(profile())) {
    return;
  }

  ipc_router_.AutocompleteResultChanged(omnibox::CreateAutocompleteResult(
      autocomplete_controller_->input().text(),
      autocomplete_controller_->result(),
      BookmarkModelFactory::GetForBrowserContext(profile()),
      profile()->GetPrefs()));

  BitmapFetcherService* bitmap_fetcher_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile());

  int match_index = -1;
  for (const auto& match : autocomplete_controller_->result()) {
    match_index++;

    // Create new bitmap requests.
    if (!match.image_url.is_empty()) {
      bitmap_fetcher_service->RequestImage(
          match.image_url, base::BindOnce(&SearchTabHelper::OnBitmapFetched,
                                          weak_factory_.GetWeakPtr(),
                                          match_index, match.image_url.spec()));
    }

    // Request favicons for navigational matches.
    if (!AutocompleteMatch::IsSearchType(match.type) &&
        match.type != AutocompleteMatchType::DOCUMENT_SUGGESTION) {
      gfx::Image favicon = favicon_cache_.GetLargestFaviconForPageUrl(
          match.destination_url,
          base::BindOnce(&SearchTabHelper::OnFaviconFetched,
                         weak_factory_.GetWeakPtr(), match_index,
                         match.destination_url.spec()));
      if (!favicon.IsEmpty()) {
        OnFaviconFetched(match_index, match.destination_url.spec(), favicon);
      }
    }
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

void SearchTabHelper::OnBitmapFetched(int match_index,
                                      const std::string& image_url,
                                      const SkBitmap& bitmap) {
  auto data = gfx::Image::CreateFrom1xBitmap(bitmap).As1xPNGBytes();
  std::string data_url = GetBitmapDataUrl(data->front_as<char>(), data->size());

  ipc_router_.AutocompleteMatchImageAvailable(match_index, image_url, data_url);
}

void SearchTabHelper::OnFaviconFetched(int match_index,
                                       const std::string& page_url,
                                       const gfx::Image& favicon) {
  DCHECK(!favicon.IsEmpty());
  auto data = favicon.As1xPNGBytes();
  std::string data_url = GetBitmapDataUrl(data->front_as<char>(), data->size());

  ipc_router_.AutocompleteMatchImageAvailable(match_index, page_url, data_url);
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

void SearchTabHelper::QueryAutocomplete(const std::u16string& input,
                                        bool prevent_inline_autocomplete) {
  if (!search::DefaultSearchProviderIsGoogle(profile()))
    return;

  if (!autocomplete_controller_) {
    autocomplete_controller_ = std::make_unique<AutocompleteController>(
        std::make_unique<ChromeAutocompleteProviderClient>(profile()),
        AutocompleteClassifier::DefaultOmniboxProviders());
    autocomplete_controller_->AddObserver(this);

    OmniboxControllerEmitter* emitter =
        OmniboxControllerEmitter::GetForBrowserContext(profile());
    if (emitter)
      autocomplete_controller_->AddObserver(emitter);
  }

  if (time_of_first_autocomplete_query_.is_null() && !input.empty())
    time_of_first_autocomplete_query_ = base::TimeTicks::Now();

  AutocompleteInput autocomplete_input(
      input, metrics::OmniboxEventProto::NTP_REALBOX,
      ChromeAutocompleteSchemeClassifier(profile()));
  // TODO(tommycli): We use the input being empty as a signal we are requesting
  // on-focus suggestions. It would be nice if we had a more explicit signal.
  autocomplete_input.set_focus_type(input.empty() ? OmniboxFocusType::ON_FOCUS
                                                  : OmniboxFocusType::DEFAULT);
  autocomplete_input.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete);

  // We do not want keyword matches for the NTP realbox, which has no UI
  // facilities to support them.
  autocomplete_input.set_prefer_keyword(false);
  autocomplete_input.set_allow_exact_keyword_match(false);

  autocomplete_controller_->Start(autocomplete_input);
}

namespace {

class DeleteAutocompleteMatchConfirmDelegate
    : public TabModalConfirmDialogDelegate {
 public:
  DeleteAutocompleteMatchConfirmDelegate(
      content::WebContents* contents,
      std::u16string search_provider_name,
      base::OnceCallback<void(bool)> dialog_callback)
      : TabModalConfirmDialogDelegate(contents),
        search_provider_name_(search_provider_name),
        dialog_callback_(std::move(dialog_callback)) {
    DCHECK(dialog_callback_);
  }

  ~DeleteAutocompleteMatchConfirmDelegate() override {
    DCHECK(!dialog_callback_);
  }

  std::u16string GetTitle() override {
    return l10n_util::GetStringUTF16(
        IDS_OMNIBOX_REMOVE_SUGGESTION_BUBBLE_TITLE);
  }

  std::u16string GetDialogMessage() override {
    return l10n_util::GetStringFUTF16(
        IDS_OMNIBOX_REMOVE_SUGGESTION_BUBBLE_DESCRIPTION,
        search_provider_name_);
  }

  std::u16string GetAcceptButtonTitle() override {
    return l10n_util::GetStringUTF16(IDS_REMOVE);
  }

  void OnAccepted() override { std::move(dialog_callback_).Run(true); }

  void OnCanceled() override { std::move(dialog_callback_).Run(false); }

  void OnClosed() override {
    if (dialog_callback_)
      OnCanceled();
  }

 private:
  std::u16string search_provider_name_;
  base::OnceCallback<void(bool)> dialog_callback_;
};

}  // namespace

void SearchTabHelper::DeleteAutocompleteMatch(uint8_t line) {
  DCHECK(autocomplete_controller_);

  if (!search::DefaultSearchProviderIsGoogle(profile()) ||
      autocomplete_controller_->result().size() <= line ||
      !autocomplete_controller_->result().match_at(line).SupportsDeletion()) {
    return;
  }

  if (!base::FeatureList::IsEnabled(ntp_features::kConfirmSuggestionRemovals)) {
    // If suggestion transparency is disabled, the UI is also disabled. This
    // must've come from a keyboard shortcut, which are allowed to remove
    // without confirmation.
    OnDeleteAutocompleteMatchConfirm(line, true);
    return;
  }

  content::BrowserContext* context = web_contents_->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(context);
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  const auto& match = autocomplete_controller_->result().match_at(line);

  std::u16string search_provider_name;
  const TemplateURL* template_url =
      match.GetTemplateURL(template_url_service, false);
  if (!template_url)
    template_url = template_url_service->GetDefaultSearchProvider();
  if (template_url)
    search_provider_name = template_url->AdjustedShortNameForLocaleDirection();

  auto delegate = std::make_unique<DeleteAutocompleteMatchConfirmDelegate>(
      web_contents_, search_provider_name,
      base::BindOnce(&SearchTabHelper::OnDeleteAutocompleteMatchConfirm,
                     weak_factory_.GetWeakPtr(), line));
  TabModalConfirmDialog::Create(std::move(delegate), web_contents_);
}

void SearchTabHelper::OnDeleteAutocompleteMatchConfirm(
    uint8_t line,
    bool accepted) {
  DCHECK(autocomplete_controller_);

  bool success = false;
  std::vector<search::mojom::AutocompleteMatchPtr> matches;

  if (accepted && search::DefaultSearchProviderIsGoogle(profile()) &&
      autocomplete_controller_->result().size() > line) {
    const auto& match = autocomplete_controller_->result().match_at(line);
    if (match.SupportsDeletion()) {
      success = true;
      autocomplete_controller_->Stop(false);
      autocomplete_controller_->DeleteMatch(match);
      matches = omnibox::CreateAutocompleteMatches(
          autocomplete_controller_->result(),
          BookmarkModelFactory::GetForBrowserContext(profile()));
    }
  }
}

void SearchTabHelper::StopAutocomplete(bool clear_result) {
  if (!autocomplete_controller_)
    return;

  autocomplete_controller_->Stop(clear_result);

  if (clear_result)
    time_of_first_autocomplete_query_ = base::TimeTicks();
}

void SearchTabHelper::ToggleSuggestionGroupIdVisibility(
    int32_t suggestion_group_id) {
  if (!autocomplete_controller_)
    return;

  omnibox::SuggestionGroupVisibility new_value =
      autocomplete_controller_->result().IsSuggestionGroupIdHidden(
          profile()->GetPrefs(), suggestion_group_id)
          ? omnibox::SuggestionGroupVisibility::SHOWN
          : omnibox::SuggestionGroupVisibility::HIDDEN;
  omnibox::SetSuggestionGroupVisibility(profile()->GetPrefs(),
                                        suggestion_group_id, new_value);
}

void SearchTabHelper::LogCharTypedToRepaintLatency(uint32_t latency_ms) {
  UMA_HISTOGRAM_TIMES("NewTabPage.Realbox.CharTypedToRepaintLatency.ToPaint",
                      base::TimeDelta::FromMillisecondsD(latency_ms));
}

void SearchTabHelper::BlocklistPromo(const std::string& promo_id) {
  auto* promo_service = PromoServiceFactory::GetForProfile(profile());
  if (!promo_service) {
    NOTREACHED();
    return;
  }

  promo_service->BlocklistPromo(promo_id);
}

void SearchTabHelper::OpenExtensionsPage(double button,
                                         bool alt_key,
                                         bool ctrl_key,
                                         bool meta_key,
                                         bool shift_key) {
  if (!search::DefaultSearchProviderIsGoogle(profile()))
    return;
  base::RecordAction(base::UserMetricsAction("Extensions.NtpPromoClicked"));
  UMA_HISTOGRAM_ENUMERATION(
      "Extensions.Checkup.NtpPromoClicked",
      static_cast<extensions::CheckupMessage>(
          base::GetFieldTrialParamByFeatureAsInt(
              extensions_features::kExtensionsCheckup,
              extensions_features::kExtensionsCheckupBannerMessageParameter,
              static_cast<int>(extensions::CheckupMessage::NEUTRAL))));

  WindowOpenDisposition disposition =
      (button > 1) ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                   : ui::DispositionFromClick((button == 1.0), alt_key,
                                              ctrl_key, meta_key, shift_key);
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(chrome::kChromeUIExtensionsURL), content::Referrer(), disposition,
      ui::PAGE_TRANSITION_LINK, false));
}

void SearchTabHelper::OpenAutocompleteMatch(
    uint8_t line,
    const GURL& url,
    bool are_matches_showing,
    double time_elapsed_since_last_focus,
    double button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  DCHECK(autocomplete_controller_);

  if (!search::DefaultSearchProviderIsGoogle(profile()) ||
      !autocomplete_controller_ ||
      line >= autocomplete_controller_->result().size()) {
    return;
  }

  AutocompleteMatch match(autocomplete_controller_->result().match_at(line));
  if (match.destination_url != url) {
    // TODO(https://crbug.com/1020025): this could be malice or staleness.
    // Either way: don't navigate.
    return;
  }

  // TODO(crbug.com/1041129): The following logic for recording Omnibox metrics
  // is largely copied over to NewTabPageHandler::OpenAutocompleteMatch(). Make
  // sure any changes here is reflected there until one code path is obsolete.

  const auto now = base::TimeTicks::Now();
  base::TimeDelta elapsed_time_since_first_autocomplete_query =
      now - time_of_first_autocomplete_query_;
  autocomplete_controller_->UpdateMatchDestinationURLWithQueryFormulationTime(
      elapsed_time_since_first_autocomplete_query, &match);

  LOCAL_HISTOGRAM_BOOLEAN("Omnibox.EventCount", true);

  UMA_HISTOGRAM_MEDIUM_TIMES(
      "Omnibox.FocusToOpenTimeAnyPopupState3",
      base::TimeDelta::FromMilliseconds(time_elapsed_since_last_focus));

  if (ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                  ui::PAGE_TRANSITION_TYPED)) {
    navigation_metrics::RecordOmniboxURLNavigation(match.destination_url);
  }

  SuggestionAnswer::LogAnswerUsed(match.answer);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  if (template_url_service &&
      template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
          match.destination_url)) {
    // Note: will always be false for the realbox.
    UMA_HISTOGRAM_BOOLEAN("Omnibox.Search.OffTheRecord",
                          profile()->IsOffTheRecord());
    base::RecordAction(
        base::UserMetricsAction("OmniboxDestinationURLIsSearchOnDSP"));
  }

  AutocompleteMatch::LogSearchEngineUsed(match, template_url_service);

  auto* bookmark_model = BookmarkModelFactory::GetForBrowserContext(profile());
  if (bookmark_model->IsBookmarked(match.destination_url)) {
    RecordBookmarkLaunch(BOOKMARK_LAUNCH_LOCATION_OMNIBOX,
                         ProfileMetrics::GetBrowserProfileType(profile()));
  }

  const AutocompleteInput& input = autocomplete_controller_->input();
  WindowOpenDisposition disposition = ui::DispositionFromClick(
      button == 1.0, alt_key, ctrl_key, meta_key, shift_key);

  base::TimeDelta default_time_delta = base::TimeDelta::FromMilliseconds(-1);

  if (time_of_first_autocomplete_query_.is_null())
    elapsed_time_since_first_autocomplete_query = default_time_delta;

  base::TimeDelta elapsed_time_since_last_change_to_default_match =
      !autocomplete_controller_->last_time_default_match_changed().is_null()
          ? now - autocomplete_controller_->last_time_default_match_changed()
          : default_time_delta;

  OmniboxLog log(
      /*text=*/input.focus_type() != OmniboxFocusType::DEFAULT
          ? std::u16string()
          : input.text(),
      /*just_deleted_text=*/input.prevent_inline_autocomplete(),
      /*input_type=*/input.type(),
      /*in_keyword_mode=*/false,
      /*entry_method=*/metrics::OmniboxEventProto::INVALID,
      /*is_popup_open=*/are_matches_showing,
      /*selected_index=*/line,
      /*disposition=*/disposition,
      /*is_paste_and_go=*/false,
      /*tab_id=*/sessions::SessionTabHelper::IdForTab(web_contents_),
      /*current_page_classification=*/metrics::OmniboxEventProto::NTP_REALBOX,
      /*elapsed_time_since_user_first_modified_omnibox=*/
      elapsed_time_since_first_autocomplete_query,
      /*completed_length=*/match.allowed_to_be_default_match
          ? match.inline_autocompletion.length()
          : std::u16string::npos,
      /*elapsed_time_since_last_change_to_default_match=*/
      elapsed_time_since_last_change_to_default_match,
      /*result=*/autocomplete_controller_->result());
  autocomplete_controller_->AddProviderAndTriggeringLogs(&log);

  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile())
      ->OnOmniboxOpenedUrl(log);

  web_contents_->OpenURL(
      content::OpenURLParams(match.destination_url, content::Referrer(),
                             disposition, match.transition, false));
  // May delete us.
}

Profile* SearchTabHelper::profile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

bool SearchTabHelper::IsInputInProgress() const {
  return search::IsOmniboxInputInProgress(web_contents_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchTabHelper)
