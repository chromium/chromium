// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
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
// File name of the manifest proto within the manifest component directory.
inline constexpr const base::FilePath::CharType kManifestFileName[] =
    FILE_PATH_LITERAL("manifest.binarypb");

// Manifest is a C++ representation of the manifest proto. It provides APIs for
// getting the information needed by the model broker implementation.
class Manifest final {
 public:
  using AssetId = std::string;
  using UseCaseName = std::string;

  enum class ParseError {
    kFileNotFound,
    kProtoParseError,
    kDuplicateIdentifier,
    kMissingIdentifier,
    kConflictingComponent,
  };

  // LINT.IfChange(UninstallReason)
  enum class UninstallReason {
    kUnknown = 0,
    kInsufficientDisk = 1,
    kDisallowedByPolicy = 2,
    kDeviceNotCapable = 3,
    kParseError = 4,
    kDisallowedByUser = 5,
    kObsolete = 6,
    kMaxValue = kObsolete,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:OnDeviceModelUninstallReason)

  static base::expected<Manifest, ParseError> Create(
      const base::FilePath& directory,
      proto::Manifest manifest,
      DeviceCategory device_category);

  static void Load(
      const base::FilePath& directory,
      DeviceCategory device_category,
      base::OnceCallback<void(base::expected<Manifest, ParseError>)> callback);

  // Constructs an empty manifest indicating no recipes are supported and all
  // assets should be uninstalled, for the indicated `uninstall_reason`.
  explicit Manifest(UninstallReason uninstall_reason);
  ~Manifest();

  Manifest(const Manifest&);
  Manifest& operator=(const Manifest&);
  Manifest(Manifest&&);
  Manifest& operator=(Manifest&&);

  // Returns true if the manifest defines any assets.
  bool HasAssets() const;

  UninstallReason uninstall_reason() const { return uninstall_reason_; }

  // Returns the OnDemandComponent for the given public key, or nullopt if it is
  // not found in the manifest.
  const proto::OnDemandComponent* GetAssetByPublicKey(
      const std::string& public_key) const;

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
  const base::FilePath& GetDirectory() const { return directory_; }

 private:
  Manifest(base::FilePath directory,
           proto::DeviceCategoryConfig device_category_config,
           proto::Recipes recipes,
           proto::Assets assets);

  base::FilePath directory_;
  // Manifest content narrowed down to what is relevant for the device category.
  proto::DeviceCategoryConfig device_category_config_;
  proto::Recipes recipes_;
  proto::Assets assets_;
  // A map from public key to the factory's manifest's OnDemandComponent.
  absl::flat_hash_map<std::string, std::string> asset_id_by_public_key_;

  // The reason to give for uninstalling an asset not listed in this manifest.
  UninstallReason uninstall_reason_ = UninstallReason::kObsolete;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_H_
