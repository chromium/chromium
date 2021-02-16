// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORIES_CONTENT_MEMORIES_SERVICE_FACTORY_H_
#define COMPONENTS_MEMORIES_CONTENT_MEMORIES_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace memories {
class MemoriesService;
}

// Factory for BrowserContext keyed MemoriesService, which clusters Chrome
// history into useful Memories to be surfaced in UI.
class MemoriesServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static memories::MemoriesService* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  friend base::NoDestructor<MemoriesServiceFactory>;
  static MemoriesServiceFactory& GetInstance();

  MemoriesServiceFactory();
  ~MemoriesServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // COMPONENTS_MEMORIES_CONTENT_MEMORIES_SERVICE_FACTORY_H_
