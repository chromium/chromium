// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"

using content::WebContents;
using content::WebUI;
using content::WebUIController;

TestChromeWebUIControllerFactory::WebUIProvider::~WebUIProvider() {
}

TestChromeWebUIControllerFactory::TestChromeWebUIControllerFactory() {
}

TestChromeWebUIControllerFactory::~TestChromeWebUIControllerFactory() {
}

void TestChromeWebUIControllerFactory::AddFactoryOverride(
    const std::string& host, WebUIProvider* provider) {
  DCHECK_EQ(0U, factory_overrides_.count(host));
  factory_overrides_[host] = provider;
}

void TestChromeWebUIControllerFactory::RemoveFactoryOverride(
    const std::string& host) {
  DCHECK_EQ(1U, factory_overrides_.count(host));
  factory_overrides_.erase(host);
}

WebUI::TypeID TestChromeWebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context, const GURL& url) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebUIProvider* provider = GetWebUIProvider(profile, url);
  return provider ? reinterpret_cast<WebUI::TypeID>(provider) :
      ChromeWebUIControllerFactory::GetWebUIType(profile, url);
}

std::unique_ptr<WebUIController>
TestChromeWebUIControllerFactory::CreateWebUIControllerForURL(
    content::WebUI* web_ui,
    const GURL& url) const {
  Profile* profile = Profile::FromWebUI(web_ui);
  WebUIProvider* provider = GetWebUIProvider(profile, url);
  return provider ? provider->NewWebUI(web_ui, url)
                  : ChromeWebUIControllerFactory::CreateWebUIControllerForURL(
                        web_ui, url);
}

TestChromeWebUIControllerFactory::WebUIProvider*
    TestChromeWebUIControllerFactory::GetWebUIProvider(
        Profile* profile, const GURL& url) const {
  auto found = factory_overrides_.find(url.host());
  return found != factory_overrides_.end() ? found->second : nullptr;
}
