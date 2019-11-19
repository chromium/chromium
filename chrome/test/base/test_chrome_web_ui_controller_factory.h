// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_CHROME_WEB_UI_CONTROLLER_FACTORY_H_
#define CHROME_TEST_BASE_TEST_CHROME_WEB_UI_CONTROLLER_FACTORY_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "content/public/browser/web_ui.h"

// This class replaces the ChromeWebUIFactory when the switches::kTestType flag
// is passed. It provides a registry to override CreateWebUIControllerForURL()
// by host.
class TestChromeWebUIControllerFactory : public ChromeWebUIControllerFactory {
 public:
  // Interface to create a new WebUI object.
  class WebUIProvider {
   public:
    // Create and return a new WebUI object for the |web_contents| based on the
    // |url|.
    virtual std::unique_ptr<content::WebUIController> NewWebUI(
        content::WebUI* web_ui,
        const GURL& url) = 0;

   protected:
    virtual ~WebUIProvider();
  };

  using FactoryOverridesMap = std::map<std::string, WebUIProvider*>;

  TestChromeWebUIControllerFactory();
  ~TestChromeWebUIControllerFactory() override;

  // Sets the Web UI host.
  void set_webui_host(const std::string& webui_host);

  // Override the creation for urls having |host| with |provider|.
  void AddFactoryOverride(const std::string& host, WebUIProvider* provider);

  // Remove the override for urls having |host|.
  void RemoveFactoryOverride(const std::string& host);

  // ChromeWebUIFactory overrides.
  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override;
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  // Return the WebUIProvider for the |url|'s host if it exists, otherwise NULL.
  WebUIProvider* GetWebUIProvider(Profile* profile, const GURL& url) const;

  // Replace |url|'s host with the Web UI host if |url| is a test URL served
  // from the TestDataSource. This ensures the factory always creates the
  // appropriate Web UI controller when these URLs are encountered instead of
  // failing.
  GURL TestURLToWebUIURL(const GURL& url) const;

  // Stores the mapping of host to WebUIProvider.
  FactoryOverridesMap factory_overrides_;

  // Stores the Web UI host to create the correct Web UI controller for
  // chrome://test URL requests.
  std::string webui_host_;

  DISALLOW_COPY_AND_ASSIGN(TestChromeWebUIControllerFactory);
};

#endif  // CHROME_TEST_BASE_TEST_CHROME_WEB_UI_CONTROLLER_FACTORY_H_
