// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_config.h"

#include "base/observer_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_utils.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace {

// The currently supported version of side search.
constexpr char kSideSearchVersion[] = "1";

// The key used to store the config user data on the profile.
constexpr char kSideSearchConfigKey[] = "side_search_config_key";

void ApplyDSEConfiguration(Profile* profile, SideSearchConfig& config) {
  // If DSE support is enabled we should only allow navigations in the side
  // panel under the following conditions:
  //   1. The default search engine supports side search.
  //   2. The `url` is a SRP belonging to the default search engine.
  config.SetShouldNavigateInSidePanelCallback(base::BindRepeating(
      [](Profile* profile, const GURL& url) {
        const auto* template_url_service =
            TemplateURLServiceFactory::GetForProfile(profile);

        return template_url_service &&
               template_url_service
                   ->IsSideSearchSupportedForDefaultSearchProvider() &&
               template_url_service
                   ->IsSearchResultsPageFromDefaultSearchProvider(url);
      },
      base::Unretained(profile)));

  // If DSE support is enabled we should only show the side panel under the
  // following conditions:
  //   1. The default search engine supports Side Search.
  //   2. The `url` is NOT a SRP belonging to the default search engine.
  //   3. The `url` is NOT the New Tab Page.
  config.SetCanShowSidePanelForURLCallback(base::BindRepeating(
      [](Profile* profile, const GURL& url) {
        const auto* template_url_service =
            TemplateURLServiceFactory::GetForProfile(profile);
        return template_url_service &&
               template_url_service
                   ->IsSideSearchSupportedForDefaultSearchProvider() &&
               !template_url_service
                    ->IsSearchResultsPageFromDefaultSearchProvider(url) &&
               !content::HasWebUIScheme(url);
      },
      base::Unretained(profile)));

  // Leverage the TemplateURLService for DSE urls.
  config.SetGenerateSideSearchURLCallback(base::BindRepeating(
      [](Profile* profile, const GURL& url) {
        const auto* template_url_service =
            TemplateURLServiceFactory::GetForProfile(profile);
        DCHECK(template_url_service
                   ->IsSideSearchSupportedForDefaultSearchProvider());
        return template_url_service
            ->GenerateSideSearchURLForDefaultSearchProvider(url,
                                                            kSideSearchVersion);
      },
      base::Unretained(profile)));
}

void ApplyGoogleSearchConfiguration(SideSearchConfig& config) {
  // If using the Google Search configuration only navigations in the side panel
  // when:
  //   1. The `url` is a Google Search SRP.
  config.SetShouldNavigateInSidePanelCallback(
      base::BindRepeating(google_util::IsGoogleSearchUrl));

  // If using the Google Search configuration only show the side panel when:
  //   1. The `url` is NOT a Google SRP.
  //   2. The `url` is NOT a Google home page.
  //   3. The `url` is NOT the New Tab Page.
  config.SetCanShowSidePanelForURLCallback(
      base::BindRepeating([](const GURL& url) {
        return !google_util::IsGoogleSearchUrl(url) &&
               !google_util::IsGoogleHomePageUrl(url) &&
               url.spec() != chrome::kChromeUINewTabURL;
      }));

  // Use the original predefined 'sidesearch' URL parameter for Google Search
  // support.
  config.SetGenerateSideSearchURLCallback(
      base::BindRepeating([](const GURL& url) {
        constexpr char kSideSearchQueryParam[] = "sidesearch";
        return net::AppendQueryParameter(url, kSideSearchQueryParam,
                                         kSideSearchVersion);
      }));
}

}  // namespace

SideSearchConfig::SideSearchConfig(Profile* profile) : profile_(profile) {
  // `template_url_service` may be null in tests.
  if (auto* template_url_service =
          TemplateURLServiceFactory::GetForProfile(profile_)) {
    template_url_service_observation_.Observe(template_url_service);

    // Call this initially in case the default URL has already been set.
    OnTemplateURLServiceChanged();
  }
  ApplyDSEConfiguration(profile_, *this);
}

SideSearchConfig::~SideSearchConfig() = default;

// static
SideSearchConfig* SideSearchConfig::Get(content::BrowserContext* context) {
  SideSearchConfig* data = static_cast<SideSearchConfig*>(
      context->GetUserData(kSideSearchConfigKey));
  if (!data) {
    auto new_data =
        std::make_unique<SideSearchConfig>(static_cast<Profile*>(context));
    data = new_data.get();
    context->SetUserData(kSideSearchConfigKey, std::move(new_data));
  }
  return data;
}

void SideSearchConfig::OnTemplateURLServiceChanged() {
  if (skip_on_template_url_changed_)
    return;

  const auto* default_template_url =
      TemplateURLServiceFactory::GetForProfile(profile_)
          ->GetDefaultSearchProvider();

  // If there is currently no default search engine set, but there was one set
  // previously, reset `default_template_url_id_` and propagate the change.
  if (!default_template_url &&
      default_template_url_id_ != kInvalidTemplateURLID) {
    default_template_url_id_ = kInvalidTemplateURLID;
    ResetStateAndNotifyConfigChanged();
    return;
  }

  // Propagate an update only if the current default search provider has
  // changed.
  if (!default_template_url ||
      default_template_url->id() == default_template_url_id_)
    return;
  default_template_url_id_ = default_template_url->id();
  ResetStateAndNotifyConfigChanged();
}

void SideSearchConfig::OnTemplateURLServiceShuttingDown() {
  template_url_service_observation_.Reset();
}

bool SideSearchConfig::ShouldNavigateInSidePanel(const GURL& url) {
  return should_navigate_in_side_panel_callback_.Run(url);
}

void SideSearchConfig::SetShouldNavigateInSidePanelCallback(
    URLTestConditionCallback callback) {
  should_navigate_in_side_panel_callback_ = std::move(callback);
}

bool SideSearchConfig::CanShowSidePanelForURL(const GURL& url) {
  return can_show_side_panel_for_url_callback_.Run(url);
}

void SideSearchConfig::SetCanShowSidePanelForURLCallback(
    URLTestConditionCallback callback) {
  can_show_side_panel_for_url_callback_ = std::move(callback);
}

GURL SideSearchConfig::GenerateSideSearchURL(const GURL& search_url) {
  return generate_side_search_url_callack_.Run(search_url);
}

void SideSearchConfig::SetGenerateSideSearchURLCallback(
    GenerateURLCallback callback) {
  generate_side_search_url_callack_ = std::move(callback);
}

void SideSearchConfig::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SideSearchConfig::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SideSearchConfig::ResetStateAndNotifyConfigChanged() {
  for (auto& observer : observers_)
    observer.OnSideSearchConfigChanged();
}

void SideSearchConfig::ApplyGoogleSearchConfigurationForTesting() {
  ApplyGoogleSearchConfiguration(*this);
}
