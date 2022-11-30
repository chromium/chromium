// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_CONTEXT_LOST_OBSERVER_H_
#define COMPONENTS_VIZ_COMMON_GPU_CONTEXT_LOST_OBSERVER_H_

namespace viz {

// Observers a ContextProvider and is notified when the context is lost.
class ContextLostObserver {
 public:
  virtual void OnContextLost() = 0;

 protected:
  virtual ~ContextLostObserver() = default;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_CONTEXT_LOST_OBSERVER_H_
