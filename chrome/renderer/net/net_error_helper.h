// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
#define CHROME_RENDERER_NET_NET_ERROR_HELPER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/common/navigation_corrector.mojom.h"
#include "chrome/common/network_diagnostics.mojom.h"
#include "chrome/common/network_easter_egg.mojom.h"
#include "chrome/renderer/net/net_error_helper_core.h"
#include "chrome/renderer/net/net_error_page_controller.h"
#include "components/error_page/common/localized_error.h"
#include "components/error_page/common/net_error_info.h"
#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "net/base/net_errors.h"

class GURL;

namespace error_page {
class Error;
struct ErrorPageParams;
}

namespace network {
class SimpleURLLoader;
}

// Listens for NetErrorInfo messages from the NetErrorTabHelper on the
// browser side and updates the error page with more details (currently, just
// DNS probe results) if/when available.
// TODO(crbug.com/578770): Should this class be moved into the error_page
// component?
class NetErrorHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<NetErrorHelper>,
      public content::RenderThreadObserver,
      public NetErrorHelperCore::Delegate,
      public NetErrorPageController::Delegate,
      public security_interstitials::SecurityInterstitialPageController::
          Delegate,
      public chrome::mojom::NetworkDiagnosticsClient,
      public chrome::mojom::NavigationCorrector {
 public:
  explicit NetErrorHelper(content::RenderFrame* render_frame);
  ~NetErrorHelper() override;

  // NetErrorPageController::Delegate implementation
  void ButtonPressed(NetErrorHelperCore::Button button) override;
  void TrackClick(int tracking_id) override;
  void LaunchOfflineItem(const std::string& id,
                         const std::string& name_space) override;
  void LaunchDownloadsPage() override;
  void SavePageForLater() override;
  void CancelSavePage() override;
  void ListVisibilityChanged(bool is_visible) override;
  void UpdateEasterEggHighScore(int high_score) override;
  void ResetEasterEggHighScore() override;

  // security_interstitials::SecurityInterstitialPageController::Delegate
  // implementation
  mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
  GetInterface() override;

  // RenderFrameObserver implementation.
  void DidStartNavigation(
      const GURL& url,
      base::Optional<blink::WebNavigationType> navigation_type) override;
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void DidFinishLoad() override;
  void OnStop() override;
  void WasShown() override;
  void WasHidden() override;
  void OnDestruct() override;

  // RenderThreadObserver implementation.
  void NetworkStateChanged(bool online) override;

  // Sets values in |pending_error_page_info_|. If |error_html| is not null, it
  // initializes |error_html| with the HTML of an error page in response to
  // |error|.  Updates internals state with the assumption the page will be
  // loaded immediately.
  void PrepareErrorPage(const error_page::Error& error,
                        bool is_failed_post,
                        std::string* error_html);

  // Returns whether a load for |url| in the |frame| the NetErrorHelper is
  // attached to should have its error page suppressed.
  bool ShouldSuppressErrorPage(const GURL& url);

 private:
  // Returns ResourceRequest filled with |url|. It has request_initiator from
  // the frame origin and origin header with "null" for a unique origin.
  std::unique_ptr<network::ResourceRequest> CreatePostRequest(
      const GURL& url) const;
  chrome::mojom::NetworkDiagnostics* GetRemoteNetworkDiagnostics();
  chrome::mojom::NetworkEasterEgg* GetRemoteNetworkEasterEgg();

  // NetErrorHelperCore::Delegate implementation:
  error_page::LocalizedError::PageState GenerateLocalizedErrorPage(
      const error_page::Error& error,
      bool is_failed_post,
      bool can_use_local_diagnostics_service,
      std::unique_ptr<error_page::ErrorPageParams> params,
      std::string* html) const override;
  void LoadErrorPage(const std::string& html, const GURL& failed_url) override;

  void EnablePageHelperFunctions() override;
  error_page::LocalizedError::PageState UpdateErrorPage(
      const error_page::Error& error,
      bool is_failed_post,
      bool can_use_local_diagnostics_service) override;
  void InitializeErrorPageEasterEggHighScore(int high_score) override;
  void RequestEasterEggHighScore() override;
  void FetchNavigationCorrections(
      const GURL& navigation_correction_url,
      const std::string& navigation_correction_request_body) override;
  void CancelFetchNavigationCorrections() override;
  void SendTrackingRequest(const GURL& tracking_url,
                           const std::string& tracking_request_body) override;
  void ReloadFrame() override;
  void DiagnoseError(const GURL& page_url) override;
  void DownloadPageLater() override;
  void SetIsShowingDownloadButton(bool show) override;
  void OfflineContentAvailable(
      bool list_visible_by_prefs,
      const std::string& offline_content_json) override;
  content::RenderFrame* GetRenderFrame() override;

#if defined(OS_ANDROID)
  void SetAutoFetchState(
      chrome::mojom::OfflinePageAutoFetcherScheduleResult state) override;
#endif

  void OnSetNavigationCorrectionInfo(const GURL& navigation_correction_url,
                                     const std::string& language,
                                     const std::string& country_code,
                                     const std::string& api_key,
                                     const GURL& search_url);

  void OnNavigationCorrectionsFetched(
      std::unique_ptr<std::string> response_body);

  void OnTrackingRequestComplete(std::unique_ptr<std::string> response_body);

  void OnNetworkDiagnosticsClientRequest(
      mojo::PendingAssociatedReceiver<chrome::mojom::NetworkDiagnosticsClient>
          receiver);
  void OnNavigationCorrectorRequest(
      mojo::PendingAssociatedReceiver<chrome::mojom::NavigationCorrector>
          receiver);

  // chrome::mojom::NetworkDiagnosticsClient:
  void SetCanShowNetworkDiagnosticsDialog(bool can_show) override;
  void DNSProbeStatus(int32_t) override;

  // chrome::mojom::NavigationCorrector:
  void SetNavigationCorrectionInfo(const GURL& navigation_correction_url,
                                   const std::string& language,
                                   const std::string& country_code,
                                   const std::string& api_key,
                                   const GURL& search_url) override;

  std::unique_ptr<network::SimpleURLLoader> correction_loader_;
  std::unique_ptr<network::SimpleURLLoader> tracking_loader_;

  std::unique_ptr<NetErrorHelperCore> core_;

  mojo::AssociatedReceiverSet<chrome::mojom::NetworkDiagnosticsClient>
      network_diagnostics_client_receivers_;
  mojo::AssociatedRemote<chrome::mojom::NetworkDiagnostics>
      remote_network_diagnostics_;
  mojo::AssociatedReceiverSet<chrome::mojom::NavigationCorrector>
      navigation_corrector_receivers_;
  mojo::AssociatedRemote<chrome::mojom::NetworkEasterEgg>
      remote_network_easter_egg_;

  // Weak factories for vending weak pointers to PageControllers. Weak
  // pointers are invalidated on each commit, to prevent getting messages from
  // Controllers used for the previous commit that haven't yet been cleaned up.
  base::WeakPtrFactory<NetErrorPageController::Delegate>
      weak_controller_delegate_factory_{this};

  base::WeakPtrFactory<
      security_interstitials::SecurityInterstitialPageController::Delegate>
      weak_security_interstitial_controller_delegate_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetErrorHelper);
};

#endif  // CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
