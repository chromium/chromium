// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/lens_searchbox_handler.h"

#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/omnibox/autocomplete_controller_emitter_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "components/lens/lens_features.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

namespace {
// Interface that allows the Lens searchbox to interact with its embedder
// (i.e., LensSearchboxController).
class LensOmniboxClient : public OmniboxClient {
 public:
  LensOmniboxClient(Profile* profile,
                    content::WebContents* web_contents,
                    LensSearchboxClient* lens_searchbox_client);
  ~LensOmniboxClient() override;

  // OmniboxClient:
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;
  PrefService* GetPrefs() override;
  SessionID GetSessionID() const override;
  const GURL& GetURL() const override;
  const PrefService* GetPrefs() const override;
  AutocompleteControllerEmitter* GetAutocompleteControllerEmitter() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  bool ShouldDefaultTypedNavigationsToHttps() const override;
  int GetHttpsPortForTesting() const override;
  bool IsUsingFakeHttpsForHttpsUpgradeTesting() const override;
  std::u16string GetFormattedFullURL() const override;
  std::u16string GetURLForDisplay() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
  GURL GetNavigationEntryURL() const override;
  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;
  void OnThumbnailRemoved() override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  net::CertStatus GetCertStatus() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
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
      const AutocompleteMatch& alternative_nav_match) override;
  base::WeakPtr<OmniboxClient> AsWeakPtr() override;

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<LensSearchboxClient> lens_searchbox_client_;
  ChromeAutocompleteSchemeClassifier scheme_classifier_;
  // This is unused, but needed for `GetVectorIcon()`.
  gfx::VectorIcon vector_icon_{nullptr, 0u, ""};
  base::WeakPtrFactory<LensOmniboxClient> weak_factory_{this};
};
LensOmniboxClient::LensOmniboxClient(Profile* profile,
                                     content::WebContents* web_contents,
                                     LensSearchboxClient* lens_searchbox_client)
    : profile_(profile),
      web_contents_(web_contents),
      lens_searchbox_client_(lens_searchbox_client),
      scheme_classifier_(ChromeAutocompleteSchemeClassifier(profile)) {}

LensOmniboxClient::~LensOmniboxClient() = default;

std::unique_ptr<AutocompleteProviderClient>
LensOmniboxClient::CreateAutocompleteProviderClient() {
  return std::make_unique<ChromeAutocompleteProviderClient>(profile_);
}

PrefService* LensOmniboxClient::GetPrefs() {
  return profile_->GetPrefs();
}

SessionID LensOmniboxClient::GetSessionID() const {
  return lens_searchbox_client_->GetTabId();
}

const GURL& LensOmniboxClient::GetURL() const {
  return lens_searchbox_client_->GetPageURL();
}

const PrefService* LensOmniboxClient::GetPrefs() const {
  return profile_->GetPrefs();
}

AutocompleteControllerEmitter*
LensOmniboxClient::GetAutocompleteControllerEmitter() {
  return AutocompleteControllerEmitterFactory::GetForBrowserContext(profile_);
}

TemplateURLService* LensOmniboxClient::GetTemplateURLService() {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

const AutocompleteSchemeClassifier& LensOmniboxClient::GetSchemeClassifier()
    const {
  return scheme_classifier_;
}

AutocompleteClassifier* LensOmniboxClient::GetAutocompleteClassifier() {
  return AutocompleteClassifierFactory::GetForProfile(profile_);
}

bool LensOmniboxClient::ShouldDefaultTypedNavigationsToHttps() const {
  return false;
}

int LensOmniboxClient::GetHttpsPortForTesting() const {
  return 0;
}

bool LensOmniboxClient::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  return false;
}

std::u16string LensOmniboxClient::GetFormattedFullURL() const {
  return u"";
}

std::optional<lens::proto::LensOverlaySuggestInputs>
LensOmniboxClient::GetLensOverlaySuggestInputs() const {
  return lens_searchbox_client_->GetLensSuggestInputs();
}

void LensOmniboxClient::OnThumbnailRemoved() {
  lens_searchbox_client_->OnThumbnailRemoved();
}

std::u16string LensOmniboxClient::GetURLForDisplay() const {
  return u"";
}

metrics::OmniboxEventProto::PageClassification
LensOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return lens_searchbox_client_->GetPageClassification();
}

GURL LensOmniboxClient::GetNavigationEntryURL() const {
  return GURL();
}

security_state::SecurityLevel LensOmniboxClient::GetSecurityLevel() const {
  return security_state::SecurityLevel::NONE;
}

net::CertStatus LensOmniboxClient::GetCertStatus() const {
  return 0;
}

const gfx::VectorIcon& LensOmniboxClient::GetVectorIcon() const {
  return vector_icon_;
}

void LensOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  if (auto* search_prefetch_service =
          SearchPrefetchServiceFactory::GetForProfile(profile_)) {
    search_prefetch_service->OnURLOpenedFromOmnibox(log);
  }
  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->OnOmniboxOpenedUrl(*log);
}

void LensOmniboxClient::OnAutocompleteAccept(
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
    const AutocompleteMatch& alternative_nav_match) {
  lens_searchbox_client_->OnSuggestionAccepted(
      destination_url, match.type,
      match.subtypes.contains(omnibox::SUBTYPE_ZERO_PREFIX));
}

base::WeakPtr<OmniboxClient> LensOmniboxClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace

LensSearchboxHandler::LensSearchboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents,
    MetricsReporter* metrics_reporter,
    LensSearchboxClient* lens_searchbox_client)
    : SearchboxHandler(std::move(pending_page_handler),
                       profile,
                       web_contents,
                       metrics_reporter),
      lens_searchbox_client_(lens_searchbox_client) {
  owned_controller_ = std::make_unique<OmniboxController>(
      /*view=*/nullptr,
      std::make_unique<LensOmniboxClient>(profile_, web_contents_,
                                          lens_searchbox_client),
      lens::features::GetLensSearchboxAutocompleteTimeout());
  controller_ = owned_controller_.get();

  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

LensSearchboxHandler::~LensSearchboxHandler() = default;

void LensSearchboxHandler::SetPage(
    mojo::PendingRemote<searchbox::mojom::Page> pending_page) {
  SearchboxHandler::SetPage(std::move(pending_page));

  // The client may have text waiting to be sent to the searchbox that it
  // couldn't do earlier since the page binding was not set. So now we let the
  // client know the binding is ready.
  lens_searchbox_client_->OnPageBound();
}

void LensSearchboxHandler::OnFocusChanged(bool focused) {
  SearchboxHandler::OnFocusChanged(focused);
  lens_searchbox_client_->OnFocusChanged(focused);
}

void LensSearchboxHandler::QueryAutocomplete(const std::u16string& input,
                                             bool prevent_inline_autocomplete) {
  lens_searchbox_client_->OnTextModified();

  SearchboxHandler::QueryAutocomplete(input, prevent_inline_autocomplete);
}

void LensSearchboxHandler::SetInputText(const std::string& input_text) {
  page_->SetInputText(input_text);
}

void LensSearchboxHandler::SetThumbnail(const std::string& thumbnail_url,
                                        bool is_deletable) {
  page_->SetThumbnail(thumbnail_url, is_deletable);
}

void LensSearchboxHandler::OnThumbnailRemoved() {
  omnibox_controller()->client()->OnThumbnailRemoved();
}

void LensSearchboxHandler::OnAutocompleteStopTimerTriggered(
    const AutocompleteInput& input) {
  // Only notify the lens controller when autocomplete stop timer is triggered
  // for zero suggest inputs.
  if (input.IsZeroSuggest() && autocomplete_controller()->done()) {
    lens_searchbox_client_->ShowGhostLoaderErrorState();
  }
}

void LensSearchboxHandler::OnResultChanged(AutocompleteController* controller,
                                           bool default_match_changed) {
  SearchboxHandler::OnResultChanged(controller, default_match_changed);
  // Show the ghost loader error state if the result is empty on the last
  // async pass of the autocomplete controller (there will not be anymore
  // updates). controller->done() itself is not a sufficient check since it
  // takes into account kStop update types which occurs when a user unfocuses
  // the searchbox, and the error state should not be shown in this case.
  if (controller->done() &&
      controller->last_update_type() ==
          AutocompleteController::UpdateType::kLastAsyncPass &&
      controller->result().empty()) {
    lens_searchbox_client_->ShowGhostLoaderErrorState();
  }

  if (controller->input().IsZeroSuggest() && !controller->result().empty()) {
    lens_searchbox_client_->OnZeroSuggestShown();
  }
}
