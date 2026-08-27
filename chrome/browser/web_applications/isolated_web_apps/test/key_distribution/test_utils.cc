// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/installer_policies/iwa_key_distribution_component_installer_policy.h"

namespace web_app::test {

namespace {

using ComponentMetadataOrError =
    base::expected<IwaComponentMetadata, IwaComponentUpdateError>;

using ComponentUpdateFuture = base::test::TestFuture<ComponentMetadataOrError>;

}  // namespace

base::expected<void, IwaComponentUpdateError>
InstallIwaKeyDistributionComponent(const base::Version& version,
                                   const IwaKeyDistribution& kd_proto) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  CHECK(base::FeatureList::IsEnabled(
      component_updater::kIwaKeyDistributionComponent))
      << "The `IwaKeyDistribution` feature must be enabled for the component "
         "installation to succeed.";
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  using Installer =
      component_updater::IwaKeyDistributionComponentInstallerPolicy;
  base::ScopedAllowBlockingForTesting allow_blocking;

  ComponentUpdateFuture future;
  auto waiter = SetOnComponentUpdatedForTesting(future.GetRepeatingCallback());

  // Write the serialized proto to the attestation list file.
  auto install_dir = [&] {
    base::FilePath component_updater_dir;
    base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                           &component_updater_dir);
    return component_updater_dir.Append(Installer::kRelativeInstallDirName)
        .AppendASCII(version.GetString());
  }();

  CHECK(base::CreateDirectory(install_dir));

  CHECK(base::WriteFile(install_dir.Append(Installer::kDataFileName),
                        kd_proto.SerializeAsString()));

  // Write a manifest file. This is needed for component updater to detect any
  // existing component on disk.
  CHECK(base::WriteFile(
      install_dir.Append(FILE_PATH_LITERAL("manifest.json")),
      *base::WriteJson(base::DictValue()
                           .Set("manifest_version", 1)
                           .Set("name", Installer::kManifestName)
                           .Set("version", version.GetString()))));

  component_updater::RegisterIwaKeyDistributionComponent(
      g_browser_process->component_updater());
  ASSIGN_OR_RETURN((auto [loaded_version, is_preloaded]), future.Take());

  // `install_dir` is no longer necessary after the installation has completed.
  CHECK(base::DeletePathRecursively(install_dir));

  if (version != loaded_version || is_preloaded) {
    return base::unexpected(IwaComponentUpdateError::kStaleVersion);
  }

  return base::ok();
}

base::expected<void, IwaComponentUpdateError>
InstallIwaKeyDistributionComponent(
    const base::Version& version,
    const std::string& web_bundle_id,
    std::optional<base::span<const uint8_t>> expected_key) {
  IwaKeyRotations::KeyRotationInfo kr_info;
  if (expected_key) {
    kr_info.set_expected_key(base::Base64Encode(*expected_key));
  }

  IwaKeyRotations key_rotations;
  key_rotations.mutable_key_rotations()->emplace(web_bundle_id,
                                                 std::move(kr_info));

  IwaKeyDistribution key_distribution;
  *key_distribution.mutable_key_rotation_data() = std::move(key_rotations);

  return InstallIwaKeyDistributionComponent(version, key_distribution);
}

base::expected<IwaComponentMetadata, IwaComponentUpdateError>
RegisterIwaKeyDistributionComponentAndWaitForLoad() {
  ComponentUpdateFuture future;
  auto waiter = SetOnComponentUpdatedForTesting(future.GetRepeatingCallback());
  component_updater::RegisterIwaKeyDistributionComponent(
      g_browser_process->component_updater());
  return future.Take();
}

}  // namespace web_app::test
