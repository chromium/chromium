// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_DEMO_HOST_DEMO_HOST_DISPLAY_CLIENT_H_
#define COMPONENTS_VIZ_DEMO_HOST_DEMO_HOST_DISPLAY_CLIENT_H_

#include "build/build_config.h"
#include "components/viz/host/host_display_client.h"

namespace demo {

class DemoHostDisplayClient : public viz::HostDisplayClient {
 public:
  explicit DemoHostDisplayClient(gfx::AcceleratedWidget widget)
      : viz::HostDisplayClient(widget) {}

  DemoHostDisplayClient(const DemoHostDisplayClient&) = delete;
  DemoHostDisplayClient& operator=(const DemoHostDisplayClient&) = delete;

  ~DemoHostDisplayClient() override = default;

#if BUILDFLAG(IS_WIN)
  void AddChildWindowToBrowser(gpu::SurfaceHandle child_window) override;
#endif
};

}  // namespace demo

#endif  // COMPONENTS_VIZ_DEMO_HOST_DEMO_HOST_DISPLAY_CLIENT_H_
