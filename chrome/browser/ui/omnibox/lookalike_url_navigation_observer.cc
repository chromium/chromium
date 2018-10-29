// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/lookalike_url_navigation_observer.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "chrome/common/chrome_features.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/url_formatter/idn_spoof_checker.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

const base::FeatureParam<std::string> kMetricsOnly{
    &features::kLookalikeUrlNavigationSuggestions, "metrics_only", ""};

void RecordEvent(
    LookalikeUrlNavigationObserver::NavigationSuggestionEvent event) {
  UMA_HISTOGRAM_ENUMERATION(LookalikeUrlNavigationObserver::kHistogramName,
                            event);
}

bool SkeletonsMatch(const url_formatter::Skeletons& skeletons1,
                    const url_formatter::Skeletons& skeletons2) {
  DCHECK(!skeletons1.empty());
  DCHECK(!skeletons2.empty());
  for (const std::string& skeleton1 : skeletons1) {
    if (base::ContainsKey(skeletons2, skeleton1))
      return true;
  }
  return false;
}

}  // namespace

// static
const char LookalikeUrlNavigationObserver::kHistogramName[] =
    "NavigationSuggestion.Event";

LookalikeUrlNavigationObserver::LookalikeUrlNavigationObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

LookalikeUrlNavigationObserver::~LookalikeUrlNavigationObserver() {}

void LookalikeUrlNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore subframe and same document navigations.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  // If the navigation was not committed, it means either the page was a
  // download or error 204/205, or the navigation never left the previous
  // URL. Basically, this isn't a problem since we stayed at the existing URL.
  if (!navigation_handle->HasCommitted())
    return;

  const GURL url = navigation_handle->GetURL();

  // If the user has engaged with this site, don't show any lookalike
  // navigation suggestions.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  SiteEngagementService* service = SiteEngagementService::Get(profile);
  if (service->IsEngagementAtLeast(url, blink::mojom::EngagementLevel::LOW))
    return;

  const base::StringPiece host = url.host_piece();
  url_formatter::IDNConversionResult result =
      url_formatter::IDNToUnicodeWithDetails(host);
  if (!result.has_idn_component)
    return;

  std::string matched_domain;
  MatchType match_type;
  if (result.matching_top_domain.empty()) {
    matched_domain = GetMatchingSiteEngagementDomain(service, url);
    if (matched_domain.empty())
      return;
    RecordEvent(NavigationSuggestionEvent::kMatchSiteEngagement);
    match_type = MatchType::kSiteEngagement;
  } else {
    matched_domain = result.matching_top_domain;
    RecordEvent(NavigationSuggestionEvent::kMatchTopSite);
    match_type = MatchType::kTopSite;
  }

  DCHECK(!matched_domain.empty());

  GURL::Replacements replace_host;
  replace_host.SetHostStr(matched_domain);
  const GURL suggested_url = url.ReplaceComponents(replace_host);

  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  CHECK(ukm_recorder);
  ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents());
  ukm::builders::LookalikeUrl_NavigationSuggestion(source_id)
      .SetMatchType(static_cast<int>(match_type))
      .Record(ukm_recorder);

  if (kMetricsOnly.Get().empty()) {
    RecordEvent(NavigationSuggestionEvent::kInfobarShown);
    AlternateNavInfoBarDelegate::CreateForLookalikeUrlNavigation(
        web_contents(), base::UTF8ToUTF16(matched_domain), suggested_url, url,
        base::BindOnce(RecordEvent, NavigationSuggestionEvent::kLinkClicked));
  }
}

// static
void LookalikeUrlNavigationObserver::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<LookalikeUrlNavigationObserver>(web_contents));
  }
}

std::string LookalikeUrlNavigationObserver::GetMatchingSiteEngagementDomain(
    SiteEngagementService* service,
    const GURL& url) {
  // Compute skeletons using eTLD+1.
  const std::string domain_and_registry =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // eTLD+1 can be empty for private domains.
  if (domain_and_registry.empty())
    return std::string();

  url_formatter::IDNConversionResult result =
      url_formatter::IDNToUnicodeWithDetails(domain_and_registry);
  DCHECK(result.has_idn_component);
  const url_formatter::Skeletons navigated_skeletons =
      url_formatter::GetSkeletons(result.result);

  std::map<std::string, url_formatter::Skeletons>
      domain_and_registry_to_skeleton;
  std::vector<mojom::SiteEngagementDetails> engagement_details =
      service->GetAllDetails();
  for (const auto& detail : engagement_details) {
    // Ignore sites with an engagement score lower than LOW.
    if (!service->IsEngagementAtLeast(detail.origin,
                                      blink::mojom::EngagementLevel::LOW))
      continue;

    // If the user has engaged with eTLD+1 of this site, don't show any
    // lookalike navigation suggestions.
    const std::string engaged_domain_and_registry =
        net::registry_controlled_domains::GetDomainAndRegistry(
            detail.origin,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    // eTLD+1 can be empty for private domains.
    if (engaged_domain_and_registry.empty())
      continue;

    if (domain_and_registry == engaged_domain_and_registry)
      return std::string();

    // Multiple domains can map to the same eTLD+1, avoid skeleton generation
    // when possible.
    auto it = domain_and_registry_to_skeleton.find(engaged_domain_and_registry);
    url_formatter::Skeletons skeletons;
    if (it == domain_and_registry_to_skeleton.end()) {
      // Engaged site can be IDN. Decode as unicode and compute the skeleton
      // from that. At this point, top domain checks have already been done, so
      // if the site is IDN, it'll always be decoded as unicode (i.e. IDN spoof
      // checker will not find a matching top domain and fall back to punycode
      // for it).
      url_formatter::IDNConversionResult conversion_result =
          url_formatter::IDNToUnicodeWithDetails(engaged_domain_and_registry);

      skeletons = url_formatter::GetSkeletons(conversion_result.result);
      domain_and_registry_to_skeleton[engaged_domain_and_registry] = skeletons;
    } else {
      skeletons = it->second;
    }

    if (SkeletonsMatch(navigated_skeletons, skeletons))
      return detail.origin.host();
  }
  return std::string();
}
