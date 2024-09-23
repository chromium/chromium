// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_GPU_CLIENT_DELEGATE_H_
#define COMPONENTS_VIZ_HOST_GPU_CLIENT_DELEGATE_H_

namespace viz {

class GpuHostImpl;

// Delegate interface that GpuClient uses to get the current GpuHost instance.
// This function is guaranteed to be called on the thread corresponding to
// GpuClient's task runner.
class GpuClientDelegate {
 public:
  virtual ~GpuClientDelegate() {}

  // Returns the current instance of GpuHostImpl. If GPU service is not running,
  // tries to launch it. If the launch is unsuccessful, returns nullptr.
  virtual GpuHostImpl* EnsureGpuHost() = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_GPU_CLIENT_DELEGATE_H_
