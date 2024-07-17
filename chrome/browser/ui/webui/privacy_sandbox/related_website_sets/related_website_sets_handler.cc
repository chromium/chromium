// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/related_website_sets/related_website_sets_handler.h"

#include "base/check.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using related_website_sets::mojom::Member;
using related_website_sets::mojom::MemberPtr;
using related_website_sets::mojom::RelatedWebsiteSet;
using related_website_sets::mojom::RelatedWebsiteSetPtr;
using related_website_sets::mojom::SiteType;

namespace {

constexpr char kNoServiceError[] = "Service error";
constexpr char kServiceNotReadyError[] =
    "Backend error, service not ready";

SiteType GetSiteType(const net::SiteType type) {
  switch (type) {
    case net::SiteType::kPrimary:
      return SiteType::kPrimary;
    case net::SiteType::kAssociated:
      return SiteType::kAssociated;
    case net::SiteType::kService:
      return SiteType::kService;
  }
}

// Requires service to be fully initialized
std::vector<RelatedWebsiteSetPtr> ComputeRelatedWebsiteSetsInfo(
    base::WeakPtr<first_party_sets::FirstPartySetsPolicyService> service) {
  // Collect all effective RWS entries for this profile.
  CHECK(service && service->is_ready());
  std::map<std::string, std::map<net::SiteType, std::set<std::string>>> sets;
  service->ForEachEffectiveSetEntry(
      [&sets](const net::SchemefulSite& site,
              const net::FirstPartySetEntry& entry) {
        sets[entry.primary().Serialize()][entry.site_type()].insert(
            site.Serialize());
        return true;
      });

  std::vector<RelatedWebsiteSetPtr> info_list;
  info_list.reserve(sets.size());

  for (const auto& [primary_site, set] : sets) {
    std::vector<MemberPtr> members;
    for (const auto& [site_type, sites] : set) {
      for (const auto& site : sites) {
        members.emplace_back(Member::New(site, GetSiteType(site_type)));
      }
    }
    info_list.emplace_back(RelatedWebsiteSet::New(
        primary_site, std::move(members),
        service->IsSiteInManagedSet(
            net::SchemefulSite::Deserialize(primary_site))));
  }
  return info_list;
}

}  // namespace

RelatedWebsiteSetsHandler::RelatedWebsiteSetsHandler(
    content::WebUI* web_ui,
    mojo::PendingReceiver<
        related_website_sets::mojom::RelatedWebsiteSetsPageHandler> receiver)
    : web_ui_(web_ui), receiver_(this, std::move(receiver)) {}

RelatedWebsiteSetsHandler::~RelatedWebsiteSetsHandler() = default;

void RelatedWebsiteSetsHandler::GetRelatedWebsiteSets(
    GetRelatedWebsiteSetsCallback callback) {
  auto* related_website_sets_service =
      first_party_sets::FirstPartySetsPolicyServiceFactory::
          GetForBrowserContext(Profile::FromWebUI(web_ui_));

  if (!related_website_sets_service) {
    std::move(callback).Run(
        {related_website_sets::mojom::GetRelatedWebsiteSetsResponse::
             NewErrorMessage(kNoServiceError)});
    return;
  }

  if (!related_website_sets_service->is_ready()) {
    std::move(callback).Run(
        {related_website_sets::mojom::GetRelatedWebsiteSetsResponse::
             NewErrorMessage(kServiceNotReadyError)});
    return;
  }

  std::vector<RelatedWebsiteSetPtr> info_list =
      ComputeRelatedWebsiteSetsInfo(related_website_sets_service->GetWeakPtr());

  std::move(callback).Run(
      related_website_sets::mojom::GetRelatedWebsiteSetsResponse::
          NewRelatedWebsiteSets(std::move(info_list)));
}
