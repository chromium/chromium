// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

SystemLogsSource::SystemLogsSource(const std::string& source_name)
    : source_name_(source_name) {}

SystemLogsSource::~SystemLogsSource() = default;

}  // namespace system_logs
