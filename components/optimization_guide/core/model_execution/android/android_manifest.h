// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_ANDROID_MANIFEST_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_ANDROID_MANIFEST_H_

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/token.h"
#include "base/types/expected.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-forward.h"

namespace optimization_guide {

class OnDeviceModelFeatureAdapter;

// The Summarizer config file name in the component installation directory.
inline constexpr base::FilePath::CharType kAndroidSummarizerConfigFileName[] =
    FILE_PATH_LITERAL("android_summarizer_solution_config.binarypb");

// Describes how to construct ModelBroker solutions using AICore-owned models.
class AndroidManifest {
 public:
  using FeatureRoutingConfigMap =
      base::flat_map<mojom::OnDeviceFeature, proto::Any>;
  using ConfigMap =
      base::flat_map<mojom::OnDeviceFeature,
                     scoped_refptr<const OnDeviceModelFeatureAdapter>>;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(LoadResult)
  enum class LoadResult {
    kSuccess = 0,
    kReadFailed = 1,
    kParseFailed = 2,
    kMissingFeature = 3,
    kFeatureMismatch = 4,
    kMaxValue = kFeatureMismatch,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:OptimizationGuideAndroidManifestLoadResult)

  AndroidManifest(const AndroidManifest&);
  AndroidManifest& operator=(const AndroidManifest&);
  AndroidManifest(AndroidManifest&&);
  AndroidManifest& operator=(AndroidManifest&&);
  ~AndroidManifest();

  // Reads and validates the component config from an installed directory.
  static base::expected<proto::SolutionConfig, LoadResult> Load(
      const base::FilePath& directory);

  static AndroidManifest CreateFromBuiltin();
  // Creates a manifest using component configs, with built-in fallbacks for
  // features omitted from the component.
  // TODO(crbug.com/575550015): Migrate callers to the Load interface and
  // remove this public method.
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
