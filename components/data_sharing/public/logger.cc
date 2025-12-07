// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/public/logger.h"

namespace data_sharing {

Logger::Entry::Entry(base::Time event_time,
                     logger_common::mojom::LogSource log_source,
                     const std::string& source_file,
                     int source_line,
                     const std::string& message)
    : event_time(event_time),
      log_source(log_source),
      source_file(source_file),
      source_line(source_line),
      message(message) {}

bool Logger::Entry::operator==(const Entry& other) const {
  return event_time == other.event_time && log_source == other.log_source &&
         source_file == other.source_file && source_line == other.source_line &&
         message == other.message;
}

}  // namespace data_sharing
