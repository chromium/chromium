// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_INTERNALS_WEBUI_OPTIMIZATION_GUIDE_INTERNALS_PAGE_HANDLER_IMPL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_INTERNALS_WEBUI_OPTIMIZATION_GUIDE_INTERNALS_PAGE_HANDLER_IMPL_H_

#include <string>

#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

// Handler for the internals page to receive and forward the log messages.
class OptimizationGuideInternalsPageHandlerImpl
    : public OptimizationGuideLogger::Observer {
 public:
  OptimizationGuideInternalsPageHandlerImpl(
      mojo::PendingRemote<optimization_guide_internals::mojom::Page> page,
      OptimizationGuideLogger* optimization_guide_logger);
  ~OptimizationGuideInternalsPageHandlerImpl() override;

  OptimizationGuideInternalsPageHandlerImpl(
      const OptimizationGuideInternalsPageHandlerImpl&) = delete;
  OptimizationGuideInternalsPageHandlerImpl& operator=(
      const OptimizationGuideInternalsPageHandlerImpl&) = delete;

 private:
  // optimization_guide::OptimizationGuideLogger::Observer overrides.
  void OnLogMessageAdded(base::Time event_time,
                         optimization_guide_common::mojom::LogSource log_source,
                         const std::string& source_file,
                         int source_line,
                         const std::string& message) override;

  mojo::Remote<optimization_guide_internals::mojom::Page> page_;

  // Logger to receive the debug logs from the optimization guide service. Not
  // owned. Guaranteed to outlive |this|, since the logger is owned by the
  // optimization guide keyed service, while |this| is part of
  // RenderFrameHostImpl::WebUIImpl.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;
};

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_INTERNALS_WEBUI_OPTIMIZATION_GUIDE_INTERNALS_PAGE_HANDLER_IMPL_H_
