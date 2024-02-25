// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PerformanceSidePanelUI;

class PerformancePageHandler
    : public side_panel::mojom::PerformancePageHandler {
 public:
  explicit PerformancePageHandler(
      mojo::PendingReceiver<side_panel::mojom::PerformancePageHandler> receiver,
      mojo::PendingRemote<side_panel::mojom::PerformancePage> page,
      PerformanceSidePanelUI* performance_ui);
  PerformancePageHandler(const PerformancePageHandler&) = delete;
  PerformancePageHandler& operator=(const PerformancePageHandler&) = delete;
  ~PerformancePageHandler() override;

  // side_panel::mojom::PerformancePageHandler:
  void ShowUI() override;

 private:
  mojo::Receiver<side_panel::mojom::PerformancePageHandler> receiver_;
  mojo::Remote<side_panel::mojom::PerformancePage> page_;
  raw_ptr<PerformanceSidePanelUI> performance_ui_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_PAGE_HANDLER_H_
