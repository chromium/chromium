// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_WEBUI_UI_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_WEBUI_UI_CONTROLLER_H_

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/kiosk/vision/internals_page_processor.h"
#include "chromeos/ash/components/kiosk/vision/webui/kiosk_vision_internals.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/webui/resource_path.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash::kiosk_vision {

// Alias for a callback with the same signature as the webui helper function
// `webui::SetupWebUIDataSource`.
//
// DEPS rules disallow this file to include the `webui::SetupWebUIDataSource`
// helper from //chrome/browser/, so this callback is used instead.
using SetupWebUIDataSourceCallback = base::RepeatingCallback<void(
    content::WebUIDataSource* source,
    base::span<const webui::ResourcePath> resources,
    int default_resource)>;

using GetInternalsPageProcessorCallback =
    base::RepeatingCallback<InternalsPageProcessor*()>;

// The WebUI controller for chrome://kiosk-vision-internals. This page displays
// development and debugging information for the Kiosk Vision feature.
class UIController : public ui::MojoWebUIController,
                     public mojom::PageConnector,
                     public InternalsPageProcessor::Observer {
 public:
  UIController(content::WebUI* web_ui,
               SetupWebUIDataSourceCallback setup_callback,
               GetInternalsPageProcessorCallback get_processor_callback);
  UIController(const UIController&) = delete;
  UIController& operator=(const UIController&) = delete;
  ~UIController() override;

  // Binds `this` object to the given `receiver`.
  void BindInterface(mojo::PendingReceiver<mojom::PageConnector> receiver);

 private:
  // `ash::kiosk_vision::mojom::PageConnector` implementation.
  void BindPage(mojo::PendingRemote<mojom::Page> page_remote) override;

  // `InternalsPageProcessor::Observer` implementation.
  void OnStateChange(const mojom::State& new_state) override;

  mojo::Remote<mojom::Page> page_;

  mojo::Receiver<mojom::PageConnector> receiver_{this};

  base::ScopedObservation<InternalsPageProcessor,
                          InternalsPageProcessor::Observer>
      observation_{this};

  GetInternalsPageProcessorCallback get_processor_callback_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// The WebUIConfig for chrome://kiosk-vision-internals.
class UIConfig : public content::WebUIConfig {
 public:
  explicit UIConfig(SetupWebUIDataSourceCallback setup_callback,
                    GetInternalsPageProcessorCallback get_processor_callback);
  UIConfig(const UIConfig&) = delete;
  UIConfig& operator=(const UIConfig&) = delete;
  ~UIConfig() override;

  // `content::WebUIConfig` implementation:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  SetupWebUIDataSourceCallback setup_callback_;
  GetInternalsPageProcessorCallback get_processor_callback_;
};

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_WEBUI_UI_CONTROLLER_H_
