// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_logger.h"

#include <string>
#include <utility>

namespace multistep_filter {

ScopedLogMessage::ScopedLogMessage(MultistepFilterLogRouter* logger,
                                   std::string nav_id,
                                   LogEventType type,
                                   std::string source_etld_plus_1)
    : logger_(logger),
      entry_(std::move(nav_id), type, std::move(source_etld_plus_1)) {}

ScopedLogMessage::~ScopedLogMessage() {
  if (logger_) {
    logger_->RouteLogMessage(std::move(entry_));
  }
}

}  // namespace multistep_filter
