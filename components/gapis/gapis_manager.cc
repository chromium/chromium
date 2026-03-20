// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gapis/gapis_manager.h"

#include "base/no_destructor.h"

namespace gapis {

GapisManager::~GapisManager() = default;

// static
GapisManager* GapisManager::GetInstance() {
  static base::NoDestructor<GapisManager> instance;
  return instance.get();
}

GapisManager::GapisManager() = default;

void GapisManager::SetAppToken(const std::string& app_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  app_token_ = app_token;
}

std::string GapisManager::GetAppToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return app_token_;
}

}  // namespace gapis
