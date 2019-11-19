// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/autofill_log_router_factory.h"

#include "components/autofill/core/browser/logging/log_router.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace autofill {

// static
LogRouter* AutofillLogRouterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LogRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, /* create = */ true));
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
