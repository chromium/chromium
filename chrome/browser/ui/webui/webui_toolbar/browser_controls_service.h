// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"



namespace content {
class RenderFrameHost;
}

namespace browser_controls_api {

class BrowserControlsService
    : public mojom::BrowserControlsServiceDirectReturnStub {
 public:
  class BrowserControlsServiceDelegate {
   public:
    virtual ~BrowserControlsServiceDelegate() = default;
    virtual void PermitLaunchUrl() = 0;
    // Queries the absolute navigation start time of the toolbar document.
    // This serves as the shared, document-scoped temporal baseline to
    // reconstruct absolute TimeTicks from renderer-reported relative offsets
    // for any toolbar button telemetry.
    virtual base::TimeTicks GetNavigationStartTicks() const = 0;
  };

  BrowserControlsService(
      mojo::PendingReceiver<mojom::BrowserControlsService> service,
      std::unique_ptr<BrowserControlsAdapter> browser_adapter,
      BrowserControlsServiceDelegate* delegate,
      content::RenderFrameHost* toolbar_rfh);

  BrowserControlsService(const BrowserControlsService&) = delete;
  BrowserControlsService& operator=(const BrowserControlsService&) = delete;

  ~BrowserControlsService() override;

  void SetDelegate(BrowserControlsServiceDelegate* delegate);

  // browser_controls_api::mojom::BrowserControlsService:
  ReloadFromClickResult ReloadFromClick(
      bool bypass_cache,
      const std::vector<mojom::EventDispositionFlag>& click_flags,
      mojom::ReloadInteractionMetadataPtr metadata = nullptr) override;
  StopLoadResult StopLoad() override;
  BackResult Back(
      const std::vector<mojom::EventDispositionFlag>& flags) override;
  ForwardResult Forward(
      const std::vector<mojom::EventDispositionFlag>& flags) override;
  BackButtonHoveredResult BackButtonHovered() override;
  SplitActiveTabResult SplitActiveTab() override;
  NavigateHomeResult NavigateHome(
      const std::vector<mojom::EventDispositionFlag>& click_flags) override;
  NavigateResult Navigate(const GURL& url) override;
  NavigateTextResult NavigateText(const std::string& text) override;

  static base::expected<ui::EventFlags, mojo_base::mojom::ErrorPtr>
  ToUiEventFlags(
      const std::vector<browser_controls_api::mojom::EventDispositionFlag>&
          flags);

 private:
  // Reconstructs the absolute interaction time from the Mojo metadata and
  // validates it against several criteria.
  // Returns the reconstructed TimeTicks, or a Mojo error if validation fails.
  base::expected<base::TimeTicks, mojo_base::mojom::ErrorPtr>
  ReconstructAndValidateInteractionTime(
      const mojom::ReloadInteractionMetadata& metadata,
      base::TimeTicks evaluation_time);

  // Maps the Mojo input type to the internal metrics input type and logs the
  // interaction-to-reload latency metric if the mapping is valid.
  void MaybeRecordInteractionToReloadMetric(
      base::TimeTicks interaction_ticks,
      mojom::ReloadInputType input_type) const;

  mojom::BrowserControlsServiceBridge bridge_{this};
  mojo::Receiver<browser_controls_api::mojom::BrowserControlsService> service_;
  std::unique_ptr<BrowserControlsAdapter> browser_adapter_;

  // Not owned.
  raw_ptr<BrowserControlsServiceDelegate> delegate_;
  const raw_ptr<content::RenderFrameHost> toolbar_rfh_;

  base::TimeTicks last_processed_interaction_ticks_;

  // Must be the last member.
  base::WeakPtrFactory<BrowserControlsService> weak_ptr_factory_{this};
};

}  // namespace browser_controls_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_
