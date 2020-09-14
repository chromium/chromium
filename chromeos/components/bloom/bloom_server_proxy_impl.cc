// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/bloom_server_proxy_impl.h"

namespace chromeos {
namespace bloom {

BloomServerProxyImpl::BloomServerProxyImpl() = default;
BloomServerProxyImpl::~BloomServerProxyImpl() = default;

void BloomServerProxyImpl::AnalyzeProblem(const std::string& access_token,
                                          const gfx::Image& screenshot,
                                          Callback callback) {
  // TODO(jeroendh): implement
}

}  // namespace bloom
}  // namespace chromeos
