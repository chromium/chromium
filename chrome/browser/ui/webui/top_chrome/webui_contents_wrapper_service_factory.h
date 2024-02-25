// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WRAPPER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WRAPPER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class WebUIContentsWrapperService;
class Profile;

class WebUIContentsWrapperServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static WebUIContentsWrapperService* GetForProfile(Profile* profile,
                                                    bool create_if_necessary);
  static WebUIContentsWrapperServiceFactory* GetInstance();

  WebUIContentsWrapperServiceFactory(
      const WebUIContentsWrapperServiceFactory&) = delete;
  WebUIContentsWrapperServiceFactory& operator=(
      const WebUIContentsWrapperServiceFactory&) = delete;

 private:
  friend base::NoDestructor<WebUIContentsWrapperServiceFactory>;

  WebUIContentsWrapperServiceFactory();
  ~WebUIContentsWrapperServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WRAPPER_SERVICE_FACTORY_H_
