// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_page.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "ui/webui/mojo_web_ui_controller.h"

// A dev UI for testing the OnDeviceModelService.
class OnDeviceInternalsUI : public ui::MojoWebUIController,
                            public mojom::OnDeviceInternalsPage {
 public:
  explicit OnDeviceInternalsUI(content::WebUI* web_ui);
  ~OnDeviceInternalsUI() override;

  OnDeviceInternalsUI(const OnDeviceInternalsUI&) = delete;
  OnDeviceInternalsUI& operator=(const OnDeviceInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::OnDeviceInternalsPage> receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  on_device_model::mojom::OnDeviceModelService& GetService();
  void OnModelAssetsLoaded(LoadModelCallback callback,
                           on_device_model::ModelAssets assets);

  // mojom::OnDeviceInternalsPage:
  void LoadModel(const base::FilePath& model_path,
                 LoadModelCallback callback) override;
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback) override;

  mojo::ReceiverSet<mojom::OnDeviceInternalsPage> page_receivers_;
  mojo::Remote<on_device_model::mojom::OnDeviceModelService> service_;

  base::WeakPtrFactory<OnDeviceInternalsUI> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_UI_H_
