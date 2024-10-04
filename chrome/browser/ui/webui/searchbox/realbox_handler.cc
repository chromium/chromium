// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/omnibox/autocomplete_controller_emitter_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "net/cookies/cookie_util.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition_utils.h"

namespace {
// TODO(crbug.com/40263573): Consider inheriting from `ChromeOmniboxClient`
//  to avoid reimplementation of methods like `OnBookmarkLaunched`.
class RealboxOmniboxClient final : public OmniboxClient {
 public:
  RealboxOmniboxClient(Profile* profile,
                       content::WebContents* web_contents,
                       LensSearchboxClient* lens_searchbox_client);
  ~RealboxOmniboxClient() override;

  // OmniboxClient:
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;
  const GURL& GetURL() const override;
  bool IsPasteAndGoEnabled() const override;
  SessionID GetSessionID() const override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  AutocompleteControllerEmitter* GetAutocompleteControllerEmitter() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  bool ShouldDefaultTypedNavigationsToHttps() const override;
  int GetHttpsPortForTesting() const override;
  bool IsUsingFakeHttpsForHttpsUpgradeTesting() const override;
  gfx::Image GetSizedIcon(const gfx::VectorIcon& vector_icon_type,
                          SkColor vector_icon_color) const override;
  std::u16string GetFormattedFullURL() const override;
  std::u16string GetURLForDisplay() const override;
  GURL GetNavigationEntryURL() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  net::CertStatus GetCertStatus() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;
  void OnThumbnailRemoved() override;
  gfx::Image GetFaviconForPageUrl(
      const GURL& page_url,
      FaviconFetchedCallback on_favicon_fetched) override;
  void OnBookmarkLaunched() override;
  void OnURLOpenedFromOmnibox(OmniboxLog* log) override;
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
      const AutocompleteMatch& alternative_nav_match,
      IDNA2008DeviationCharacter deviation_char_in_hostname) override;
  base::WeakPtr<OmniboxClient> AsWeakPtr() override;

  void SetLensSearchboxClientForTesting(  // IN-TEST
      LensSearchboxClient* lens_searchbox_client) {
    lens_searchbox_client_ = lens_searchbox_client;
  }

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  // Owns RealboxHandler which owns this.
  raw_ptr<LensSearchboxClient> lens_searchbox_client_;
  ChromeAutocompleteSchemeClassifier scheme_classifier_;
  // This is unused, but needed for `GetVectorIcon()`.
  gfx::VectorIcon vector_icon_{nullptr, 0u, ""};
  base::WeakPtrFactory<RealboxOmniboxClient> weak_factory_{this};
};

RealboxOmniboxClient::RealboxOmniboxClient(
    Profile* profile,
    content::WebContents* web_contents,
    LensSearchboxClient* lens_searchbox_client)
    : profile_(profile),
      web_contents_(web_contents),
      lens_searchbox_client_(lens_searchbox_client),
      scheme_classifier_(ChromeAutocompleteSchemeClassifier(profile)) {}

RealboxOmniboxClient::~RealboxOmniboxClient() = default;

std::unique_ptr<AutocompleteProviderClient>
RealboxOmniboxClient::CreateAutocompleteProviderClient() {
  return std::make_unique<ChromeAutocompleteProviderClient>(profile_);
}

const GURL& RealboxOmniboxClient::GetURL() const {
  if (lens_searchbox_client_) {
    return lens_searchbox_client_->GetPageURL();
  }
  return GURL::EmptyGURL();
}

bool RealboxOmniboxClient::IsPasteAndGoEnabled() const {
  return false;
}

SessionID RealboxOmniboxClient::GetSessionID() const {
  if (lens_searchbox_client_) {
    return lens_searchbox_client_->GetTabId();
  }
  return sessions::SessionTabHelper::IdForTab(web_contents_);
}

PrefService* RealboxOmniboxClient::GetPrefs() {
  return profile_->GetPrefs();
}

const PrefService* RealboxOmniboxClient::GetPrefs() const {
  return profile_->GetPrefs();
}

bookmarks::BookmarkModel* RealboxOmniboxClient::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

AutocompleteControllerEmitter*
RealboxOmniboxClient::GetAutocompleteControllerEmitter() {
  return AutocompleteControllerEmitterFactory::GetForBrowserContext(profile_);
}

TemplateURLService* RealboxOmniboxClient::GetTemplateURLService() {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

const AutocompleteSchemeClassifier& RealboxOmniboxClient::GetSchemeClassifier()
    const {
  return scheme_classifier_;
}

AutocompleteClassifier* RealboxOmniboxClient::GetAutocompleteClassifier() {
  return AutocompleteClassifierFactory::GetForProfile(profile_);
}

bool RealboxOmniboxClient::ShouldDefaultTypedNavigationsToHttps() const {
  return false;
}

int RealboxOmniboxClient::GetHttpsPortForTesting() const {
  return 0;
}

bool RealboxOmniboxClient::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  return false;
}

gfx::Image RealboxOmniboxClient::GetSizedIcon(
    const gfx::VectorIcon& vector_icon_type,
    SkColor vector_icon_color) const {
  return gfx::Image();
}

std::u16string RealboxOmniboxClient::GetFormattedFullURL() const {
  return u"";
}

std::u16string RealboxOmniboxClient::GetURLForDisplay() const {
  return u"";
}

GURL RealboxOmniboxClient::GetNavigationEntryURL() const {
  return GURL();
}

metrics::OmniboxEventProto::PageClassification
RealboxOmniboxClient::GetPageClassification(bool is_prefetch) const {
  if (lens_searchbox_client_) {
    return lens_searchbox_client_->GetPageClassification();
  }
  return metrics::OmniboxEventProto::NTP_REALBOX;
}

security_state::SecurityLevel RealboxOmniboxClient::GetSecurityLevel() const {
  return security_state::SecurityLevel::NONE;
}

net::CertStatus RealboxOmniboxClient::GetCertStatus() const {
  return 0;
}

const gfx::VectorIcon& RealboxOmniboxClient::GetVectorIcon() const {
  return vector_icon_;
}

gfx::Image RealboxOmniboxClient::GetFaviconForPageUrl(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return gfx::Image();
}

std::optional<lens::proto::LensOverlaySuggestInputs>
RealboxOmniboxClient::GetLensOverlaySuggestInputs() const {
  if (lens_searchbox_client_) {
    return lens_searchbox_client_->GetLensSuggestInputs();
  }
  return std::nullopt;
}

void RealboxOmniboxClient::OnThumbnailRemoved() {
  if (lens_searchbox_client_) {
    lens_searchbox_client_->OnThumbnailRemoved();
  }
}

void RealboxOmniboxClient::OnBookmarkLaunched() {
  RecordBookmarkLaunch(BookmarkLaunchLocation::kOmnibox,
                       profile_metrics::GetBrowserProfileType(profile_));
}

void RealboxOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  if (auto* search_prefetch_service =
          SearchPrefetchServiceFactory::GetForProfile(profile_)) {
    search_prefetch_service->OnURLOpenedFromOmnibox(log);
  }
  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->OnOmniboxOpenedUrl(*log);
}

void RealboxOmniboxClient::OnAutocompleteAccept(
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
    const AutocompleteMatch& alternative_nav_match,
    IDNA2008DeviationCharacter deviation_char_in_hostname) {
  if (lens_searchbox_client_) {
    lens_searchbox_client_->OnSuggestionAccepted(
        destination_url, match.type,
        match.subtypes.contains(omnibox::SUBTYPE_ZERO_PREFIX));
    return;
  }
  web_contents_->OpenURL(
      content::OpenURLParams(destination_url, content::Referrer(), disposition,
                             transition, false),
      /*navigation_handle_callback=*/{});
}

base::WeakPtr<OmniboxClient> RealboxOmniboxClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace

RealboxHandler::RealboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents,
    MetricsReporter* metrics_reporter,
    LensSearchboxClient* lens_searchbox_client,
    OmniboxController* omnibox_controller)
    : SearchboxHandler(std::move(pending_page_handler),
                       profile,
                       web_contents,
                       metrics_reporter),
      lens_searchbox_client_(lens_searchbox_client) {
  // Keep a reference to the OmniboxController instance owned by the OmniboxView
  // when the handler is being used in the context of the omnibox popup.
  // Otherwise, create own instance of OmniboxController. Either way, observe
  // the AutocompleteController instance owned by the OmniboxController.
  if (omnibox_controller) {
    controller_ = omnibox_controller;
  } else {
    owned_controller_ = std::make_unique<OmniboxController>(
        /*view=*/nullptr, std::make_unique<RealboxOmniboxClient>(
                              profile_, web_contents_, lens_searchbox_client));
    controller_ = owned_controller_.get();
  }

  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

RealboxHandler::~RealboxHandler() = default;

bool RealboxHandler::IsRemoteBound() const {
  return page_set_;
}

void RealboxHandler::AddObserver(OmniboxWebUIPopupChangeObserver* observer) {
  observers_.AddObserver(observer);
  observer->OnPopupElementSizeChanged(webui_size_);
}

void RealboxHandler::RemoveObserver(OmniboxWebUIPopupChangeObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool RealboxHandler::HasObserver(
    const OmniboxWebUIPopupChangeObserver* observer) const {
  return observers_.HasObserver(observer);
}

void RealboxHandler::SetPage(
    mojo::PendingRemote<searchbox::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
  page_set_ = page_.is_bound();

  // The client may have text waiting to be sent to the searchbox that it
  // couldn't do earlier since the page binding was not set. So now we let the
  // client know the binding is ready.
  if (lens_searchbox_client_) {
    lens_searchbox_client_->OnPageBound();
  }
}

void RealboxHandler::OnFocusChanged(bool focused) {
  if (focused) {
    edit_model()->OnSetFocus(false);
  } else {
    edit_model()->OnWillKillFocus();
    edit_model()->OnKillFocus();
  }
}

void RealboxHandler::QueryAutocomplete(const std::u16string& input,
                                       bool prevent_inline_autocomplete) {
  if (lens_searchbox_client_) {
    lens_searchbox_client_->OnTextModified();
  }

  // TODO(tommycli): We use the input being empty as a signal we are requesting
  // on-focus suggestions. It would be nice if we had a more explicit signal.
  bool is_on_focus = input.empty();

  // Early exit if a query is already in progress for on focus inputs.
  if (!autocomplete_controller()->done() && is_on_focus) {
    return;
  }

  // This will SetInputInProgress and consequently mark the input timer so that
  // Omnibox.TypingDuration will be logged correctly.
  edit_model()->SetUserText(input);

  // RealboxOmniboxClient::GetPageClassification() ignores the arguments.
  const auto page_classification =
      omnibox_controller()->client()->GetPageClassification(
          /*is_prefetch=*/false);
  AutocompleteInput autocomplete_input(
      input, page_classification, ChromeAutocompleteSchemeClassifier(profile_));
  autocomplete_input.set_current_url(controller_->client()->GetURL());
  autocomplete_input.set_focus_type(
      is_on_focus ? metrics::OmniboxFocusType::INTERACTION_FOCUS
                  : metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  autocomplete_input.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete);
  // Disable keyword matches as NTP realbox has no UI affordance for it.
  autocomplete_input.set_prefer_keyword(false);
  autocomplete_input.set_allow_exact_keyword_match(false);
  // Set the lens overlay suggest inputs, if available.
  if (std::optional<lens::proto::LensOverlaySuggestInputs> suggest_inputs =
          controller_->client()->GetLensOverlaySuggestInputs()) {
    autocomplete_input.set_lens_overlay_suggest_inputs(*suggest_inputs);
  }

  omnibox_controller()->StartAutocomplete(autocomplete_input);
}

void RealboxHandler::StopAutocomplete(bool clear_result) {
  omnibox_controller()->StopAutocomplete(clear_result);
}

void RealboxHandler::OpenAutocompleteMatch(uint8_t line,
                                           const GURL& url,
                                           bool are_matches_showing,
                                           uint8_t mouse_button,
                                           bool alt_key,
                                           bool ctrl_key,
                                           bool meta_key,
                                           bool shift_key) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  edit_model()->OpenSelection(OmniboxPopupSelection(line), timestamp,
                              disposition);
}

void RealboxHandler::OnNavigationLikely(
    uint8_t line,
    const GURL& url,
    omnibox::mojom::NavigationPredictor navigation_predictor) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  if (auto* search_prefetch_service =
          SearchPrefetchServiceFactory::GetForProfile(profile_)) {
    search_prefetch_service->OnNavigationLikely(
        line, *match, navigation_predictor, web_contents_);
  }
}

void RealboxHandler::OnThumbnailRemoved() {
  omnibox_controller()->client()->OnThumbnailRemoved();
}

void RealboxHandler::PopupElementSizeChanged(const gfx::Size& size) {
  webui_size_ = size;
  for (OmniboxWebUIPopupChangeObserver& observer : observers_) {
    observer.OnPopupElementSizeChanged(size);
  }
}

void RealboxHandler::DeleteAutocompleteMatch(uint8_t line, const GURL& url) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match || !match->SupportsDeletion()) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  omnibox_controller()->StopAutocomplete(/*clear_result=*/false);
  autocomplete_controller()->DeleteMatch(*match);
}

void RealboxHandler::ToggleSuggestionGroupIdVisibility(
    int32_t suggestion_group_id) {
  const auto& group_id = omnibox::GroupIdForNumber(suggestion_group_id);
  DCHECK_NE(omnibox::GROUP_INVALID, group_id);
  const bool current_visibility =
      omnibox_controller()->IsSuggestionGroupHidden(group_id);
  omnibox_controller()->SetSuggestionGroupHidden(group_id, !current_visibility);
}

void RealboxHandler::ExecuteAction(uint8_t line,
                                   uint8_t action_index,
                                   const GURL& url,
                                   base::TimeTicks match_selection_timestamp,
                                   uint8_t mouse_button,
                                   bool alt_key,
                                   bool ctrl_key,
                                   bool meta_key,
                                   bool shift_key) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  if (action_index >= match->actions.size()) {
    return;
  }
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  OmniboxPopupSelection selection(
      line, OmniboxPopupSelection::LineState::FOCUSED_BUTTON_ACTION,
      action_index);
  edit_model()->OpenSelection(selection, match_selection_timestamp,
                              disposition);
}

searchbox::mojom::SelectionLineState ConvertLineState(
    OmniboxPopupSelection::LineState state) {
  switch (state) {
    case OmniboxPopupSelection::LineState::FOCUSED_BUTTON_HEADER:
      return searchbox::mojom::SelectionLineState::kFocusedButtonHeader;
    case OmniboxPopupSelection::LineState::NORMAL:
      return searchbox::mojom::SelectionLineState::kNormal;
    case OmniboxPopupSelection::LineState::KEYWORD_MODE:
      return searchbox::mojom::SelectionLineState::kKeywordMode;
    case OmniboxPopupSelection::LineState::FOCUSED_BUTTON_ACTION:
      return searchbox::mojom::SelectionLineState::kFocusedButtonAction;
    case OmniboxPopupSelection::LineState::FOCUSED_BUTTON_REMOVE_SUGGESTION:
      return searchbox::mojom::SelectionLineState::
          kFocusedButtonRemoveSuggestion;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return searchbox::mojom::SelectionLineState::kNormal;
}

void RealboxHandler::SetInputText(const std::string& input_text) {
  page_->SetInputText(input_text);
}

void RealboxHandler::SetThumbnail(const std::string& thumbnail_url) {
  page_->SetThumbnail(thumbnail_url);
}

void RealboxHandler::UpdateSelection(OmniboxPopupSelection old_selection,
                                     OmniboxPopupSelection selection) {
  page_->UpdateSelection(
      searchbox::mojom::OmniboxPopupSelection::New(
          old_selection.line, ConvertLineState(old_selection.state),
          old_selection.action_index),
      searchbox::mojom::OmniboxPopupSelection::New(
          selection.line, ConvertLineState(selection.state),
          selection.action_index));
}

void RealboxHandler::SetLensSearchboxClientForTesting(
    LensSearchboxClient* lens_searchbox_client) {
  lens_searchbox_client_ = lens_searchbox_client;
  static_cast<RealboxOmniboxClient*>(omnibox_controller()->client())
      ->SetLensSearchboxClientForTesting(lens_searchbox_client);  // IN-TEST
}

OmniboxEditModel* RealboxHandler::edit_model() const {
  return omnibox_controller()->edit_model();
}

const AutocompleteMatch* RealboxHandler::GetMatchWithUrl(size_t index,
                                                         const GURL& url) {
  const AutocompleteResult& result = autocomplete_controller()->result();
  if (index >= result.size()) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return nullptr;
  }
  const AutocompleteMatch& match = result.match_at(index);
  if (match.destination_url != url) {
    // This can happen also, for the same reason. We could search the result
    // for the match with this URL, but there would be no guarantee that it's
    // the same match, so for this edge case we treat result mismatch as none.
    return nullptr;
  }
  return &match;
}
