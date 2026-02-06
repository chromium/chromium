// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/logging/logs_page_handler.h"

#include "components/omnibox/common/logger.h"

namespace omnibox::logging {

LogsPageHandler::LogsPageHandler(
    mojo::PendingReceiver<omnibox::logging::mojom::PageHandler> receiver,
    mojo::PendingRemote<omnibox::logging::mojom::Page> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {
  logger_observation_.Observe(omnibox::Logger::GetInstance());
}

LogsPageHandler::~LogsPageHandler() = default;

void LogsPageHandler::OnLogMessageAdded(base::Time event_time,
                                        const std::string& tag,
                                        const std::string& source_file,
                                        uint32_t source_line,
                                        const std::string& message) {
  page_->OnLogMessageAdded(event_time, tag, source_file, source_line, message);
}

}  // namespace omnibox::logging
