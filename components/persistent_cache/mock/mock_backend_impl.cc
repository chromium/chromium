// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/mock/mock_backend_impl.h"

#include "components/persistent_cache/backend_params.h"

namespace persistent_cache {

MockBackendImpl::MockBackendImpl(const BackendParams& backend_params) {}
MockBackendImpl::~MockBackendImpl() = default;

BackendType MockBackendImpl::GetType() const {
  return BackendType::kMock;
}

}  // namespace persistent_cache
