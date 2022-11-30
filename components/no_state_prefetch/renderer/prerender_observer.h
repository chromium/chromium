// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_RENDERER_PRERENDER_OBSERVER_H_
#define COMPONENTS_NO_STATE_PREFETCH_RENDERER_PRERENDER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace prerender {

class PrerenderObserver : public base::CheckedObserver {
 public:
  // Set prerendering mode for the plugin.
  virtual void SetIsPrerendering(bool is_prerendering) = 0;

 protected:
  ~PrerenderObserver() override = default;
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_RENDERER_PRERENDER_OBSERVER_H_
