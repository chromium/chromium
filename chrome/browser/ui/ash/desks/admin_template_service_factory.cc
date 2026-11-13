// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks/admin_template_service_factory.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
desks_storage::AdminTemplateService*
AdminTemplateServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  if (!context) {
    return nullptr;
  }
  return static_cast<desks_storage::AdminTemplateService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
AdminTemplateServiceFactory* AdminTemplateServiceFactory::GetInstance() {
  static base::NoDestructor<AdminTemplateServiceFactory> instance;
  return instance.get();
}

AdminTemplateServiceFactory::AdminTemplateServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AdminTemplateService",
          BrowserContextDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
AdminTemplateServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  const AccountId* account_id = ash::AnnotatedAccountId::Get(context);
  return std::make_unique<desks_storage::AdminTemplateService>(
      context->GetPath(), account_id ? *account_id : EmptyAccountId(),
      user_prefs::UserPrefs::Get(context));
}

}  // namespace ash
