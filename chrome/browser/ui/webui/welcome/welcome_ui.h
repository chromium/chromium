// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_WELCOME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_WELCOME_UI_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/welcome/ntp_background_fetcher.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

class WelcomeUI;

class WelcomeUIConfig : public content::DefaultWebUIConfig<WelcomeUI> {
 public:
  WelcomeUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIWelcomeHost) {}

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://welcome, the page which greets new Desktop users and
// promotes sign-in. By default, this page uses the "Welcome to Chrome" language
// and layout; the "Take Chrome Everywhere" variant may be accessed by appending
// the query string "?variant=everywhere".
class WelcomeUI : public content::WebUIController {
 public:
  WelcomeUI(content::WebUI* web_ui, const GURL& url);

  WelcomeUI(const WelcomeUI&) = delete;
  WelcomeUI& operator=(const WelcomeUI&) = delete;

  ~WelcomeUI() override;

  void CreateBackgroundFetcher(
      size_t background_index,
      content::WebUIDataSource::GotDataCallback callback);

 protected:
  // Visible for testing.
  static bool IsGzipped(const std::string& path);

 private:
  void StorePageSeen(Profile* profile);
  std::unique_ptr<welcome::NtpBackgroundFetcher> background_fetcher_;
  base::WeakPtrFactory<WelcomeUI> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_WELCOME_UI_H_
