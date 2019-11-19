// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_requirements_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "components/password_manager/core/browser/generation/password_requirements_spec_fetcher_impl.h"
#include "components/password_manager/core/browser/generation/password_requirements_spec_printer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
constexpr size_t kCacheSizeForDomainKeyedSpecs = 200;
constexpr size_t kCacheSizeForSignatureKeyedSpecs = 500;
}  // namespace

using autofill::PasswordRequirementsSpec;
using autofill::PasswordRequirementsSpecFetcher;
using autofill::PasswordRequirementsSpecFetcherImpl;

namespace password_manager {

PasswordRequirementsService::PasswordRequirementsService(
    std::unique_ptr<PasswordRequirementsSpecFetcher> fetcher)
    : specs_for_domains_(kCacheSizeForDomainKeyedSpecs),
      specs_for_signatures_(kCacheSizeForSignatureKeyedSpecs),
      fetcher_(std::move(fetcher)) {}

PasswordRequirementsService::~PasswordRequirementsService() = default;

PasswordRequirementsSpec PasswordRequirementsService::GetSpec(
    const GURL& main_frame_domain,
    autofill::FormSignature form_signature,
    autofill::FieldSignature field_signature) {
  PasswordRequirementsSpec result;

  auto iter_by_signature = specs_for_signatures_.Get(
      std::make_pair(form_signature, field_signature));
  bool found_item_by_signature =
      iter_by_signature != specs_for_signatures_.end();
  if (found_item_by_signature) {
    result = iter_by_signature->second;
  }

  auto iter_by_domain = specs_for_domains_.Get(main_frame_domain);
  if (iter_by_domain != specs_for_domains_.end()) {
    const PasswordRequirementsSpec& spec = iter_by_domain->second;
    if (!found_item_by_signature) {
      // If nothing was found by signature, |spec| can be adopted.
      result = spec;
    } else if (spec.has_priority() && (!result.has_priority() ||
                                       spec.priority() > result.priority())) {
      // If something was found by signature, override with |spec| only in case
      // the priority of |spec| exceeds the priority of the data found by
      // signature.
      result = spec;
    }
  }

  VLOG(1) << "PasswordRequirementsService::GetSpec(" << main_frame_domain
          << ", " << form_signature << ", " << field_signature
          << ") = " << result;

  return result;
}

void PasswordRequirementsService::PrefetchSpec(const GURL& main_frame_domain) {
  VLOG(1) << "PasswordRequirementsService::PrefetchSpec(" << main_frame_domain
          << ")";

  if (!fetcher_) {
    VLOG(1) << "PasswordRequirementsService::PrefetchSpec has no fetcher";
    return;
  }

  // No need to fetch the same data multiple times.
  if (specs_for_domains_.Get(main_frame_domain) != specs_for_domains_.end()) {
    VLOG(1) << "PasswordRequirementsService::PrefetchSpec has an entry already";
    return;
  }

  // Using base::Unretained(this) is safe here because the
  // PasswordRequirementsService owns fetcher_. If |this| is deleted, so is
  // the |fetcher_|, and no callback can happen.
  fetcher_->Fetch(
      main_frame_domain,
      base::BindOnce(&PasswordRequirementsService::OnFetchedRequirements,
                     base::Unretained(this), main_frame_domain));
}

void PasswordRequirementsService::OnFetchedRequirements(
    const GURL& main_frame_domain,
    const PasswordRequirementsSpec& spec) {
  VLOG(1) << "PasswordRequirementsService::OnFetchedRequirements("
          << main_frame_domain << ", " << spec << ")";
  specs_for_domains_.Put(main_frame_domain, spec);
}

void PasswordRequirementsService::AddSpec(
    const GURL& main_frame_domain,
    autofill::FormSignature form_signature,
    autofill::FieldSignature field_signature,
    const PasswordRequirementsSpec& spec) {
  VLOG(1) << "PasswordRequirementsService::AddSpec(" << form_signature << ", "
          << field_signature << ", " << spec << ")";
  specs_for_signatures_.Put(std::make_pair(form_signature, field_signature),
                            spec);

  auto iter_by_domain = specs_for_domains_.Get(main_frame_domain);
  if (iter_by_domain != specs_for_domains_.end()) {
    PasswordRequirementsSpec& existing_spec = iter_by_domain->second;
    if (existing_spec.priority() > spec.priority())
      return;
  }
  specs_for_domains_.Put(main_frame_domain, spec);
}

void PasswordRequirementsService::ClearDataForTestingImpl() {
  specs_for_domains_.Clear();
  specs_for_signatures_.Clear();
}

std::unique_ptr<PasswordRequirementsService> CreatePasswordRequirementsService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // Default parameters.
  int version = 1;
  int prefix_length = 0;
  int timeout_in_ms = 5000;

  // Override defaults with parameters from field trial if defined.
  std::map<std::string, std::string> field_trial_params;
  base::GetFieldTrialParams(features::kGenerationRequirementsFieldTrial,
                            &field_trial_params);
  // base::StringToInt modifies the target even if it fails to parse the input.
  // |tmp| is used to protect the default values above.
  int tmp = 0;
  if (base::StringToInt(
          field_trial_params[features::kGenerationRequirementsVersion], &tmp)) {
    version = tmp;
  }
  if (base::StringToInt(
          field_trial_params[features::kGenerationRequirementsPrefixLength],
          &tmp)) {
    prefix_length = tmp;
  }
  if (base::StringToInt(
          field_trial_params[features::kGenerationRequirementsTimeout], &tmp)) {
    timeout_in_ms = tmp;
  }

  VLOG(1) << "PasswordGenerationRequirements parameters: " << version << ", "
          << prefix_length << ", " << timeout_in_ms << " ms";

  std::unique_ptr<PasswordRequirementsSpecFetcher> fetcher =
      std::make_unique<PasswordRequirementsSpecFetcherImpl>(
          url_loader_factory, version, prefix_length, timeout_in_ms);
  return std::make_unique<PasswordRequirementsService>(std::move(fetcher));
}

}  // namespace password_manager
