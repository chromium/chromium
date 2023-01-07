// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/gpu/cast_content_gpu_client.h"

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/service/display/overlay_strategy_underlay_cast.h"
#include "content/public/child/child_thread.h"

namespace chromecast {
namespace shell {

CastContentGpuClient::CastContentGpuClient() = default;
CastContentGpuClient::~CastContentGpuClient() = default;

// static
std::unique_ptr<CastContentGpuClient> CastContentGpuClient::Create() {
  return base::WrapUnique(new CastContentGpuClient());
}

void CastContentGpuClient::PostCompositorThreadCreated(
    base::SingleThreadTaskRunner* task_runner) {
  DCHECK(task_runner);
  mojo::PendingRemote<chromecast::media::mojom::VideoGeometrySetter>
      video_geometry_setter;
  content::ChildThread::Get()->BindHostReceiver(
      video_geometry_setter.InitWithNewPipeAndPassReceiver());

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &viz::OverlayStrategyUnderlayCast::ConnectVideoGeometrySetter,
          std::move(video_geometry_setter)));
}

}  // namespace shell
}  // namespace chromecast
