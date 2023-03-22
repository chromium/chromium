// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Browser;
class CompanionSidePanelUntrustedUI;

// Query parameter for the url of the main web content.
inline constexpr char kUrlQueryParameterKey[] = "url";
// Query parameter for the Chrome WebUI origin.
inline constexpr char kOriginQueryParameterKey[] = "origin";
// Query parameter value for the Chrome WebUI origin. This needs to be different
// from the WebUI URL constant because it does not include the last '/'.
inline constexpr char kOriginQueryParameterValue[] =
    "chrome-untrusted://companion-side-panel.top-chrome";

class CompanionPageHandler : public side_panel::mojom::CompanionPageHandler,
                             public content::WebContentsObserver {
 public:
  explicit CompanionPageHandler(
      mojo::PendingReceiver<side_panel::mojom::CompanionPageHandler> receiver,
      mojo::PendingRemote<side_panel::mojom::CompanionPage> page,
      Browser* browser,
      CompanionSidePanelUntrustedUI* search_companion_ui);
  CompanionPageHandler(const CompanionPageHandler&) = delete;
  CompanionPageHandler& operator=(const CompanionPageHandler&) = delete;
  ~CompanionPageHandler() override;

  // side_panel::mojom::CompanionPageHandler:
  void ShowUI() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

 private:
  // Notifies the companion page of the initial URL it should load if any.
  // Otherwise, it will load the zero state.
  void InitializePage();

  // Notifies the companion page of the visible URL when the active tab has
  // changed or when the primary page has changed on the active tab.
  void NotifyURLChanged();
  bool IsMsbbEnabled();

  // Returns the companion URL that will be loaded in the side panel with the
  // URL query parameter set to `url_query_param_value` and the origin query
  // parameter set the to URL of the WebUI.
  GURL GetCompanionURLWithQueryParams(GURL url_query_param_value);
  GURL GetHomepageURLForCompanion();

  mojo::Receiver<side_panel::mojom::CompanionPageHandler> receiver_;
  mojo::Remote<side_panel::mojom::CompanionPage> page_;
  raw_ptr<CompanionSidePanelUntrustedUI> companion_untrusted_ui_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_PAGE_HANDLER_H_
