// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_RENDERER_NO_STATE_PREFETCH_OBSERVER_H_
#define COMPONENTS_NO_STATE_PREFETCH_RENDERER_NO_STATE_PREFETCH_OBSERVER_H_

#include "base/observer_list_types.h"

namespace prerender {

class NoStatePrefetchObserver : public base::CheckedObserver {
 public:
  // Set NoStatePrefetching mode for the plugin.
  virtual void SetIsNoStatePrefetching(bool is_no_state_prefetching) = 0;

 protected:
  ~NoStatePrefetchObserver() override = default;
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_RENDERER_NO_STATE_PREFETCH_OBSERVER_H_
