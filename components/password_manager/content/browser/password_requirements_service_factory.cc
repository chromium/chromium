// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/password_requirements_service_factory.h"

#include <map>
#include <memory>
#include <string>

#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/password_requirements_spec_fetcher.h"
#include "components/autofill/core/browser/password_requirements_spec_fetcher_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "ui/base/ui_base_features.h"

namespace password_manager {

// static
PasswordRequirementsServiceFactory*
PasswordRequirementsServiceFactory::GetInstance() {
  return base::Singleton<PasswordRequirementsServiceFactory>::get();
}

// static
PasswordRequirementsService*
PasswordRequirementsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PasswordRequirementsService*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

PasswordRequirementsServiceFactory::PasswordRequirementsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordRequirementsServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

PasswordRequirementsServiceFactory::~PasswordRequirementsServiceFactory() {}

KeyedService* PasswordRequirementsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord())
    return nullptr;

  bool enable_domain_overrides = base::FeatureList::IsEnabled(
      features::kPasswordGenerationRequirementsDomainOverrides);

  VLOG(1)
      << "PasswordGenerationRequirementsDomainOverrides experiment enabled? "
      << enable_domain_overrides;

  if (!enable_domain_overrides)
    return new PasswordRequirementsService(nullptr);

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

  std::unique_ptr<autofill::PasswordRequirementsSpecFetcher> fetcher =
      std::make_unique<autofill::PasswordRequirementsSpecFetcherImpl>(
          content::BrowserContext::GetDefaultStoragePartition(context)
              ->GetURLLoaderFactoryForBrowserProcess(),
          version, prefix_length, timeout_in_ms);
  return new PasswordRequirementsService(std::move(fetcher));
}

}  // namespace password_manager
