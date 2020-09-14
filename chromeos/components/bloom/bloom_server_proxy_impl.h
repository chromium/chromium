// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_BLOOM_SERVER_PROXY_IMPL_H_
#define CHROMEOS_COMPONENTS_BLOOM_BLOOM_SERVER_PROXY_IMPL_H_

#include "chromeos/components/bloom/bloom_server_proxy.h"

namespace chromeos {
namespace bloom {

class BloomServerProxyImpl : public BloomServerProxy {
 public:
  BloomServerProxyImpl();
  ~BloomServerProxyImpl() override;

  void AnalyzeProblem(const std::string& access_token,
                      const gfx::Image& screenshot,
                      Callback callback) override;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_BLOOM_SERVER_PROXY_IMPL_H_
