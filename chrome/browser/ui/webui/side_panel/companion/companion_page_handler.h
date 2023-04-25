// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/companion/core/msbb_delegate.h"
#include "components/lens/buildflags.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Browser;
class CompanionSidePanelUntrustedUI;
class Profile;

namespace companion {
class CompanionMetricsLogger;
class CompanionUrlBuilder;
class PromoHandler;
class SigninDelegate;
class TextFinderManager;

class CompanionPageHandler : public side_panel::mojom::CompanionPageHandler,
                             public content::WebContentsObserver,
                             public MsbbDelegate {
 public:
  CompanionPageHandler(
      mojo::PendingReceiver<side_panel::mojom::CompanionPageHandler> receiver,
      mojo::PendingRemote<side_panel::mojom::CompanionPage> page,
      CompanionSidePanelUntrustedUI* companion_ui);
  CompanionPageHandler(const CompanionPageHandler&) = delete;
  CompanionPageHandler& operator=(const CompanionPageHandler&) = delete;
  ~CompanionPageHandler() override;

  // side_panel::mojom::CompanionPageHandler:
  void ShowUI() override;
  void OnPromoAction(side_panel::mojom::PromoType promo_type,
                     side_panel::mojom::PromoAction promo_action) override;
  void OnRegionSearchClicked() override;
  void OnExpsOptInStatusAvailable(bool is_exps_opted_in) override;
  void OnOpenInNewTabButtonURLChanged(const ::GURL& url_to_open) override;
  void RecordUiSurfaceShown(side_panel::mojom::UiSurface ui_surface,
                            uint32_t child_element_count) override;
  void RecordUiSurfaceClicked(side_panel::mojom::UiSurface ui_surface) override;
  void OnCqCandidatesAvailable(
      const std::vector<std::string>& text_directives) override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // Informs the page handler that a new text query to initialize / reload the
  // page with was sent from client.
  void OnSearchTextQuery(const std::string& text_query);
  void OnImageQuery(side_panel::mojom::ImageQuery image_query);

  // Returns the latest set url to be used for the 'open in new tab' button in
  // the side panel header.
  GURL GetNewTabButtonUrl();

 private:
  // MsbbDelegate overrides.
  void EnableMsbb(bool enable_msbb) override;

  // Notifies the companion side panel about the URL of the main frame. Based on
  // the call site, either does a full reload of the side panel or does a
  // postmessage() update. Reload is done during initial load of the side panel,
  // and context menu initiated navigations, while postmessage() is used for
  // subsequent navigations on the main frame.
  void NotifyURLChanged(bool is_full_reload);

  // Get the current browser associated with the WebUI.
  Browser* GetBrowser();
  // Get the profile associated with the WebUI.
  Profile* GetProfile();

  // A callback function called when the text finder manager finishes finding
  // all input text directives.
  void DidFinishFindingCqTexts(
      const std::vector<std::pair<std::string, bool>>& text_found_vec);

  mojo::Receiver<side_panel::mojom::CompanionPageHandler> receiver_;
  mojo::Remote<side_panel::mojom::CompanionPage> page_;
  raw_ptr<CompanionSidePanelUntrustedUI> companion_untrusted_ui_ = nullptr;
  raw_ptr<TextFinderManager> text_finder_manager_ = nullptr;
  std::unique_ptr<SigninDelegate> signin_delegate_;
  std::unique_ptr<CompanionUrlBuilder> url_builder_;
  std::unique_ptr<PromoHandler> promo_handler_;
  GURL open_in_new_tab_url_;

  // Logs metrics for companion page. Reset when there is a new navigation.
  std::unique_ptr<CompanionMetricsLogger> metrics_logger_;

  base::WeakPtrFactory<CompanionPageHandler> weak_ptr_factory_{this};
};
}  // namespace companion

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_PAGE_HANDLER_H_
