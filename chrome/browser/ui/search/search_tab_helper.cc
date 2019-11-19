// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/chrome_colors/chrome_colors_factory.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/search/promos/promo_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search/search_suggest/search_suggest_service.h"
#include "chrome/browser/search/search_suggest/search_suggest_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/search/ntp_user_data_logger.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/search.mojom.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

std::vector<chrome::mojom::AutocompleteMatchPtr> CreateAutocompleteMatches(
    const AutocompleteResult& result) {
  std::vector<chrome::mojom::AutocompleteMatchPtr> matches;
  for (const AutocompleteMatch& match : result) {
    chrome::mojom::AutocompleteMatchPtr mojom_match =
        chrome::mojom::AutocompleteMatch::New();
    mojom_match->allowed_to_be_default_match =
        match.allowed_to_be_default_match;
    mojom_match->contents = match.contents;
    for (const auto& contents_class : match.contents_class) {
      mojom_match->contents_class.push_back(
          chrome::mojom::ACMatchClassification::New(contents_class.offset,
                                                    contents_class.style));
    }
    mojom_match->description = match.description;
    for (const auto& description_class : match.description_class) {
      mojom_match->description_class.push_back(
          chrome::mojom::ACMatchClassification::New(description_class.offset,
                                                    description_class.style));
    }
    mojom_match->destination_url = match.destination_url.spec();
    mojom_match->fill_into_edit = match.fill_into_edit;
    mojom_match->inline_autocompletion = match.inline_autocompletion;
    mojom_match->is_search_type = AutocompleteMatch::IsSearchType(match.type);
    mojom_match->swap_contents_and_description =
        match.swap_contents_and_description;
    mojom_match->type = AutocompleteMatchType::ToString(match.type);
    mojom_match->supports_deletion = match.SupportsDeletion();
    matches.push_back(std::move(mojom_match));
  }
  return matches;
}

bool IsCacheableNTP(content::WebContents* contents) {
  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  return search::NavEntryIsInstantNTP(contents, entry) &&
         entry->GetURL() != chrome::kChromeSearchLocalNtpUrl;
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
}

SearchTabHelper::~SearchTabHelper() {
  if (instant_service_)
    instant_service_->RemoveObserver(this);
}

void SearchTabHelper::OmniboxInputStateChanged() {
  ipc_router_.SetInputInProgress(IsInputInProgress());
}

void SearchTabHelper::OmniboxFocusChanged(OmniboxFocusState state,
                                          OmniboxFocusChangeReason reason) {
  ipc_router_.OmniboxFocusChanged(state, reason);

  // Don't send oninputstart/oninputend updates in response to focus changes
  // if there's a navigation in progress. This prevents Chrome from sending
  // a spurious oninputend when the user accepts a match in the omnibox.
  if (web_contents_->GetController().GetPendingEntry() == nullptr)
    ipc_router_.SetInputInProgress(IsInputInProgress());
}

void SearchTabHelper::OnTabActivated() {
  ipc_router_.OnTabActivated();

  if (search::IsInstantNTP(web_contents_)) {
    if (instant_service_)
      instant_service_->OnNewTabPageOpened();

    // Force creation of NTPUserDataLogger, if we loaded an NTP. The
    // NTPUserDataLogger tries to detect whether the NTP is being created at
    // startup or from the user opening a new tab, and if we wait until later,
    // it won't correctly detect this case.
    NTPUserDataLogger::GetOrCreateFromWebContents(web_contents_);
  }
}

void SearchTabHelper::OnTabDeactivated() {
  ipc_router_.OnTabDeactivated();
}

void SearchTabHelper::OnTabClosing() {
  if (search::IsInstantNTP(web_contents_) && chrome_colors_service_)
    chrome_colors_service_->RevertThemeChangesForTab(
        web_contents_, chrome_colors::RevertReason::TAB_CLOSED);
}

void SearchTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // When navigating away from NTP we should revert all the unconfirmed state.
  if (search::IsInstantNTP(web_contents_) && chrome_colors_service_) {
    chrome_colors_service_->RevertThemeChangesForTab(
        web_contents_, chrome_colors::RevertReason::NAVIGATION);
  }

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

  if (search::IsInstantNTP(web_contents_))
    ipc_router_.SetInputInProgress(IsInputInProgress());

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
  OmniboxView* omnibox_view = GetOmniboxView();
  if (!omnibox_view)
    return;

  if (focus) {
    // This is an invisible focus to support "realbox" implementations on NTPs
    // (including other search providers). We shouldn't consider it as the user
    // explicitly focusing the omnibox.
    omnibox_view->SetFocus(/*is_user_initiated=*/false);
    omnibox_view->model()->SetCaretVisibility(false);
    // If the user clicked on the fakebox, any text already in the omnibox
    // should get cleared when they start typing. Selecting all the existing
    // text is a convenient way to accomplish this. It also gives a slight
    // visual cue to users who really understand selection state about what
    // will happen if they start typing.
    omnibox_view->SelectAll(false);
#if !defined(OS_WIN)
    omnibox_view->ShowVirtualKeyboardIfEnabled();
#endif
  } else {
    // Remove focus only if the popup is closed. This will prevent someone
    // from changing the omnibox value and closing the popup without user
    // interaction.
    if (!omnibox_view->model()->popup_model()->IsOpen())
      web_contents()->Focus();
  }
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
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogEvent(event, time);
}

void SearchTabHelper::OnLogSuggestionEventWithValue(
    NTPSuggestionsLoggingEventType event,
    int data,
    base::TimeDelta time) {
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogSuggestionEventWithValue(event, data, time);
}

void SearchTabHelper::OnLogMostVisitedImpression(
    const ntp_tiles::NTPTileImpression& impression) {
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogMostVisitedImpression(impression);
}

void SearchTabHelper::OnLogMostVisitedNavigation(
    const ntp_tiles::NTPTileImpression& impression) {
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogMostVisitedNavigation(impression);
}

void SearchTabHelper::PasteIntoOmnibox(const base::string16& text) {
  OmniboxView* omnibox_view = GetOmniboxView();
  if (!omnibox_view)
    return;
  // The first case is for right click to paste, where the text is retrieved
  // from the clipboard already sanitized. The second case is needed to handle
  // drag-and-drop value and it has to be sanitazed before setting it into the
  // omnibox.
  base::string16 text_to_paste = text.empty()
                                     ? GetClipboardText()
                                     : omnibox_view->SanitizeTextForPaste(text);

  if (text_to_paste.empty())
    return;

  if (!omnibox_view->model()->has_focus()) {
    // Pasting into a "realbox" should not be considered the user explicitly
    // focusing the omnibox.
    omnibox_view->SetFocus(/*is_user_initiated=*/false);
  }

  omnibox_view->OnBeforePossibleChange();
  omnibox_view->model()->OnPaste();
  omnibox_view->SetUserText(text_to_paste);
  omnibox_view->OnAfterPossibleChange(true);
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
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogEvent(NTP_CUSTOMIZE_LOCAL_IMAGE_DONE,
                 base::TimeDelta::FromSeconds(0));
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogEvent(NTP_BACKGROUND_UPLOAD_DONE, base::TimeDelta::FromSeconds(0));

  ipc_router_.SendLocalBackgroundSelected();
}

void SearchTabHelper::FileSelectionCanceled(void* params) {
  select_file_dialog_ = nullptr;
  // File selection can happen at any time after NTP load, and is not logged
  // with the event.
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogEvent(NTP_CUSTOMIZE_LOCAL_IMAGE_CANCEL,
                 base::TimeDelta::FromSeconds(0));
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogEvent(NTP_BACKGROUND_UPLOAD_CANCEL, base::TimeDelta::FromSeconds(0));
}

void SearchTabHelper::OnResultChanged(bool default_result_changed) {
  if (!autocomplete_controller_) {
    NOTREACHED();
    return;
  }

  if (!autocomplete_controller_->done())
    return;

  if (!search::DefaultSearchProviderIsGoogle(profile())) {
    return;
  }

  ipc_router_.AutocompleteResultChanged(chrome::mojom::AutocompleteResult::New(
      autocomplete_controller_->input().text(),
      CreateAutocompleteMatches(autocomplete_controller_->result())));
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
      ui::SelectFileDialog::SELECT_OPEN_FILE, base::string16(), directory,
      &file_types, 0, base::FilePath::StringType(), parent_window, nullptr);
}

const OmniboxView* SearchTabHelper::GetOmniboxView() const {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser)
    return nullptr;

  return browser->window()->GetLocationBar()->GetOmniboxView();
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
  if (chrome_colors_service_)
    chrome_colors_service_->ApplyDefaultTheme(web_contents_);
}

void SearchTabHelper::OnApplyAutogeneratedTheme(SkColor color) {
  if (chrome_colors_service_)
    chrome_colors_service_->ApplyAutogeneratedTheme(color, web_contents_);
}

void SearchTabHelper::OnRevertThemeChanges() {
  if (chrome_colors_service_)
    chrome_colors_service_->RevertThemeChanges();
}

void SearchTabHelper::OnConfirmThemeChanges() {
  if (chrome_colors_service_)
    chrome_colors_service_->ConfirmThemeChanges();
}

void SearchTabHelper::QueryAutocomplete(const base::string16& input,
                                        bool prevent_inline_autocomplete) {
  if (!search::DefaultSearchProviderIsGoogle(profile())) {
    return;
  }

  if (!autocomplete_controller_) {
    int providers = AutocompleteProvider::TYPE_BOOKMARK |
                    AutocompleteProvider::TYPE_BUILTIN |
                    AutocompleteProvider::TYPE_HISTORY_QUICK |
                    AutocompleteProvider::TYPE_HISTORY_URL |
                    AutocompleteProvider::TYPE_SEARCH |
                    AutocompleteProvider::TYPE_ZERO_SUGGEST |
                    AutocompleteProvider::TYPE_ZERO_SUGGEST_LOCAL_HISTORY;
    autocomplete_controller_ = std::make_unique<AutocompleteController>(
        std::make_unique<ChromeAutocompleteProviderClient>(profile()), this,
        providers);
  }

  AutocompleteInput autocomplete_input(
      input, metrics::OmniboxEventProto::NTP_REALBOX,
      ChromeAutocompleteSchemeClassifier(profile()));
  autocomplete_input.set_from_omnibox_focus(input.empty());
  autocomplete_input.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete);
  autocomplete_controller_->Start(autocomplete_input);
}

namespace {

class DeleteAutocompleteMatchConfirmDelegate
    : public TabModalConfirmDialogDelegate {
 public:
  DeleteAutocompleteMatchConfirmDelegate(
      content::WebContents* contents,
      base::string16 search_provider_name,
      base::OnceCallback<void(bool)> dialog_callback)
      : TabModalConfirmDialogDelegate(contents),
        search_provider_name_(search_provider_name),
        dialog_callback_(std::move(dialog_callback)) {
    DCHECK(dialog_callback_);
  }

  ~DeleteAutocompleteMatchConfirmDelegate() override {
    DCHECK(!dialog_callback_);
  }

  base::string16 GetTitle() override {
    return l10n_util::GetStringUTF16(
        IDS_OMNIBOX_REMOVE_SUGGESTION_BUBBLE_TITLE);
  }

  base::string16 GetDialogMessage() override {
    return l10n_util::GetStringFUTF16(
        IDS_OMNIBOX_REMOVE_SUGGESTION_BUBBLE_DESCRIPTION,
        search_provider_name_);
  }

  base::string16 GetAcceptButtonTitle() override {
    return l10n_util::GetStringUTF16(IDS_REMOVE);
  }

  void OnAccepted() override { std::move(dialog_callback_).Run(true); }

  void OnCanceled() override { std::move(dialog_callback_).Run(false); }

  void OnClosed() override {
    if (dialog_callback_)
      OnCanceled();
  }

 private:
  base::string16 search_provider_name_;
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

  base::string16 search_provider_name;
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
  std::vector<chrome::mojom::AutocompleteMatchPtr> matches;

  if (accepted && search::DefaultSearchProviderIsGoogle(profile()) &&
      autocomplete_controller_->result().size() > line) {
    const auto& match = autocomplete_controller_->result().match_at(line);
    if (match.SupportsDeletion()) {
      success = true;
      autocomplete_controller_->Stop(false);
      autocomplete_controller_->DeleteMatch(match);
      matches = CreateAutocompleteMatches(autocomplete_controller_->result());
    }
  }
}

void SearchTabHelper::StopAutocomplete(bool clear_result) {
  if (!autocomplete_controller_) {
    return;
  }

  autocomplete_controller_->Stop(clear_result);
}

void SearchTabHelper::BlocklistPromo(const std::string& promo_id) {
  auto* promo_service = PromoServiceFactory::GetForProfile(profile());
  if (!promo_service) {
    NOTREACHED();
    return;
  }

  promo_service->BlocklistPromo(promo_id);
}

void SearchTabHelper::OpenAutocompleteMatch(uint8_t line,
                                            const GURL& url,
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

  const auto& match = autocomplete_controller_->result().match_at(line);
  if (match.destination_url != url) {
    // TODO(https://crbug.com/1020025): this could be malice or staleness.
    // Either way: don't navigate.
    return;
  }

  WindowOpenDisposition disposition = ui::DispositionFromClick(
      button == 1.0, alt_key, ctrl_key, meta_key, shift_key);
  web_contents_->OpenURL(
      content::OpenURLParams(match.destination_url, content::Referrer(),
                             disposition, ui::PAGE_TRANSITION_LINK, false));
}

OmniboxView* SearchTabHelper::GetOmniboxView() {
  return const_cast<OmniboxView*>(
      const_cast<const SearchTabHelper*>(this)->GetOmniboxView());
}

Profile* SearchTabHelper::profile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

bool SearchTabHelper::IsInputInProgress() const {
  const OmniboxView* omnibox_view = GetOmniboxView();
  return omnibox_view && omnibox_view->model()->user_input_in_progress() &&
         omnibox_view->model()->focus_state() == OMNIBOX_FOCUS_VISIBLE;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchTabHelper)
