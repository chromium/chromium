// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/password_requirements_service_factory.h"

#include <map>
#include <memory>
#include <string>

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

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

PasswordRequirementsServiceFactory::~PasswordRequirementsServiceFactory() =
    default;

std::unique_ptr<KeyedService>
PasswordRequirementsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord())
    return nullptr;

  return CreatePasswordRequirementsService(
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

}  // namespace password_manager
