// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_CONTROLLER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/favicon_base/favicon_callback.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "ui/base/layout.h"

class Profile;

namespace base {
class RefCountedMemory;
}

namespace url {
class Origin;
}

class ChromeWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  static ChromeWebUIControllerFactory* GetInstance();

  // http://crbug.com/829412
  // Renderers with WebUI bindings shouldn't make http(s) requests for security
  // reasons (e.g. to avoid malicious responses being able to run code in
  // priviliged renderers). Fix these webui's to make requests through C++
  // code instead.
  static bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin);

  // content::WebUIControllerFactory:
  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override;
  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override;
  bool UseWebUIBindingsForURL(content::BrowserContext* browser_context,
                              const GURL& url) override;
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override;

  // Get the favicon for |page_url| and run |callback| with result when loaded.
  // Note. |callback| is always run asynchronously.
  void GetFaviconForURL(Profile* profile,
                        const GURL& page_url,
                        const std::vector<int>& desired_sizes_in_pixel,
                        favicon_base::FaviconResultsCallback callback) const;

 protected:
  ChromeWebUIControllerFactory();
  ~ChromeWebUIControllerFactory() override;

 private:
  friend struct base::DefaultSingletonTraits<ChromeWebUIControllerFactory>;

  // Gets the data for the favicon for a WebUI page. Returns NULL if the WebUI
  // does not have a favicon.
  // The returned favicon data must be
  // |gfx::kFaviconSize| x |gfx::kFaviconSize| DIP. GetFaviconForURL() should
  // be updated if this changes.
  base::RefCountedMemory* GetFaviconResourceBytes(
      const GURL& page_url,
      ui::ScaleFactor scale_factor) const;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebUIControllerFactory);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_CONTROLLER_FACTORY_H_
