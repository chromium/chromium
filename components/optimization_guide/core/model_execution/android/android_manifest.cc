// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/android/android_manifest.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "components/optimization_guide/core/model_execution/configs/summarize_solution_config.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/proto/feature_configs.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"

namespace optimization_guide {
namespace {

template <typename Config>
proto::Any MakeFeatureRoutingConfig(mojom::OnDeviceFeature feature) {
  Config config;
  config.set_default_use_case(ToUseCaseName(feature));
  proto::Any packed_config;
  packed_config.set_type_url(
      base::StrCat({"type.googleapis.com/", config.GetTypeName()}));
  config.SerializeToString(packed_config.mutable_value());
  return packed_config;
}

}  // namespace

AndroidManifest::AndroidManifest() = default;
AndroidManifest::AndroidManifest(const AndroidManifest&) = default;
AndroidManifest& AndroidManifest::operator=(const AndroidManifest&) = default;
AndroidManifest::AndroidManifest(AndroidManifest&&) = default;
AndroidManifest& AndroidManifest::operator=(AndroidManifest&&) = default;
AndroidManifest::~AndroidManifest() = default;

AndroidManifest AndroidManifest::CreateFromBuiltin() {
  AndroidManifest manifest;
  auto summarize_config = BuildAndroidSummarizeSolutionConfig();
  manifest.configs_.emplace(
      mojom::OnDeviceFeature::kSummarize,
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
          std::move(*summarize_config.mutable_feature())));
  return manifest;
}

AndroidManifest AndroidManifest::CreateFromComponent(
    ConfigMap component_configs) {
  // Keep built-in fallbacks for features omitted from the component snapshot.
  auto manifest = CreateFromBuiltin();
  for (auto& [feature, config] : component_configs) {
    // Callers must supply validated adapters keyed by their matching feature.
    CHECK(config);
    CHECK_EQ(config->config().feature(), ToModelExecutionFeatureProto(feature));
    manifest.configs_.insert_or_assign(feature, std::move(config));
  }
  return manifest;
}

AndroidManifest::FeatureRoutingConfigMap
AndroidManifest::GetFeatureRoutingConfigs() {
  FeatureRoutingConfigMap configs;
  configs.emplace(mojom::OnDeviceFeature::kSummarize,
                  MakeFeatureRoutingConfig<proto::SummarizerFeatureConfig>(
                      mojom::OnDeviceFeature::kSummarize));
  return configs;
}

scoped_refptr<const OnDeviceModelFeatureAdapter> AndroidManifest::SelectConfig(
    mojom::OnDeviceFeature feature) const {
  auto it = configs_.find(feature);
  if (it == configs_.end()) {
    return nullptr;
  }
  return it->second;
}

}  // namespace optimization_guide
