// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_SIDE_PANEL_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/side_panel/performance_controls/performance.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

class PerformancePageHandler;
class BatterySaverCardHandler;
class MemorySaverCardHandler;

class PerformanceSidePanelUI
    : public ui::MojoBubbleWebUIController,
      side_panel::mojom::PerformancePageHandlerFactory,
      side_panel::mojom::BatterySaverCardHandlerFactory,
      side_panel::mojom::MemorySaverCardHandlerFactory {
 public:
  explicit PerformanceSidePanelUI(content::WebUI* web_ui);
  PerformanceSidePanelUI(const PerformanceSidePanelUI&) = delete;
  PerformanceSidePanelUI& operator=(const PerformanceSidePanelUI&) = delete;
  ~PerformanceSidePanelUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<side_panel::mojom::PerformancePageHandlerFactory>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

 private:
  // side_panel::mojom::PerformancePageHandlerFactory:
  void CreatePerformancePageHandler(
      mojo::PendingRemote<side_panel::mojom::PerformancePage> page,
      mojo::PendingReceiver<side_panel::mojom::PerformancePageHandler> receiver)
      override;

  // side_panel::mojom::BatterySaverCardHandlerFactory:
  void CreateBatterySaverCardHandler(
      mojo::PendingRemote<side_panel::mojom::BatterySaverCard>
          battery_saver_card,
      mojo::PendingReceiver<side_panel::mojom::BatterySaverCardHandler>
          battery_saver_receiver) override;

  // side_panel::mojom::MemorySaverCardHandlerFactory:
  void CreateMemorySaverCardHandler(
      mojo::PendingRemote<side_panel::mojom::MemorySaverCard> memory_saver_card,
      mojo::PendingReceiver<side_panel::mojom::MemorySaverCardHandler>
          memory_saver_receiver) override;

  std::unique_ptr<PerformancePageHandler> performance_page_handler_;
  std::unique_ptr<BatterySaverCardHandler> battery_saver_card_handler_;
  std::unique_ptr<MemorySaverCardHandler> memory_saver_card_handler_;
  mojo::Receiver<side_panel::mojom::PerformancePageHandlerFactory>
      performance_page_factory_receiver_{this};
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_SIDE_PANEL_UI_H_
