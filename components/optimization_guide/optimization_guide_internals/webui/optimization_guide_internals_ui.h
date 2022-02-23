// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_INTERNALS_WEBUI_OPTIMIZATION_GUIDE_INTERNALS_UI_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_INTERNALS_WEBUI_OPTIMIZATION_GUIDE_INTERNALS_UI_H_

#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/webui/resource_path.h"
#include "ui/webui/mojo_web_ui_controller.h"

class OptimizationGuideLogger;
class OptimizationGuideInternalsPageHandlerImpl;

// The WebUI controller for chrome://optimization-guide-internals.
class OptimizationGuideInternalsUI
    : public ui::MojoWebUIController,
      public optimization_guide_internals::mojom::PageHandlerFactory {
 public:
  using SetupWebUIDataSourceCallback =
      base::OnceCallback<void(base::span<const webui::ResourcePath> resources,
                              int default_resource)>;

  explicit OptimizationGuideInternalsUI(
      content::WebUI* web_ui,
      OptimizationGuideLogger* optimization_guide_logger,
      SetupWebUIDataSourceCallback set_up_data_source_callback);
  ~OptimizationGuideInternalsUI() override;

  OptimizationGuideInternalsUI(const OptimizationGuideInternalsUI&) = delete;
  OptimizationGuideInternalsUI& operator=(const OptimizationGuideInternalsUI&) =
      delete;

  void BindInterface(
      mojo::PendingReceiver<
          optimization_guide_internals::mojom::PageHandlerFactory> receiver);

 private:
  // optimization_guide_internals::mojom::PageHandlerFactory impls.
  void CreatePageHandler(
      mojo::PendingRemote<optimization_guide_internals::mojom::Page> page)
      override;

  std::unique_ptr<OptimizationGuideInternalsPageHandlerImpl>
      optimization_guide_internals_page_handler_;
  mojo::Receiver<optimization_guide_internals::mojom::PageHandlerFactory>
      optimization_guide_internals_page_factory_receiver_{this};

  // Logger to receive the debug logs from the optimization guide service. Not
  // owned. Guaranteed to outlive |this|, since the logger is owned by the
  // optimization guide keyed service, while |this| is part of
  // RenderFrameHostImpl::WebUIImpl.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_INTERNALS_WEBUI_OPTIMIZATION_GUIDE_INTERNALS_UI_H_
