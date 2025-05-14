// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"

#include <optional>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_histograms.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"

namespace web_app::test {

namespace {

using ComponentMetadataOrError =
    base::expected<IwaComponentMetadata, IwaComponentUpdateError>;

using ComponentUpdateFuture = base::test::TestFuture<ComponentMetadataOrError>;

class ComponentUpdateWaiter : public IwaKeyDistributionInfoProvider::Observer {
 public:
  using UpdateCallback = base::OnceCallback<void(ComponentMetadataOrError)>;

  // The waiter invokes the `on_update` callback when the updated
  // component's version matches the provided `version`, or on the first
  // update if no `version` is specified.
  explicit ComponentUpdateWaiter(
      UpdateCallback on_update,
      std::optional<base::Version> wait_until_version = std::nullopt)
      : on_update_(std::move(on_update)),
        version_(std::move(wait_until_version)) {
    obs_.Observe(IwaKeyDistributionInfoProvider::GetInstance());
  }

  // IwaKeyRotationInfoProvider::Observer:
  void OnComponentUpdateSuccess(const base::Version& version,
                                bool is_preloaded) override {
    if (version_ && version_ != version) {
      return;
    }
    std::move(on_update_)
        .Run(IwaComponentMetadata{.version = version,
                                  .is_preloaded = is_preloaded});
    obs_.Reset();
  }

  void OnComponentUpdateError(const base::Version& version,
                              IwaComponentUpdateError error) override {
    if (version_ && version_ != version) {
      return;
    }
    std::move(on_update_).Run(base::unexpected(error));
    obs_.Reset();
  }

 private:
  UpdateCallback on_update_;
  std::optional<base::Version> version_;
  base::ScopedObservation<IwaKeyDistributionInfoProvider,
                          IwaKeyDistributionInfoProvider::Observer>
      obs_{this};
};

}  // namespace

base::expected<void, IwaComponentUpdateError> UpdateKeyDistributionInfo(
    const base::Version& version,
    const base::FilePath& path) {
  ComponentUpdateFuture future;
  auto waiter = ComponentUpdateWaiter(future.GetCallback(), version);
  IwaKeyDistributionInfoProvider::GetInstance()->LoadKeyDistributionData(
      version, path, /*is_preloaded=*/false);
  ASSIGN_OR_RETURN((auto [loaded_version, is_preloaded]), future.Take());
  CHECK(version == loaded_version && !is_preloaded);
  return base::ok();
}

base::expected<void, IwaComponentUpdateError> UpdateKeyDistributionInfo(
    const base::Version& version,
    const IwaKeyDistribution& kd_proto) {
  base::ScopedTempDir component_install_dir;
  CHECK(component_install_dir.CreateUniqueTempDir());
  auto path = component_install_dir.GetPath().AppendASCII("krc");
  CHECK(base::WriteFile(path, kd_proto.SerializeAsString()));
  return UpdateKeyDistributionInfo(version, path);
}

base::expected<void, IwaComponentUpdateError> UpdateKeyDistributionInfo(
    const base::Version& version,
    const std::string& web_bundle_id,
    std::optional<base::span<const uint8_t>> expected_key) {
  IwaKeyDistribution key_distribution;
  IwaKeyRotations key_rotations;
  IwaKeyRotations::KeyRotationInfo kr_info;
  if (expected_key) {
    kr_info.set_expected_key(base::Base64Encode(*expected_key));
  }
  key_rotations.mutable_key_rotations()->emplace(web_bundle_id,
                                                 std::move(kr_info));
  *key_distribution.mutable_key_rotation_data() = std::move(key_rotations);
  return UpdateKeyDistributionInfo(version, key_distribution);
}

base::expected<void, IwaComponentUpdateError>
UpdateKeyDistributionInfoWithAllowlist(
    const base::Version& version,
    const std::vector<std::string>& managed_allowlist) {
  IwaKeyDistribution key_distribution;
  for (const auto& bundle_id : managed_allowlist) {
    auto& managed_allowlist_proto =
        *key_distribution.mutable_iwa_access_control()
             ->mutable_managed_allowlist();
    managed_allowlist_proto[bundle_id] = {};
  }
  return UpdateKeyDistributionInfo(version, key_distribution);
}

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
  auto waiter = std::make_unique<ComponentUpdateWaiter>(future.GetCallback());

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
      *base::WriteJson(base::Value::Dict()
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
  auto waiter = std::make_unique<ComponentUpdateWaiter>(future.GetCallback());
  component_updater::RegisterIwaKeyDistributionComponent(
      g_browser_process->component_updater());
  return future.Take();
}

}  // namespace web_app::test
