// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/capabilities_service_impl.h"

#include <set>

#include "base/callback.h"
#include "url/origin.h"

CapabilitiesServiceImpl::CapabilitiesServiceImpl() = default;

CapabilitiesServiceImpl::~CapabilitiesServiceImpl() = default;

void CapabilitiesServiceImpl::QueryPasswordChangeScriptAvailability(
    const std::vector<url::Origin>& origins,
    ResponseCallback callback) {
  std::move(callback).Run(std::set<url::Origin>());
}
