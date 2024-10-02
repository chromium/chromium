// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "chrome/browser/companion/core/companion_metrics_logger.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "components/lens/buildflags.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/browser/navigation_handle.h"
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

class CompanionPageHandler
    : public side_panel::mojom::CompanionPageHandler,
      public content::WebContentsObserver,
      public signin::IdentityManager::Observer,
      public unified_consent::UrlKeyedDataCollectionConsentHelper::Observer {
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
  void OnOpenInNewTabButtonURLChanged(const GURL& url_to_open) override;
  void RecordUiSurfaceShown(side_panel::mojom::UiSurface ui_surface,
                            int32_t ui_surface_position,
                            int32_t child_element_available_count,
                            int32_t child_element_shown_count) override;
  void RecordUiSurfaceClicked(side_panel::mojom::UiSurface ui_surface,
                              int32_t click_position) override;
  void OnCqCandidatesAvailable(
      const std::vector<std::string>& text_directives) override;
  void OnPhFeedback(side_panel::mojom::PhFeedback ph_feedback) override;
  void OnCqJumptagClicked(const std::string& text_directive) override;
  void OpenUrlInBrowser(const std::optional<GURL>& url_to_open,
                        bool use_new_tab) override;
  void OnLoadingState(side_panel::mojom::LoadingState loading_state) override;
  void RefreshCompanionPage() override;
  void OnServerSideUrlFilterEvent() override;

  // content::WebContentsObserver overrides.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // IdentityManager::Observer overrides.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;

  // UrlKeyedDataCollectionConsentHelper::Observer overrides.
  void OnUrlKeyedDataCollectionConsentStateChanged(
      unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper)
      override;

  // Called when page content setting pref changes.
  void OnPageContentPrefChanged();

  // Attempts to retrieve a search query string and initiate a search if
  // available. If it does not load a companion page, returns false.
  bool OnSearchTextQuery();
  void OnImageQuery(side_panel::mojom::ImageQuery image_query);

  // Informs the page handler that the WebUI has detected a navigation that
  // resulted in an error page.
  void OnNavigationError();

  // Notifies the companion side panel about a link click that happened in
  // the side panel that maybe was handled by the browser (either new tab or
  // same tab).
  void NotifyLinkOpened(GURL opened_url,
                        side_panel::mojom::LinkOpenMetadataPtr metadata);

 private:
  // Notifies the companion side panel about the URL of the main frame. Based on
  // the call site, either does a full reload of the side panel or does a
  // postmessage() update. Reload is done during initial load of the side panel,
  // and context menu initiated navigations, while postmessage() is used for
  // subsequent navigations on the main frame.
  void NotifyURLChanged(bool is_full_reload);

  // Registers a WebContentsModalDialogManager for our WebContents in order to
  // display web modal dialogs triggered by it.
  void RegisterModalDialogManager(Browser* browser);

  // Get the current browser associated with the WebUI.
  Browser* GetBrowser();
  // Get the profile associated with the WebUI.
  Profile* GetProfile();

  // A callback function called when the text finder manager finishes finding
  // all input text directives.
  void DidFinishFindingCqTexts(
      const std::vector<std::pair<std::string, bool>>& text_found_vec);

  // The callback that handles the response to the request for the innerHTML of
  // the main frame. Stores the response in |inner_html_| and sends it to the
  // side panel if ready.
  void HandleInnerHtmlResponse(const std::optional<std::string>& inner_html);

  // Notifies the companion side panel about the title and the innerHTML of the
  // main frame using a postmessage() update.
  void SendPageContent();

  mojo::Receiver<side_panel::mojom::CompanionPageHandler> receiver_;
  mojo::Remote<side_panel::mojom::CompanionPage> page_;
  raw_ptr<CompanionSidePanelUntrustedUI> companion_untrusted_ui_ = nullptr;
  std::unique_ptr<SigninDelegate> signin_delegate_;
  std::unique_ptr<CompanionUrlBuilder> url_builder_;
  std::unique_ptr<PromoHandler> promo_handler_;
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      consent_helper_;

  // Logs metrics for companion page. Reset when there is a new navigation.
  std::unique_ptr<CompanionMetricsLogger> metrics_logger_;

  // The current URL of the main frame.
  GURL page_url_;

  // Observers for sign-in, MSBB, and page content status.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::ScopedObservation<
      unified_consent::UrlKeyedDataCollectionConsentHelper,
      unified_consent::UrlKeyedDataCollectionConsentHelper::Observer>
      consent_helper_observation_{this};
  PrefChangeRegistrar pref_change_registrar_;

  std::optional<base::TimeTicks> full_load_start_time_;
  std::optional<base::TimeTicks> reload_start_time_;
  std::optional<base::TimeTicks> ui_ready_for_visual_queries_time_;

  // Indicates that the kStartedLoading signal was received from side panel. The
  // page content is sent to the side panel only if the side panel is ready.
  // Otherwise, it is stored to be sent when the kStartedLoading signal is
  // received from the side panel.
  std::optional<base::TimeTicks> ui_ready_for_page_content_time_;
  // Used to store the page content before the side panel is ready for it. This
  // is untrustworthy content which will be sent to the webui for processing.
  // TODO(crbug.com/40285814): Use an opaque mojo type to hold this data in the
  // browser.
  std::optional<std::string> inner_html_;

  base::WeakPtrFactory<CompanionPageHandler> weak_ptr_factory_{this};
};
}  // namespace companion

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_PAGE_HANDLER_H_
