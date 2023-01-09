// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GPU_CAST_CONTENT_GPU_CLIENT_H_
#define CHROMECAST_GPU_CAST_CONTENT_GPU_CLIENT_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "content/public/gpu/content_gpu_client.h"

namespace chromecast {
namespace shell {

class CastContentGpuClient : public content::ContentGpuClient {
 public:
  static std::unique_ptr<CastContentGpuClient> Create();

  CastContentGpuClient(const CastContentGpuClient&) = delete;
  CastContentGpuClient& operator=(const CastContentGpuClient&) = delete;

  ~CastContentGpuClient() override;

  // content::ContentGpuClient:
  void PostCompositorThreadCreated(
      base::SingleThreadTaskRunner* task_runner) override;

 protected:
  CastContentGpuClient();
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_GPU_CAST_CONTENT_GPU_CLIENT_H_
