// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_provider_base_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"

namespace web_app {

namespace {
WebAppProviderBaseFactory* g_factory = nullptr;
}  // namespace

// static
WebAppProviderBase* WebAppProviderBaseFactory::GetForProfile(Profile* profile) {
  return static_cast<WebAppProviderBase*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create=*/true));
}

// static
WebAppProviderBaseFactory* WebAppProviderBaseFactory::GetInstance() {
  DCHECK(g_factory);
  return g_factory;
}

// static
void WebAppProviderBaseFactory::SetInstance(
    WebAppProviderBaseFactory* factory) {
  g_factory = factory;
}

WebAppProviderBaseFactory::WebAppProviderBaseFactory(
    const char* service_name,
    BrowserContextDependencyManager* dependency_manager)
    : BrowserContextKeyedServiceFactory(service_name, dependency_manager) {}

WebAppProviderBaseFactory::~WebAppProviderBaseFactory() = default;

}  //  namespace web_app
