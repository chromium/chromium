// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_SERVER_BLOOM_SERVER_PROXY_IMPL_H_
#define CHROMEOS_COMPONENTS_BLOOM_SERVER_BLOOM_SERVER_PROXY_IMPL_H_

#include <memory>

#include "chromeos/components/bloom/server/bloom_server_proxy.h"

namespace chromeos {
namespace bloom {

class BloomURLLoader;

class BloomServerProxyImpl : public BloomServerProxy {
 public:
  explicit BloomServerProxyImpl(std::unique_ptr<BloomURLLoader> url_loader);
  ~BloomServerProxyImpl() override;

  void AnalyzeProblem(const std::string& access_token,
                      const gfx::Image& screenshot,
                      Callback callback) override;

  BloomURLLoader* url_loader() { return url_loader_.get(); }

 private:
  class Worker;

  void OnWorkerComplete(Worker* worker);

  std::unique_ptr<BloomURLLoader> url_loader_;
  std::unique_ptr<Worker> worker_;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_SERVER_BLOOM_SERVER_PROXY_IMPL_H_
