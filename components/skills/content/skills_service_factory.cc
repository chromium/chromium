// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/content/skills_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/skills/internal/skills_service_impl.h"

namespace skills {

SkillsService* SkillsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SkillsService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

SkillsServiceFactory* SkillsServiceFactory::GetInstance() {
  static base::NoDestructor<SkillsServiceFactory> instance;
  return instance.get();
}

SkillsServiceFactory::SkillsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SkillsService",
          BrowserContextDependencyManager::GetInstance()) {}

SkillsServiceFactory::~SkillsServiceFactory() = default;

std::unique_ptr<KeyedService>
SkillsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TODO(crbug.com/466802878): Return a nullptr if the feature is disabled.
  return std::make_unique<SkillsServiceImpl>();
}

}  // namespace skills
