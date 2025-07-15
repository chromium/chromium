// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/console_message_logger/headless_console_message_logger.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "content/public/common/content_features.h"

namespace headless {

void LogConsoleMessage(blink::mojom::ConsoleMessageLevel log_level,
                       const std::u16string& message,
                       int32_t line_number,
                       const std::u16string& source_id) {
  const int32_t resolved_level =
      content::ConsoleMessageLevelToLogSeverity(log_level);
  if (resolved_level < ::logging::GetMinLogLevel()) {
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kLogJsConsoleMessages)) {
    return;
  }

  logging::LogMessage("CONSOLE", line_number, resolved_level).stream()
      << "\"" << message << "\", source: " << source_id << " (" << line_number
      << ")";
}

}  // namespace headless
