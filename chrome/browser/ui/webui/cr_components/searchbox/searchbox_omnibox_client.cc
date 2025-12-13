// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"

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
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#endif

SearchboxOmniboxClient::SearchboxOmniboxClient(
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      scheme_classifier_(ChromeAutocompleteSchemeClassifier(profile)) {}

SearchboxOmniboxClient::~SearchboxOmniboxClient() = default;

std::unique_ptr<AutocompleteProviderClient>
SearchboxOmniboxClient::CreateAutocompleteProviderClient() {
  return std::make_unique<ChromeAutocompleteProviderClient>(profile_);
}

bool SearchboxOmniboxClient::IsPasteAndGoEnabled() const {
  return false;
}

SessionID SearchboxOmniboxClient::GetSessionID() const {
  return sessions::SessionTabHelper::IdForTab(web_contents_);
}

PrefService* SearchboxOmniboxClient::GetPrefs() {
  return profile_->GetPrefs();
}

const PrefService* SearchboxOmniboxClient::GetPrefs() const {
  return profile_->GetPrefs();
}

bookmarks::BookmarkModel* SearchboxOmniboxClient::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

AutocompleteControllerEmitter*
SearchboxOmniboxClient::GetAutocompleteControllerEmitter() {
  return AutocompleteControllerEmitterFactory::GetForBrowserContext(profile_);
}

TemplateURLService* SearchboxOmniboxClient::GetTemplateURLService() {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

const AutocompleteSchemeClassifier&
SearchboxOmniboxClient::GetSchemeClassifier() const {
  return scheme_classifier_;
}

AutocompleteClassifier* SearchboxOmniboxClient::GetAutocompleteClassifier() {
  return AutocompleteClassifierFactory::GetForProfile(profile_);
}

bool SearchboxOmniboxClient::ShouldDefaultTypedNavigationsToHttps() const {
  return false;
}

int SearchboxOmniboxClient::GetHttpsPortForTesting() const {
  return 0;
}

bool SearchboxOmniboxClient::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  return false;
}

gfx::Image SearchboxOmniboxClient::GetSizedIcon(
    const gfx::VectorIcon& vector_icon_type,
    SkColor vector_icon_color) const {
  return gfx::Image();
}

std::u16string SearchboxOmniboxClient::GetFormattedFullURL() const {
  return u"";
}

std::u16string SearchboxOmniboxClient::GetURLForDisplay() const {
  return u"";
}

GURL SearchboxOmniboxClient::GetNavigationEntryURL() const {
  return GURL();
}

security_state::SecurityLevel SearchboxOmniboxClient::GetSecurityLevel() const {
  return security_state::SecurityLevel::NONE;
}

net::CertStatus SearchboxOmniboxClient::GetCertStatus() const {
  return 0;
}

const gfx::VectorIcon& SearchboxOmniboxClient::GetVectorIcon() const {
  return vector_icon_;
}

gfx::Image SearchboxOmniboxClient::GetFaviconForPageUrl(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return gfx::Image();
}

void SearchboxOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  if (auto* search_prefetch_service =
          SearchPrefetchServiceFactory::GetForProfile(profile_)) {
    search_prefetch_service->OnURLOpenedFromOmnibox(log);
  }
  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->OnOmniboxOpenedUrl(*log);
}

void SearchboxOmniboxClient::OnAutocompleteAccept(
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
  web_contents_->OpenURL(
      content::OpenURLParams(destination_url, content::Referrer(), disposition,
                             transition, false),
      /*navigation_handle_callback=*/{});

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (AutocompleteMatch::IsSearchType(match.type)) {
    if (auto* telemetry_service =
            safe_browsing::ExtensionTelemetryService::Get(profile_)) {
      telemetry_service->OnOmniboxSearch(match);
    }
  }
#endif
}

base::WeakPtr<OmniboxClient> SearchboxOmniboxClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
