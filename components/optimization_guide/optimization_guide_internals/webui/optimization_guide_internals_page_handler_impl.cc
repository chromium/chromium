// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals_page_handler_impl.h"

#include "base/time/time.h"

OptimizationGuideInternalsPageHandlerImpl::
    OptimizationGuideInternalsPageHandlerImpl(
        mojo::PendingRemote<optimization_guide_internals::mojom::Page> page,
        OptimizationGuideLogger* optimization_guide_logger)
    : page_(std::move(page)),
      optimization_guide_logger_(optimization_guide_logger) {
  if (optimization_guide_logger_)
    optimization_guide_logger_->AddObserver(this);
}

OptimizationGuideInternalsPageHandlerImpl::
    ~OptimizationGuideInternalsPageHandlerImpl() {
  if (optimization_guide_logger_)
    optimization_guide_logger_->RemoveObserver(this);
}

void OptimizationGuideInternalsPageHandlerImpl::OnLogMessageAdded(
    base::Time event_time,
    optimization_guide_common::mojom::LogSource log_source,
    const std::string& source_file,
    int source_line,
    const std::string& message) {
  page_->OnLogMessageAdded(event_time, log_source, source_file, source_line,
                           message);
}
