// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/autofill_log_router_factory.h"

#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace autofill {

// static
LogRouter* AutofillLogRouterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  LogRouter* log_router = static_cast<LogRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, /* create = */ true));
  if (base::FeatureList::IsEnabled(features::test::kAutofillLogToTerminal)) {
    log_router->LogToTerminal();
  }
  return log_router;
}

// static
AutofillLogRouterFactory* AutofillLogRouterFactory::GetInstance() {
  return base::Singleton<AutofillLogRouterFactory>::get();
}

AutofillLogRouterFactory::AutofillLogRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "AutofillInternalsService",
          BrowserContextDependencyManager::GetInstance()) {}

AutofillLogRouterFactory::~AutofillLogRouterFactory() = default;

KeyedService* AutofillLogRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* /* context */) const {
  return new LogRouter();
}

}  // namespace autofill
