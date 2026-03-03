// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_H_

#include <optional>
#include <string>
#include <vector>

#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace optimization_guide {

enum class DeviceCategory {
  kGpuHighTier = 1,
  kGpuLowTier = 2,
  kCpu = 3,
};

// Stringifies a device category as a key into the manifest's category_configs.
extern std::ostream& operator<<(std::ostream& stream,
                                DeviceCategory device_category);

inline constexpr std::string kManifestAssetName = "manifest";

// Manifest is a C++ representation of the manifest proto. It provides APIs for
// getting the information needed by the model broker implementation.
class Manifest final {
 public:
  using AssetId = std::string;
  using UseCaseName = std::string;

  enum class ParseError {
    kDuplicateIdentifier,
    kMissingIdentifier,
    kConflictingComponent,
  };

  static base::expected<Manifest, ParseError> Create(
      proto::Manifest manifest,
      DeviceCategory device_category);

  ~Manifest();

  Manifest(const Manifest&);
  Manifest& operator=(const Manifest&);
  Manifest(Manifest&&);
  Manifest& operator=(Manifest&&);

  // Returns the identifiers of the Assets required for a single use case.
  // Returns nullopt if the use case is not defined in the manifest.
  std::optional<absl::flat_hash_set<AssetId>> GetRequiredAssets(
      const UseCaseName& use_case) const;
  // Returns the identifiers of all of the Assets for the given use_cases.
  // If a use case is not defined in the manifest, it is ignored.
  absl::flat_hash_set<AssetId> GetRequiredAssets(
      const std::vector<UseCaseName>& use_cases) const;

  const proto::DeviceCategoryConfig& GetDeviceCategoryConfig() const {
    return device_category_config_;
  }
  const proto::Recipes& GetRecipes() const { return recipes_; }
  const proto::Assets& GetAssets() const { return assets_; }

 private:
  Manifest(proto::DeviceCategoryConfig device_category_config,
           proto::Recipes recipes,
           proto::Assets assets);

  // Manifest content narrowed down to what is relevant for the device category.
  proto::DeviceCategoryConfig device_category_config_;
  proto::Recipes recipes_;
  proto::Assets assets_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_H_
