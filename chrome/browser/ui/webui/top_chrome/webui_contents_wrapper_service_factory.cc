// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper_service.h"

// static
WebUIContentsWrapperService*
WebUIContentsWrapperServiceFactory::GetForProfile(Profile* profile,
                                                   bool create_if_necessary) {
  return static_cast<WebUIContentsWrapperService*>(
      GetInstance()->GetServiceForBrowserContext(profile, create_if_necessary));
}

// static
WebUIContentsWrapperServiceFactory*
WebUIContentsWrapperServiceFactory::GetInstance() {
  static base::NoDestructor<WebUIContentsWrapperServiceFactory> factory;
  return factory.get();
}

WebUIContentsWrapperServiceFactory::WebUIContentsWrapperServiceFactory()
    : ProfileKeyedServiceFactory(
          "WebUIContentsWrapperService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

std::unique_ptr<KeyedService>
WebUIContentsWrapperServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<WebUIContentsWrapperService>(
      Profile::FromBrowserContext(context));
}

WebUIContentsWrapperServiceFactory::~WebUIContentsWrapperServiceFactory() =
    default;
