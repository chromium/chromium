// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_handler.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/searchbox.mojom-shared.h"
#include "components/omnibox/browser/searchbox_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition_utils.h"

namespace {

// Query parameter key ("gs_ivs" = "Google Search Input Voice Search") and
// value ("1") used by Google Search (matching ComposeboxQueryController and
// New Tab Page voice search) to signal that the query was initiated via speech
// recognition, allowing Google Search to return voice-optimized results.
constexpr char kVoiceSearchQueryParameterKey[] = "gs_ivs";
constexpr char kVoiceSearchQueryParameterValue[] = "1";

bool IsAimEligible(Profile* profile) {
  auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  return aim_eligibility_service && aim_eligibility_service->IsAimEligible();
}

class OmniboxEverywhereClient : public ContextualOmniboxClient {
 public:
  OmniboxEverywhereClient(Profile* profile,
                          content::WebContents* web_contents,
                          OmniboxEverywhereService* service)
      : ContextualOmniboxClient(profile, web_contents), service_(service) {}
  ~OmniboxEverywhereClient() override = default;

  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override {
    return metrics::OmniboxEventProto::OMNIBOX_EVERYWHERE;
  }

  // Base class SearchboxOmniboxClient::OnAutocompleteAccept invokes
  // web_contents_->OpenURL(...) directly on the WebContents, which for
  // Omnibox Everywhere is the standalone popup widget. Omnibox Everywhere
  // routes all navigations to the browser window and dismisses the popup via
  // OmniboxEverywhereService::OpenUrl, so we intentionally override and bypass
  // the base class navigation.
  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match) override {
    if (service_) {
      service_->OpenUrl(destination_url, disposition, transition);
    }
  }

  void OpenUrl(GURL gurl, WindowOpenDisposition disposition) override {
    if (service_) {
      service_->OpenUrl(gurl, disposition, ui::PAGE_TRANSITION_GENERATED);
    }
  }

 private:
  raw_ptr<OmniboxEverywhereService> service_;
};

}  // namespace

OmniboxEverywhereHandler::OmniboxEverywhereHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_page,
    MetricsReporter* metrics_reporter,
    content::WebUI* web_ui,
    OmniboxEverywhereService* service,
    GetSessionHandleCallback get_session_callback,
    ScreenshareDelegate* screenshare_delegate)
    : ContextualSearchboxHandler(
          std::move(pending_page_handler),
          std::move(pending_page),
          Profile::FromWebUI(web_ui),
          web_ui->GetWebContents(),
          std::make_unique<OmniboxEverywhereClient>(Profile::FromWebUI(web_ui),
                                                    web_ui->GetWebContents(),
                                                    service),
          std::move(get_session_callback),
          screenshare_delegate),
      service_(service) {
  static_cast<ContextualOmniboxClient*>(client())->SetSuggestInputsCallback(
      base::BindRepeating(&OmniboxEverywhereHandler::GetSuggestInputs,
                          base::Unretained(this)));
  autocomplete_controller_observation_.Observe(autocomplete_controller());
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      omnibox::kShowAiModeOmniboxButton,
      base::BindRepeating(&OmniboxEverywhereHandler::OnAimEligibilityChanged,
                          base::Unretained(this)));
}

OmniboxEverywhereHandler::~OmniboxEverywhereHandler() = default;

void OmniboxEverywhereHandler::OnDriveUploadClicked(
    OnDriveUploadClickedCallback callback) {
  // Notify the service that the Google Drive picker is being opened so it can
  // suppress auto-dismissal of the standalone Omnibox Everywhere widget.
  service_->OnDrivePickerOpened();

  // Since the Omnibox Everywhere widget is a standalone popup without a native
  // embedding browser window, we must dynamically associate the WebContents
  // with the latest active browser window interface. Doing this on each click
  // ensures that even if the previously associated browser tab/window was
  // closed, the flow can still resolve a valid BrowserWindowInterface and
  // successfully reopen the modal picker dialog. If no active browser window
  // exists (e.g. Chrome is running in the background), we pass nullptr and
  // let the DrivePickerHostController handle the top-level dialog.
  ProfileBrowserCollection* profile_collection =
      ProfileBrowserCollection::GetForProfile(profile_);
  CHECK(profile_collection);
  BrowserWindowInterface* active_bwi =
      profile_collection->GetLastActiveBrowser();
  webui::SetBrowserWindowInterface(web_contents_, active_bwi);

  ContextualSearchboxHandler::OnDriveUploadClicked(std::move(callback));
}

void OmniboxEverywhereHandler::CleanupDrivePicker() {
  ContextualSearchboxHandler::CleanupDrivePicker();
  // Notify the service that the Drive picker has closed (either via success,
  // cancel, or error) so that the widget can regain focus and restore standard
  // auto-dismissal.
  service_->OnDrivePickerClosed();
}

void OmniboxEverywhereHandler::SubmitQuery(const std::string& query_text,
                                           uint8_t mouse_button,
                                           bool alt_key,
                                           bool ctrl_key,
                                           bool meta_key,
                                           bool shift_key,
                                           bool is_voice_search) {
  if (!is_voice_search) {
    ContextualSearchboxHandler::SubmitQuery(query_text, mouse_button, alt_key,
                                            ctrl_key, meta_key, shift_key,
                                            is_voice_search);
    return;
  }

  // In classic Omnibox mode, unlike Composebox mode (which handles contextual
  // file/tab attachments via ComposeboxEverywhereHandler and routes through
  // ComposeboxQueryController), there are no contextual attachments. Voice
  // searches from classic Omnibox mode should navigate directly to the user's
  // default search provider rather than being routed to AI Mode (udm=50).
  //
  // Unlike the New Tab Page, which can perform a client-side navigation inside
  // its own tab, Omnibox Everywhere runs in a standalone popup window. We must
  // route query submissions through OpenUrl so that the navigation redirects
  // to the active browser window/tab and dismisses the popup widget.
  //
  // This override lives specifically in OmniboxEverywhereHandler to avoid
  // altering default behavior in the shared base ContextualSearchboxHandler.
  TemplateURLService* template_url_service = client()->GetTemplateURLService();
  if (!template_url_service) {
    return;
  }
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider) {
    return;
  }

  TemplateURLRef::SearchTermsArgs search_terms_args(
      base::UTF8ToUTF16(query_text));
  if (search::DefaultSearchProviderIsGoogle(template_url_service)) {
    search_terms_args.additional_query_params = base::StrCat(
        {kVoiceSearchQueryParameterKey, "=", kVoiceSearchQueryParameterValue});
  }

  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  GURL search_url = GURL(default_provider->url_ref().ReplaceSearchTerms(
      search_terms_args, template_url_service->search_terms_data()));
  ClearFiles(/*should_block_auto_suggested_tabs=*/false,
             /*query_submitted=*/true);
  client()->OpenUrl(search_url, disposition);
}

void OmniboxEverywhereHandler::OpenProfilePicker() {
  if (service_) {
    service_->ShowProfilePicker();
  }
}

void OmniboxEverywhereHandler::ActivateKeyword(
    uint8_t line,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    bool is_mouse_event) {
  // OmniboxEverywhere does not make use of OmniboxEditModel. Keyword mode is
  // handled directly by the frontend SearchboxMixin via `onKeywordClick`.
}

// TODO(crbug.com/550402735): Combine into SearchboxHandler for clean reuse.
std::optional<searchbox::mojom::AutocompleteMatchPtr>
OmniboxEverywhereHandler::CreateAutocompleteMatch(
    const AutocompleteMatch& match,
    size_t line,
    bookmarks::BookmarkModel* bookmark_model,
    const omnibox::GroupConfigMap& suggestion_groups_map,
    const TemplateURLService* turl_service) const {
  auto mojom_match = SearchboxHandler::CreateAutocompleteMatch(
      match, line, bookmark_model, suggestion_groups_map, turl_service);

  if (mojom_match) {
    KeywordState keyword_state;
    std::u16string keyword;
    std::u16string keyword_placeholder;
    match.GetKeywordUiState(turl_service,
                            client()->IsHistoryEmbeddingsEnabled(),
                            &keyword_state, &keyword, &keyword_placeholder);

    searchbox::mojom::KeywordType keyword_type;
    bool has_keyword = false;
    if (keyword_state == KeywordState::kKeyword) {
      keyword_type = searchbox::mojom::KeywordType::kInKeyword;
      has_keyword = true;
    } else if (match.HasInstantKeyword(turl_service)) {
      keyword_type = searchbox::mojom::KeywordType::kInstant;
      has_keyword = true;
    } else if (keyword_state == KeywordState::kHint ||
               !match.associated_keyword.empty()) {
      keyword_type = searchbox::mojom::KeywordType::kChip;
      has_keyword = true;
    }

    // Populate `keyword_model`.
    if (has_keyword) {
      auto keyword_model = searchbox::mojom::MatchKeywordModel::New();
      keyword_model->type = keyword_type;
      keyword_model->keyword = base::UTF16ToUTF8(keyword);
      keyword_model->placeholder = base::UTF16ToUTF8(keyword_placeholder);
      const auto names = searchbox::GetKeywordLabelNames(keyword, turl_service);
      keyword_model->chip_hint = base::UTF16ToUTF8(names.full_name);
      keyword_model->chip_a11y =
          l10n_util::GetStringFUTF8(IDS_ACC_KEYWORD_MODE, names.short_name);
      mojom_match.value()->keyword_model = std::move(keyword_model);
    }
  }

  return mojom_match;
}

void OmniboxEverywhereHandler::OnAimEligibilityChanged() {
  if (page()) {
    page()->UpdateAimPopupEligibility(
        IsAimEligible(profile_) &&
        profile_->GetPrefs()->GetBoolean(omnibox::kShowAiModeOmniboxButton));
  }
}

void OmniboxEverywhereHandler::OpenUrl(
    GURL url,
    const WindowOpenDisposition disposition,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  if (service_) {
    service_->OpenUrl(url, disposition, ui::PAGE_TRANSITION_LINK,
                      std::move(navigation_handle_callback));
  }
}
