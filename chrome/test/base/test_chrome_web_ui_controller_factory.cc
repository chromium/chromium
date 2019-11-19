// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"

#include "base/bind_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/test_data_source.h"
#include "content/public/browser/url_data_source.h"
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

void TestChromeWebUIControllerFactory::set_webui_host(
    const std::string& webui_host) {
  webui_host_ = webui_host;
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
    content::BrowserContext* browser_context,
    const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const GURL& webui_url = TestURLToWebUIURL(url);
  WebUIProvider* provider = GetWebUIProvider(profile, webui_url);
  return provider
             ? reinterpret_cast<WebUI::TypeID>(provider)
             : ChromeWebUIControllerFactory::GetWebUIType(profile, webui_url);
}

std::unique_ptr<WebUIController>
TestChromeWebUIControllerFactory::CreateWebUIControllerForURL(
    content::WebUI* web_ui,
    const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  const GURL& webui_url = TestURLToWebUIURL(url);
  WebUIProvider* provider = GetWebUIProvider(profile, webui_url);
  auto controller =
      provider ? provider->NewWebUI(web_ui, webui_url)
               : ChromeWebUIControllerFactory::CreateWebUIControllerForURL(
                     web_ui, webui_url);
  // Add an empty callback since managed-footnote always sends this message.
  web_ui->RegisterMessageCallback("observeManagedUI", base::DoNothing());
  content::URLDataSource::Add(profile,
                              std::make_unique<TestDataSource>("webui"));
  return controller;
}

TestChromeWebUIControllerFactory::WebUIProvider*
    TestChromeWebUIControllerFactory::GetWebUIProvider(
        Profile* profile, const GURL& url) const {
  const GURL& webui_url = TestURLToWebUIURL(url);
  auto found = factory_overrides_.find(webui_url.host());
  return found != factory_overrides_.end() ? found->second : nullptr;
}

GURL TestChromeWebUIControllerFactory::TestURLToWebUIURL(
    const GURL& url) const {
  if (url.host() != "test" || webui_host_.empty())
    return url;

  GURL webui_url(url);
  GURL::Replacements replacements;
  replacements.SetHostStr(webui_host_);
  return webui_url.ReplaceComponents(replacements);
}
