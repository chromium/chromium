// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_PROVIDER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}

namespace optimization_guide {

// Notifies consumers when an installed config directory is available.
// Callbacks run on the subscribing sequence.
class ManifestProvider {
 public:
  using ReadyCallback = base::RepeatingCallback<void(base::FilePath)>;
  virtual ~ManifestProvider() = default;
  virtual base::CallbackListSubscription ListenForManifestReady(
      ReadyCallback on_ready) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_PROVIDER_H_
