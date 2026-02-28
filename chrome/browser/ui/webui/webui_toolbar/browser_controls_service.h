// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class MetricsReporter;

namespace browser_controls_api {

class BrowserControlsService
    : public browser_controls_api::mojom::BrowserControlsService {
 public:
  class BrowserControlsServiceDelegate {
   public:
    virtual ~BrowserControlsServiceDelegate() = default;
    virtual void PermitLaunchUrl() = 0;
  };

  BrowserControlsService(
      mojo::PendingReceiver<mojom::BrowserControlsService> service,
      std::unique_ptr<BrowserControlsAdapter> browser_adapter,
      MetricsReporter* metrics_reporter,
      BrowserControlsServiceDelegate* delegate);

  BrowserControlsService(const BrowserControlsService&) = delete;
  BrowserControlsService& operator=(const BrowserControlsService&) = delete;

  ~BrowserControlsService() override;

  void SetDelegate(BrowserControlsServiceDelegate* delegate);

  // browser_controls_api::mojom::BrowserControlsService:
  void ReloadFromClick(
      bool bypass_cache,
      const std::vector<mojom::ClickDispositionFlag>& click_flags) override;
  void StopLoad() override;
  void SplitActiveTab() override;

 private:
  // Callback for `MetricsReporter::Measure()`. Records the resulting
  // base::TimeDelta to the given UMA histogram and clears the start mark.
  void OnMeasureResultAndClearMark(const std::string& histogram_name,
                                   const std::string& start_mark,
                                   base::TimeDelta duration);

  mojo::Receiver<browser_controls_api::mojom::BrowserControlsService> service_;
  std::unique_ptr<BrowserControlsAdapter> browser_adapter_;

  // Not owned.
  raw_ptr<MetricsReporter> metrics_reporter_;
  raw_ptr<BrowserControlsServiceDelegate> delegate_;

  // Must be the last member.
  base::WeakPtrFactory<BrowserControlsService> weak_ptr_factory_{this};
};

}  // namespace browser_controls_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_
