// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
#define CHROME_RENDERER_NET_NET_ERROR_HELPER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/net/net_error_page_support.mojom.h"
#include "chrome/common/network_diagnostics.mojom.h"
#include "chrome/common/network_easter_egg.mojom.h"
#include "chrome/renderer/net/net_error_helper_core.h"
#include "chrome/renderer/net/net_error_page_controller.h"
#include "components/error_page/common/localized_error.h"
#include "components/error_page/common/net_error_info.h"
#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/common/alternative_error_page_override_info.mojom-forward.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class GURL;

namespace error_page {
class Error;
}

// Listens for NetErrorInfo messages from the NetErrorTabHelper on the
// browser side and updates the error page with more details (currently, just
// DNS probe results) if/when available.
// TODO(crbug.com/41235130): Should this class be moved into the error_page
// component?
class NetErrorHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<NetErrorHelper>,
      public NetErrorHelperCore::Delegate,
      public NetErrorPageController::Delegate,
      public chrome::mojom::NetworkDiagnosticsClient {
 public:
  explicit NetErrorHelper(content::RenderFrame* render_frame);
  NetErrorHelper(const NetErrorHelper&) = delete;
  NetErrorHelper& operator=(const NetErrorHelper&) = delete;
  ~NetErrorHelper() override;

  // NetErrorPageController::Delegate implementation
  void ButtonPressed(NetErrorHelperCore::Button button) override;
  void LaunchOfflineItem(const std::string& id,
                         const std::string& name_space) override;
  void LaunchDownloadsPage() override;
  void SavePageForLater() override;
  void CancelSavePage() override;
  void ListVisibilityChanged(bool is_visible) override;
  void UpdateEasterEggHighScore(int high_score) override;
  void ResetEasterEggHighScore() override;

  // RenderFrameObserver implementation.
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFinishLoad() override;
  void OnDestruct() override;

  // Sets values in |pending_error_page_info_|. If |error_html| is not null, it
  // initializes |error_html| with the HTML of an error page in response to
  // |error|.  Updates internals state with the assumption the page will be
  // loaded immediately.
  void PrepareErrorPage(const error_page::Error& error,
                        bool is_failed_post,
                        content::mojom::AlternativeErrorPageOverrideInfoPtr
                            alternative_error_page_info,
                        std::string* error_html);

 private:
  chrome::mojom::NetworkDiagnostics* GetRemoteNetworkDiagnostics();
  chrome::mojom::NetworkEasterEgg* GetRemoteNetworkEasterEgg();
  chrome::mojom::NetErrorPageSupport* GetRemoteNetErrorPageSupport();

  // NetErrorHelperCore::Delegate implementation:
  error_page::LocalizedError::PageState GenerateLocalizedErrorPage(
      const error_page::Error& error,
      bool is_failed_post,
      bool can_use_local_diagnostics_service,
      content::mojom::AlternativeErrorPageOverrideInfoPtr
          alternative_error_page_info,
      std::string* html) override;

  void EnablePageHelperFunctions() override;
  error_page::LocalizedError::PageState UpdateErrorPage(
      const error_page::Error& error,
      bool is_failed_post,
      bool can_use_local_diagnostics_service) override;
  void InitializeErrorPageEasterEggHighScore(int high_score) override;
  void RequestEasterEggHighScore() override;
  void ReloadFrame() override;
  void DiagnoseError(const GURL& page_url) override;
  void PortalSignin() override;
  void DownloadPageLater() override;
  void SetIsShowingDownloadButton(bool show) override;
  void OfflineContentAvailable(
      bool list_visible_by_prefs,
      const std::string& offline_content_json) override;
  content::RenderFrame* GetRenderFrame() override;

#if BUILDFLAG(IS_ANDROID)
  void SetAutoFetchState(
      chrome::mojom::OfflinePageAutoFetcherScheduleResult state) override;
#endif

  void OnNetworkDiagnosticsClientRequest(
      mojo::PendingAssociatedReceiver<chrome::mojom::NetworkDiagnosticsClient>
          receiver);

  // chrome::mojom::NetworkDiagnosticsClient:
  void SetCanShowNetworkDiagnosticsDialog(bool can_show) override;
  void DNSProbeStatus(int32_t) override;

  std::unique_ptr<NetErrorHelperCore> core_;

  mojo::AssociatedReceiverSet<chrome::mojom::NetworkDiagnosticsClient>
      network_diagnostics_client_receivers_;
  mojo::AssociatedRemote<chrome::mojom::NetworkDiagnostics>
      remote_network_diagnostics_;
  mojo::AssociatedRemote<chrome::mojom::NetworkEasterEgg>
      remote_network_easter_egg_;
  mojo::AssociatedRemote<chrome::mojom::NetErrorPageSupport>
      remote_net_error_page_support_;

  base::Value::Dict error_page_params_;

  // Weak factories for vending weak pointers to PageControllers. Weak
  // pointers are invalidated on each commit, to prevent getting messages from
  // Controllers used for the previous commit that haven't yet been cleaned up.
  base::WeakPtrFactory<NetErrorPageController::Delegate>
      weak_controller_delegate_factory_{this};
};

#endif  // CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
