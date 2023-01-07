// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/status.h"

namespace ntp_snippets {

Status::Status(StatusCode status_code, const std::string& message)
    : code(status_code), message(message) {}

Status Status::Success() {
  return Status(StatusCode::SUCCESS, std::string());
}

Status::~Status() = default;

}  // namespace ntp_snippets
