// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_BUBBLE_CONTENTS_WRAPPER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_BUBBLE_CONTENTS_WRAPPER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class BubbleContentsWrapperService;
class Profile;

class BubbleContentsWrapperServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static BubbleContentsWrapperService* GetForProfile(Profile* profile,
                                                     bool create_if_necessary);
  static BubbleContentsWrapperServiceFactory* GetInstance();

  BubbleContentsWrapperServiceFactory(
      const BubbleContentsWrapperServiceFactory&) = delete;
  BubbleContentsWrapperServiceFactory& operator=(
      const BubbleContentsWrapperServiceFactory&) = delete;

 private:
  friend base::NoDestructor<BubbleContentsWrapperServiceFactory>;

  BubbleContentsWrapperServiceFactory();
  ~BubbleContentsWrapperServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_BUBBLE_CONTENTS_WRAPPER_SERVICE_FACTORY_H_
