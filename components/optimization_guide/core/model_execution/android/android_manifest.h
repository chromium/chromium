// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_ANDROID_MANIFEST_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_ANDROID_MANIFEST_H_

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-forward.h"

namespace optimization_guide {

class OnDeviceModelFeatureAdapter;

// Describes how to construct ModelBroker solutions using AICore-owned models.
class AndroidManifest {
 public:
  using FeatureRoutingConfigMap =
      base::flat_map<mojom::OnDeviceFeature, proto::Any>;

  AndroidManifest(AndroidManifest&&);
  AndroidManifest& operator=(AndroidManifest&&);
  ~AndroidManifest();

  static AndroidManifest CreateFromBuiltin();

  static FeatureRoutingConfigMap GetFeatureRoutingConfigs();

  scoped_refptr<const OnDeviceModelFeatureAdapter> SelectConfig(
      mojom::OnDeviceFeature feature) const;

 private:
  AndroidManifest();

  base::flat_map<mojom::OnDeviceFeature,
                 scoped_refptr<const OnDeviceModelFeatureAdapter>>
      configs_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_ANDROID_MANIFEST_H_
