// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ENGAGEMENT_SITE_ENGAGEMENT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ENGAGEMENT_SITE_ENGAGEMENT_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom-forward.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class SiteEngagementUI;

class SiteEngagementUIConfig
    : public content::DefaultWebUIConfig<SiteEngagementUI> {
 public:
  SiteEngagementUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISiteEngagementHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The UI for chrome://site-engagement/.
class SiteEngagementUI : public ui::MojoWebUIController {
 public:
  explicit SiteEngagementUI(content::WebUI* web_ui);

  SiteEngagementUI(const SiteEngagementUI&) = delete;
  SiteEngagementUI& operator=(const SiteEngagementUI&) = delete;

  ~SiteEngagementUI() override;

  // Instantiates the implementor of the mojom::SiteEngagementDetailsProvider
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          site_engagement::mojom::SiteEngagementDetailsProvider> receiver);

 private:
  std::unique_ptr<site_engagement::mojom::SiteEngagementDetailsProvider>
      ui_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_ENGAGEMENT_SITE_ENGAGEMENT_UI_H_
