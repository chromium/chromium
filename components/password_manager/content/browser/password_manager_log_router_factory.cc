// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/password_manager_log_router_factory.h"

#include "components/autofill/core/browser/logging/log_router.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/features/password_features.h"

namespace password_manager {

using autofill::LogRouter;

// static
LogRouter* PasswordManagerLogRouterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  LogRouter* log_router = static_cast<LogRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, /* create = */ true));
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManagerLogToTerminal)) {
    log_router->LogToTerminal();
  }
  return log_router;
}

// static
PasswordManagerLogRouterFactory*
PasswordManagerLogRouterFactory::GetInstance() {
  return base::Singleton<PasswordManagerLogRouterFactory>::get();
}

PasswordManagerLogRouterFactory::PasswordManagerLogRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordManagerInternalsService",
          BrowserContextDependencyManager::GetInstance()) {}

PasswordManagerLogRouterFactory::~PasswordManagerLogRouterFactory() = default;

std::unique_ptr<KeyedService>
PasswordManagerLogRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* /* context */) const {
  return std::make_unique<LogRouter>();
}

}  // namespace password_manager
