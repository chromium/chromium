// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_android_manifest_asset.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"

namespace optimization_guide {

FakeAndroidManifestAsset::FakeAndroidManifestAsset() = default;
FakeAndroidManifestAsset::~FakeAndroidManifestAsset() = default;

void FakeAndroidManifestAsset::AddSolutionConfig(const std::string& use_case,
                                                 proto::SolutionConfig config) {
  auto feature = GetFeatureForUseCase(use_case);
  CHECK(feature);
  CHECK_EQ(config.feature().feature(), ToModelExecutionFeatureProto(*feature));
  configs_.insert_or_assign(*feature,
                            base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
                                std::move(*config.mutable_feature())));
}

AndroidManifest FakeAndroidManifestAsset::CreateManifest() const {
  return AndroidManifest::CreateFromComponent(configs_);
}

}  // namespace optimization_guide
