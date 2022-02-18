// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals_ui.h"

#include "components/grit/optimization_guide_internals_resources.h"
#include "components/grit/optimization_guide_internals_resources_map.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals_page_handler_impl.h"

OptimizationGuideInternalsUI::OptimizationGuideInternalsUI(
    content::WebUI* web_ui,
    OptimizationGuideLogger* optimization_guide_logger,
    SetupWebUIDataSourceCallback set_up_data_source_callback)
    : MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      optimization_guide_logger_(optimization_guide_logger) {
  std::move(set_up_data_source_callback)
      .Run(base::make_span(kOptimizationGuideInternalsResources,
                           kOptimizationGuideInternalsResourcesSize),
           IDR_OPTIMIZATION_GUIDE_INTERNALS_OPTIMIZATION_GUIDE_INTERNALS_HTML);
}

OptimizationGuideInternalsUI::~OptimizationGuideInternalsUI() = default;

void OptimizationGuideInternalsUI::BindInterface(
    mojo::PendingReceiver<
        optimization_guide_internals::mojom::PageHandlerFactory> receiver) {
  // TODO(https://crbug.com/1297362): Remove the reset which is needed now since
  // |this| is reused on internals page reloads.
  optimization_guide_internals_page_factory_receiver_.reset();
  optimization_guide_internals_page_factory_receiver_.Bind(std::move(receiver));
}

void OptimizationGuideInternalsUI::CreatePageHandler(
    mojo::PendingRemote<optimization_guide_internals::mojom::Page> page) {
  optimization_guide_internals_page_handler_ =
      std::make_unique<OptimizationGuideInternalsPageHandlerImpl>(
          std::move(page), optimization_guide_logger_);
}

WEB_UI_CONTROLLER_TYPE_IMPL(OptimizationGuideInternalsUI)
