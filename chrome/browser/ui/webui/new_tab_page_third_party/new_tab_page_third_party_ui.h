// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_UI_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"

namespace content {
class WebContents;
class WebUI;
}  // namespace content

class GURL;
class MostVisitedHandler;
class NewTabPageThirdPartyHandler;
class Profile;
class NewTabPageThirdPartyUI;

class NewTabPageThirdPartyUIConfig
    : public content::DefaultWebUIConfig<NewTabPageThirdPartyUI> {
 public:
  NewTabPageThirdPartyUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUINewTabPageThirdPartyHost) {}

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class NewTabPageThirdPartyUI
    : public ui::MojoWebUIController,
      public new_tab_page_third_party::mojom::PageHandlerFactory,
      public most_visited::mojom::MostVisitedPageHandlerFactory {
 public:
  explicit NewTabPageThirdPartyUI(content::WebUI* web_ui);

  NewTabPageThirdPartyUI(const NewTabPageThirdPartyUI&) = delete;
  NewTabPageThirdPartyUI& operator=(const NewTabPageThirdPartyUI&) = delete;

  ~NewTabPageThirdPartyUI() override;

  static bool IsNewTabPageOrigin(const GURL& url);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<new_tab_page_third_party::mojom::PageHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of the
  // most_visited::mojom::MostVisitedPageHandlerFactory mojo interface passing
  // the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
          pending_receiver);

 private:
  // new_tab_page::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<new_tab_page_third_party::mojom::Page> pending_page,
      mojo::PendingReceiver<new_tab_page_third_party::mojom::PageHandler>
          pending_page_handler) override;

  // most_visited::mojom::MostVisitedPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
          pending_page_handler) override;

  std::unique_ptr<NewTabPageThirdPartyHandler> page_handler_;
  mojo::Receiver<new_tab_page_third_party::mojom::PageHandlerFactory>
      page_factory_receiver_;
  std::unique_ptr<MostVisitedHandler> most_visited_page_handler_;
  mojo::Receiver<most_visited::mojom::MostVisitedPageHandlerFactory>
      most_visited_page_factory_receiver_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  // Time the NTP started loading. Used for logging the WebUI NTP's load
  // performance.
  base::Time navigation_start_time_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_UI_H_
