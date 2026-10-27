// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_ANDROID_MANIFEST_ASSET_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_ANDROID_MANIFEST_ASSET_H_

#include <string>

#include "components/optimization_guide/core/model_execution/android/android_manifest.h"
#include "components/optimization_guide/proto/manifest.pb.h"

namespace optimization_guide {

// Models a fake android manifest snapshot.
class FakeAndroidManifestAsset {
 public:
  FakeAndroidManifestAsset();
  ~FakeAndroidManifestAsset();
  void AddSolutionConfig(const std::string& use_case,
                         proto::SolutionConfig config);
  AndroidManifest CreateManifest() const;

 private:
  AndroidManifest::ConfigMap configs_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_ANDROID_MANIFEST_ASSET_H_
