// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper_service_factory.h"

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper_service.h"

// static
BubbleContentsWrapperService*
BubbleContentsWrapperServiceFactory::GetForProfile(Profile* profile,
                                                   bool create_if_necessary) {
  return static_cast<BubbleContentsWrapperService*>(
      GetInstance()->GetServiceForBrowserContext(profile, create_if_necessary));
}

// static
BubbleContentsWrapperServiceFactory*
BubbleContentsWrapperServiceFactory::GetInstance() {
  static base::NoDestructor<BubbleContentsWrapperServiceFactory> factory;
  return factory.get();
}

BubbleContentsWrapperServiceFactory::BubbleContentsWrapperServiceFactory()
    : ProfileKeyedServiceFactory("BubbleContentsWrapperService") {}

KeyedService* BubbleContentsWrapperServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BubbleContentsWrapperService(Profile::FromBrowserContext(context));
}

BubbleContentsWrapperServiceFactory::~BubbleContentsWrapperServiceFactory() =
    default;
