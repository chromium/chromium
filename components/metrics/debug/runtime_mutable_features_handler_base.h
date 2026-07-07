// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEBUG_RUNTIME_MUTABLE_FEATURES_HANDLER_BASE_H_
#define COMPONENTS_METRICS_DEBUG_RUNTIME_MUTABLE_FEATURES_HANDLER_BASE_H_

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "base/values.h"

namespace variations {
class VariationsService;
}

namespace metrics {

// Platform-agnostic logic for the Runtime-Mutable-Features tab of
// chrome://metrics-internals.
class RuntimeMutableFeaturesHandlerBase {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void ResolvePageCallback(const base::ValueView callback_id,
                                     const base::ValueView response) = 0;
  };

  RuntimeMutableFeaturesHandlerBase(
      Delegate* delegate,
      variations::VariationsService* variations_service);

  RuntimeMutableFeaturesHandlerBase(const RuntimeMutableFeaturesHandlerBase&) =
      delete;
  RuntimeMutableFeaturesHandlerBase& operator=(
      const RuntimeMutableFeaturesHandlerBase&) = delete;

  ~RuntimeMutableFeaturesHandlerBase();

  void HandleFetchRuntimeMutableFeatures(const base::Value& callback_id);

  void HandleIsSeedFetchingPaused(const base::Value& callback_id);

  void HandleSetSeedFetchingPaused(const base::Value& callback_id, bool paused);

  void HandleUploadSeed(const base::Value& callback_id,
                        const base::Value& seed_value);

 private:
  const raw_ptr<Delegate> delegate_;
  const raw_ptr<variations::VariationsService> variations_service_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DEBUG_RUNTIME_MUTABLE_FEATURES_HANDLER_BASE_H_
