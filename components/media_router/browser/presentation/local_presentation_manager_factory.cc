// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/presentation/local_presentation_manager_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/media_router/browser/presentation/local_presentation_manager.h"
#include "content/public/browser/web_contents.h"

namespace media_router {

namespace {

LocalPresentationManagerFactory* g_instance = nullptr;

}  // namespace

// static
LocalPresentationManager*
LocalPresentationManagerFactory::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  return LocalPresentationManagerFactory::GetOrCreateForBrowserContext(
      web_contents->GetBrowserContext());
}

// static
LocalPresentationManager*
LocalPresentationManagerFactory::GetOrCreateForBrowserContext(
    content::BrowserContext* context) {
  DCHECK(context);
  return static_cast<LocalPresentationManager*>(
      g_instance->GetServiceForBrowserContext(context, true));
}

LocalPresentationManagerFactory::LocalPresentationManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "LocalPresentationManager",
          BrowserContextDependencyManager::GetInstance()) {
  DCHECK(!g_instance);
  g_instance = this;
}

LocalPresentationManagerFactory::~LocalPresentationManagerFactory() {
  g_instance = nullptr;
}

std::unique_ptr<KeyedService>
LocalPresentationManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<LocalPresentationManager>();
}

}  // namespace media_router
