// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/console_message_logger/headless_console_message_logger.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "content/public/browser/console_message.h"
#include "content/public/common/content_features.h"

namespace headless {

namespace {

const std::u16string TruncateDataUrl(const std::u16string& data_url) {
  DCHECK(data_url.starts_with(u"data:"));
  // data:[<media-type>][;base64],<data>
  size_t data_index = data_url.find(u',');
  if (data_index == std::string::npos) {
    data_index = sizeof("data:") - 1;
  }
  return data_url.substr(0, data_index) + u"...";
}

}  // namespace

void LogConsoleMessage(blink::mojom::ConsoleMessageLevel log_level,
                       const std::u16string& message,
                       int32_t line_number,
                       bool is_builtin_component,
                       const std::u16string& source_id) {
  const int32_t resolved_level =
      is_builtin_component
          ? content::ConsoleMessageLevelToLogSeverity(log_level)
          : ::logging::LOGGING_INFO;
  if (resolved_level < ::logging::GetMinLogLevel()) {
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kLogJsConsoleMessages)) {
    return;
  }

  logging::LogMessage("CONSOLE", line_number, resolved_level).stream()
      << "\"" << message << "\", source: "
      << (source_id.starts_with(u"data:") ? TruncateDataUrl(source_id)
                                          : source_id)
      << " (" << line_number << ")";
}

}  // namespace headless
