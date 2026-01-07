// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_CONTENT_SKILLS_SERVICE_FACTORY_H_
#define COMPONENTS_SKILLS_CONTENT_SKILLS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace skills {

class SkillsService;

// Factory for SkillsService.
class SkillsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static SkillsService* GetForBrowserContext(content::BrowserContext* context);

  static SkillsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SkillsServiceFactory>;

  SkillsServiceFactory();
  ~SkillsServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_CONTENT_SKILLS_SERVICE_FACTORY_H_
