// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_core_host_remote.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ip_protection {

IpProtectionCoreHostRemote::IpProtectionCoreHostRemote(
    mojo::PendingRemote<ip_protection::mojom::CoreHost> core_host) {
  DCHECK(core_host.is_valid());
  core_host_.Bind(std::move(core_host));
}

IpProtectionCoreHostRemote::~IpProtectionCoreHostRemote() = default;

}  // namespace ip_protection
