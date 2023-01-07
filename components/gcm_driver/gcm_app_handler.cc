// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_app_handler.h"

namespace gcm {

GCMAppHandler::GCMAppHandler() = default;
GCMAppHandler::~GCMAppHandler() = default;

void GCMAppHandler::OnMessageDecryptionFailed(
    const std::string& app_id,
    const std::string& message_id,
    const std::string& error_message) {}

bool GCMAppHandler::CanHandle(const std::string& app_id) const {
  return false;
}

}  // namespace gcm
