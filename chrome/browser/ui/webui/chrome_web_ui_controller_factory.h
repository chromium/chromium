// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_CONTROLLER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/favicon_base/favicon_callback.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "ui/base/resource/resource_scale_factor.h"

class Profile;

namespace base {
class RefCountedMemory;
}

namespace url {
class Origin;
}

class ChromeWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  ChromeWebUIControllerFactory(const ChromeWebUIControllerFactory&) = delete;
  ChromeWebUIControllerFactory& operator=(const ChromeWebUIControllerFactory&) =
      delete;

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
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override;

  // Get the favicon for |page_url| and run |callback| with result when loaded.
  // Note. |callback| is always run asynchronously.
  void GetFaviconForURL(Profile* profile,
                        const GURL& page_url,
                        const std::vector<int>& desired_sizes_in_pixel,
                        favicon_base::FaviconResultsCallback callback) const;

#if BUILDFLAG(IS_CHROMEOS)
  // When Lacros is enabled, this function is called to retrieve a list of URLs
  // which can be handled by this browser (Ash or Lacros).
  // For Ash this means that they are shown in an SWA application and for
  // Lacros it means that Lacros will handle them themselves.
  const std::vector<GURL>& GetListOfAcceptableURLs();

  // Determines if the given URL can be handled by any known handler.
  bool CanHandleUrl(const GURL& url);
#endif

 protected:
  ChromeWebUIControllerFactory();
  ~ChromeWebUIControllerFactory() override;

 private:
  friend base::NoDestructor<ChromeWebUIControllerFactory>;

  // Gets the data for the favicon for a WebUI page. Returns NULL if the WebUI
  // does not have a favicon.
  // The returned favicon data must be
  // |gfx::kFaviconSize| x |gfx::kFaviconSize| DIP. GetFaviconForURL() should
  // be updated if this changes.
  base::RefCountedMemory* GetFaviconResourceBytes(
      const GURL& page_url,
      ui::ResourceScaleFactor scale_factor) const;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_CONTROLLER_FACTORY_H_
