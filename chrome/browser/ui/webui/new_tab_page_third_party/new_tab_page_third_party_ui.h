// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_UI_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebContents;
class WebUI;
}  // namespace content

class GURL;
class NewTabPageThirdPartyHandler;
class Profile;

class NewTabPageThirdPartyUI
    : public ui::MojoWebUIController,
      public new_tab_page_third_party::mojom::PageHandlerFactory {
 public:
  explicit NewTabPageThirdPartyUI(content::WebUI* web_ui);
  ~NewTabPageThirdPartyUI() override;

  static bool IsNewTabPageOrigin(const GURL& url);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<new_tab_page_third_party::mojom::PageHandlerFactory>
          pending_receiver);

 private:
  // new_tab_page::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<new_tab_page_third_party::mojom::Page> pending_page,
      mojo::PendingReceiver<new_tab_page_third_party::mojom::PageHandler>
          pending_page_handler) override;

  std::unique_ptr<NewTabPageThirdPartyHandler> page_handler_;
  mojo::Receiver<new_tab_page_third_party::mojom::PageHandlerFactory>
      page_factory_receiver_;
  Profile* profile_;
  content::WebContents* web_contents_;
  // Time the NTP started loading. Used for logging the WebUI NTP's load
  // performance.
  base::Time navigation_start_time_;

  WEB_UI_CONTROLLER_TYPE_DECL();

  DISALLOW_COPY_AND_ASSIGN(NewTabPageThirdPartyUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_UI_H_
