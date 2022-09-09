// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service_factory.h"

#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace app_list {

// static
FileSuggestKeyedServiceFactory* FileSuggestKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<FileSuggestKeyedServiceFactory> factory;
  return factory.get();
}

FileSuggestKeyedServiceFactory::FileSuggestKeyedServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FileSuggestKeyedService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
}

FileSuggestKeyedServiceFactory::~FileSuggestKeyedServiceFactory() = default;

FileSuggestKeyedService* FileSuggestKeyedServiceFactory::GetService(
    content::BrowserContext* context) {
  return static_cast<FileSuggestKeyedService*>(
      GetServiceForBrowserContext(context, /*create=*/true));
}

content::BrowserContext* FileSuggestKeyedServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

KeyedService* FileSuggestKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FileSuggestKeyedService(Profile::FromBrowserContext(context));
}

}  // namespace app_list
