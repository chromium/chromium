// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"

namespace safe_browsing {

OhttpKeyService::OhttpKeyService() = default;

OhttpKeyService::~OhttpKeyService() = default;

void OhttpKeyService::GetOhttpKey(Callback callback) {
  std::move(callback).Run(absl::nullopt);
}

}  // namespace safe_browsing
