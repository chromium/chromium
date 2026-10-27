// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_ANDROID_MANIFEST_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_ANDROID_MANIFEST_H_

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/token.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-forward.h"

namespace optimization_guide {

class OnDeviceModelFeatureAdapter;

// Describes how to construct ModelBroker solutions using AICore-owned models.
class AndroidManifest {
 public:
  using FeatureRoutingConfigMap =
      base::flat_map<mojom::OnDeviceFeature, proto::Any>;
  using ConfigMap =
      base::flat_map<mojom::OnDeviceFeature,
                     scoped_refptr<const OnDeviceModelFeatureAdapter>>;

  // Errors for manifest parsing failures.
  // TODO(crbug.com/575550015): Add detailed error reasons when integrating with
  // component delivery.
  enum class ParseError { kInvalidConfig };

  AndroidManifest(const AndroidManifest&);
  AndroidManifest& operator=(const AndroidManifest&);
  AndroidManifest(AndroidManifest&&);
  AndroidManifest& operator=(AndroidManifest&&);
  ~AndroidManifest();

  static AndroidManifest CreateFromBuiltin();
  // Creates a manifest using component configs, with built-in fallbacks for
  // features omitted from the component.
  // TODO(crbug.com/575550015): Load component snapshots from a directory when
  // component delivery is connected. Keep construction in memory for now.
  static AndroidManifest CreateFromComponent(ConfigMap component_configs);

  static FeatureRoutingConfigMap GetFeatureRoutingConfigs();

  scoped_refptr<const OnDeviceModelFeatureAdapter> SelectConfig(
      mojom::OnDeviceFeature feature) const;

  // Unique snapshot ID for this manifest.
  const base::Token& id() const { return id_; }

 private:
  AndroidManifest();

  ConfigMap configs_;
  base::Token id_ = base::Token::CreateRandom();
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_ANDROID_MANIFEST_H_
