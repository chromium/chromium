// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/fcm_driver.h"

#include "base/check.h"

namespace fcm {

FcmDriver::FcmDriver() = default;

FcmDriver::~FcmDriver() = default;

void FcmDriver::AddAppHandler(const std::string& app_id,
                              FcmAppHandler* handler) {
  CHECK(handler);
  CHECK(app_handlers_.find(app_id) == app_handlers_.end());
  app_handlers_[app_id] = handler;
}

void FcmDriver::RemoveAppHandler(const std::string& app_id,
                                 FcmAppHandler* handler) {
  CHECK(handler);
  auto it = app_handlers_.find(app_id);
  CHECK(it != app_handlers_.end() && it->second == handler);
  app_handlers_.erase(it);
}

}  // namespace fcm
