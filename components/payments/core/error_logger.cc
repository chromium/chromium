// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/error_logger.h"

#include "base/logging.h"

namespace payments {

ErrorLogger::ErrorLogger() = default;

ErrorLogger::~ErrorLogger() = default;

void ErrorLogger::DisableInTest() {
  enabled_ = false;
}

void ErrorLogger::Warn(const std::string& warning_message) const {
  if (enabled_)
    LOG(WARNING) << warning_message;
}

void ErrorLogger::Error(const std::string& error_message) const {
  if (enabled_)
    LOG(ERROR) << error_message;
}

}  // namespace payments
