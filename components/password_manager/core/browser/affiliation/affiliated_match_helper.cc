// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace password_manager {

namespace {

using AffiliatedRealms =
    base::StrongAlias<class AffiliatedRealmsTag, std::vector<std::string>>;
using GroupedRealms =
    base::StrongAlias<class GroupedRealmsTag, std::vector<std::string>>;

bool IsValidAndroidCredential(PasswordForm* form) {
  return form->scheme == PasswordForm::Scheme::kHtml &&
         IsValidAndroidFacetURI(form->signon_realm);
}

std::vector<std::string> GetRealmsFromFacets(const FacetURI& original_facet_uri,
                                             const std::vector<Facet>& facets) {
  std::vector<std::string> realms;
  realms.reserve(facets.size());
  for (const Facet& affiliated_facet : facets) {
    if (affiliated_facet.uri == original_facet_uri) {
      continue;
    }
    if (affiliated_facet.uri.IsValidAndroidFacetURI()) {
      // Facet URIs have no trailing slash, whereas realms do.
      realms.push_back(affiliated_facet.uri.canonical_spec() + "/");
    }

#if !BUILDFLAG(IS_ANDROID)
    // All platforms except Android supports filling across affiliated websites.
    if (affiliated_facet.uri.IsValidWebFacetURI()) {
      CHECK(!base::EndsWith(affiliated_facet.uri.canonical_spec(), "/"));
      // Facet URIs have no trailing slash, whereas realms do.
      realms.push_back(affiliated_facet.uri.canonical_spec() + "/");
    }
#endif
  }
  return realms;
}

AffiliatedRealms ProcessAffiliatedFacets(const FacetURI& original_facet_uri,
                                         const AffiliatedFacets& results,
                                         bool success) {
  if (!success) {
    return {};
  }
  return AffiliatedRealms(GetRealmsFromFacets(original_facet_uri, results));
}

GroupedRealms ProcessGroupedFacets(const FacetURI& original_facet_uri,
                                   const std::vector<GroupedFacets>& results) {
  // GetGroupingInfo() returns a group matches for each facet.
  // Asking for only one facet means that it would return only one group (that
  // includes requested realm itself). Therefore, resulting number of groups
  // not bigger than 1 and not smaller than 1.
  CHECK_EQ(1U, results.size());
  return GroupedRealms(
      GetRealmsFromFacets(original_facet_uri, results[0].facets));
}

void ProcessAffiliationAndGroupResponse(
    AffiliatedMatchHelper::AffiliatedRealmsCallback result_callback,
    std::vector<absl::variant<AffiliatedRealms, GroupedRealms>> results) {
  CHECK(!results.empty());

  AffiliatedRealms affiliated_realms;
  GroupedRealms grouped_realms;

  for (auto& result : results) {
    if (absl::holds_alternative<AffiliatedRealms>(result)) {
      affiliated_realms = absl::get<AffiliatedRealms>(std::move(result));
    } else {
      grouped_realms = absl::get<GroupedRealms>(std::move(result));
    }
  }

  std::move(result_callback)
      .Run(std::move(affiliated_realms).value(),
           std::move(grouped_realms).value());
}

}  // namespace

AffiliatedMatchHelper::AffiliatedMatchHelper(
    AffiliationService* affiliation_service)
    : affiliation_service_(affiliation_service) {
  DCHECK(affiliation_service_);
}

AffiliatedMatchHelper::~AffiliatedMatchHelper() = default;

void AffiliatedMatchHelper::GetAffiliatedAndGroupedRealms(
    const PasswordFormDigest& observed_form,
    AffiliatedRealmsCallback result_callback) {
  if (!IsValidWebCredential(observed_form)) {
    std::move(result_callback).Run({}, {});
    return;
  }

  const int kCallsNumber = 2;
  auto barrier_callback =
      base::BarrierCallback<absl::variant<AffiliatedRealms, GroupedRealms>>(
          kCallsNumber, base::BindOnce(&ProcessAffiliationAndGroupResponse,
                                       std::move(result_callback)));

  FacetURI facet_uri(
      FacetURI::FromPotentiallyInvalidSpec(observed_form.signon_realm));
  affiliation_service_->GetAffiliationsAndBranding(
      facet_uri, AffiliationService::StrategyOnCacheMiss::FAIL,
      base::BindOnce(&ProcessAffiliatedFacets, facet_uri)
          .Then(barrier_callback));

  affiliation_service_->GetGroupingInfo(
      {facet_uri}, base::BindOnce(&ProcessGroupedFacets, facet_uri)
                       .Then(std::move(barrier_callback)));
}

void AffiliatedMatchHelper::InjectAffiliationAndBrandingInformation(
    std::vector<std::unique_ptr<PasswordForm>> forms,
    base::OnceCallback<void(LoginsResultOrError)> result_callback) {
  std::vector<PasswordForm*> android_credentials;
  for (const auto& form : forms) {
    if (IsValidAndroidCredential(form.get())) {
      android_credentials.push_back(form.get());
    }
  }

  // Create a closure that runs after affiliations are injected and
  // CompleteInjectAffiliationAndBrandingInformation is called for
  // all forms in |android_credentials|.
  base::OnceClosure on_get_all_realms(
      base::BindOnce(std::move(result_callback), std::move(forms)));
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      android_credentials.size(), std::move(on_get_all_realms));

  for (auto* form : android_credentials) {
    // |forms| are not destroyed until the |barrier_closure| executes,
    // making it safe to use base::Unretained(form) below.
    affiliation_service_->GetAffiliationsAndBranding(
        FacetURI::FromPotentiallyInvalidSpec(form->signon_realm),
        AffiliationService::StrategyOnCacheMiss::FAIL,
        base::BindOnce(&AffiliatedMatchHelper::
                           CompleteInjectAffiliationAndBrandingInformation,
                       weak_ptr_factory_.GetWeakPtr(), base::Unretained(form),
                       barrier_closure));
  }
}

void AffiliatedMatchHelper::GetPSLExtensions(
    base::OnceCallback<void(const base::flat_set<std::string>&)> callback) {
  if (psl_extensions_.has_value()) {
    std::move(callback).Run(psl_extensions_.value());
    return;
  }

  psl_extensions_callbacks_.push_back(std::move(callback));

  if (psl_extensions_callbacks_.size() > 1) {
    return;
  }

  affiliation_service_->GetPSLExtensions(
      base::BindOnce(&AffiliatedMatchHelper::OnPSLExtensionsReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

// static
bool AffiliatedMatchHelper::IsValidWebCredential(
    const PasswordFormDigest& form) {
  FacetURI facet_uri(FacetURI::FromPotentiallyInvalidSpec(form.signon_realm));
  return form.scheme == PasswordForm::Scheme::kHtml &&
         facet_uri.IsValidWebFacetURI();
}

void AffiliatedMatchHelper::CompleteInjectAffiliationAndBrandingInformation(
    PasswordForm* form,
    base::OnceClosure barrier_closure,
    const AffiliatedFacets& results,
    bool success) {
  const FacetURI facet_uri(
      FacetURI::FromPotentiallyInvalidSpec(form->signon_realm));

  // Facet can also be web URI, in this case we do nothing.
  if (!success || !facet_uri.IsValidAndroidFacetURI()) {
    std::move(barrier_closure).Run();
    return;
  }

  // Inject branding information into the form (e.g. the Play Store name and
  // icon URL). We expect to always find a matching facet URI in the results.
  auto facet = base::ranges::find(results, facet_uri, &Facet::uri);

  DCHECK(facet != results.end());
  form->app_display_name = facet->branding_info.name;
  form->app_icon_url = facet->branding_info.icon_url;

  // Inject the affiliated web realm into the form, if available. In case
  // multiple web realms are available, this will always choose the first
  // available web realm for injection.
  auto affiliated_facet =
      base::ranges::find_if(results, [](const Facet& affiliated_facet) {
        return affiliated_facet.uri.IsValidWebFacetURI();
      });
  if (affiliated_facet != results.end()) {
    form->affiliated_web_realm = affiliated_facet->uri.canonical_spec() + "/";
  }

  std::move(barrier_closure).Run();
}

void AffiliatedMatchHelper::OnPSLExtensionsReceived(
    std::vector<std::string> psl_extensions) {
  psl_extensions_ = base::flat_set<std::string>(
      std::make_move_iterator(psl_extensions.begin()),
      std::make_move_iterator(psl_extensions.end()));

  for (auto& callback : std::exchange(psl_extensions_callbacks_, {})) {
    std::move(callback).Run(psl_extensions_.value());
  }
}

}  // namespace password_manager
