// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/ui_controllers/toolbar/icon_handle.h"

#include <cstdint>
#include <utility>

#include "base/memory/ref_counted.h"

namespace toolbar_ui_api {

IconHandle::IconHandle() = default;
IconHandle::IconHandle(scoped_refptr<Provider> provider)
    : provider_(std::move(provider)) {}
IconHandle::IconHandle(const IconHandle& other) = default;
IconHandle::IconHandle(IconHandle&& other) = default;
IconHandle::~IconHandle() = default;

IconHandle& IconHandle::operator=(const IconHandle& other) = default;
IconHandle& IconHandle::operator=(IconHandle&& other) = default;

IconHandleId IconHandle::HandleId() const {
  return provider_ ? provider_->HandleId() : IconHandleId(kNullIconHandleId);
}

}  // namespace toolbar_ui_api
