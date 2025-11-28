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
#include "base/notimplemented.h"
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
        wait_until_version_(std::move(wait_until_version)) {
    obs_.Observe(&IwaKeyDistributionInfoProvider::GetInstance());
  }

  // IwaKeyDistributionInfoProvider::Observer:
  void OnComponentUpdateSuccess(bool is_preloaded) override {
    const std::optional<base::Version> loaded_version =
        IwaKeyDistributionInfoProvider::GetInstance().GetVersion();
    CHECK(loaded_version.has_value());

    if (wait_until_version_.has_value() &&
        *wait_until_version_ != *loaded_version) {
      return;
    }
    std::move(on_update_)
        .Run(IwaComponentMetadata{.version = *loaded_version,
                                  .is_preloaded = is_preloaded});
    obs_.Reset();
  }

  void OnComponentUpdateError(IwaComponentUpdateError error) override {
    std::move(on_update_).Run(base::unexpected(error));
    obs_.Reset();
  }

 private:
  UpdateCallback on_update_;
  std::optional<base::Version> wait_until_version_;
  base::ScopedObservation<IwaKeyDistributionInfoProvider,
                          IwaKeyDistributionInfoProvider::Observer>
      obs_{this};
};

}  // namespace

base::expected<void, IwaComponentUpdateError> KeyDistributionComponent::
    KeyDistributionComponent::UploadFromComponentFolder() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return UpdateKeyDistributionInfo(version, component_data);
}

void KeyDistributionComponent::KeyDistributionComponent::
    InjectComponentDataDirectly() {
  IwaKeyDistributionInfoProvider::GetInstance().SetComponentDataForTesting(
      version, is_preloaded, component_data);
}

KeyDistributionComponentBuilder::KeyDistributionComponentBuilder(
    const base::Version& component_version)
    : component_(/*version=*/component_version,
                 /*is_preloaded=*/false,
                 /*data=*/IwaKeyDistribution{}) {}

KeyDistributionComponentBuilder::~KeyDistributionComponentBuilder() = default;

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::AddToKeyRotations(
    const web_package::SignedWebBundleId& web_bundle_id,
    std::optional<std::vector<uint8_t>> expected_key) & {
  IwaKeyRotations::KeyRotationInfo kr_info_proto;
  if (expected_key.has_value()) {
    kr_info_proto.set_expected_key(base::Base64Encode(*expected_key));
  }
  (*component_.component_data.mutable_key_rotation_data()
        ->mutable_key_rotations())[web_bundle_id.id()] =
      std::move(kr_info_proto);
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::AddToKeyRotations(
    const web_package::SignedWebBundleId& web_bundle_id,
    std::optional<std::vector<uint8_t>> expected_key) && {
  return std::move(AddToKeyRotations(web_bundle_id, std::move(expected_key)));
}

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::AddToSpecialAppPermissions(
    const web_package::SignedWebBundleId& web_bundle_id,
    SpecialAppPermissions special_app_permissions) & {
  IwaSpecialAppPermissions_SpecialAppPermissions special_app_permissions_proto;
  special_app_permissions_proto.mutable_multi_screen_capture()
      ->set_skip_capture_started_notification(
          special_app_permissions.skip_capture_started_notification);

  (*component_.component_data.mutable_special_app_permissions_data()
        ->mutable_special_app_permissions())[web_bundle_id.id()] =
      std::move(special_app_permissions_proto);
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::AddToSpecialAppPermissions(
    const web_package::SignedWebBundleId& web_bundle_id,
    SpecialAppPermissions special_app_permissions) && {
  return std::move(
      AddToSpecialAppPermissions(web_bundle_id, special_app_permissions));
}

KeyDistributionComponentBuilder& KeyDistributionComponentBuilder::WithBlocklist(
    const std::vector<web_package::SignedWebBundleId>& bundle_ids) & {
  // TODO(crbug.com/432446316): Implement the blocklist
  NOTIMPLEMENTED();
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::WithBlocklist(
    const std::vector<web_package::SignedWebBundleId>& bundle_ids) && {
  // TODO(crbug.com/432446316): Implement the blocklist
  NOTIMPLEMENTED();
  return std::move(*this);
}

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::AddToBlocklist(
    const web_package::SignedWebBundleId& bundle_id) & {
  // TODO(crbug.com/432446316): Implement the blocklist
  NOTIMPLEMENTED();
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::AddToBlocklist(
    const web_package::SignedWebBundleId& bundle_id) && {
  // TODO(crbug.com/432446316): Implement the blocklist
  NOTIMPLEMENTED();
  return std::move(*this);
}

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::WithManagedAllowlist(
    const std::vector<web_package::SignedWebBundleId>& bundle_ids) & {
  for (const auto& bundle_id : bundle_ids) {
    (*component_.component_data.mutable_iwa_access_control()
          ->mutable_managed_allowlist())[bundle_id.id()] = {};
  }
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::WithManagedAllowlist(
    const std::vector<web_package::SignedWebBundleId>& bundle_ids) && {
  return std::move(WithManagedAllowlist(bundle_ids));
}

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::AddToManagedAllowlist(
    const web_package::SignedWebBundleId& web_bundle_id) & {
  (*component_.component_data.mutable_iwa_access_control()
        ->mutable_managed_allowlist())[web_bundle_id.id()] = {};
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::AddToManagedAllowlist(
    const web_package::SignedWebBundleId& web_bundle_id) && {
  return std::move(AddToManagedAllowlist(web_bundle_id));
}

KeyDistributionComponent KeyDistributionComponentBuilder::Build() && {
  return std::move(component_);
}

base::expected<void, IwaComponentUpdateError> UpdateKeyDistributionInfo(
    const base::Version& version,
    const base::FilePath& path) {
  ComponentUpdateFuture future;
  auto waiter = ComponentUpdateWaiter(future.GetCallback(), version);
  IwaKeyDistributionInfoProvider::GetInstance().LoadKeyDistributionData(
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
    const std::vector<web_package::SignedWebBundleId>& managed_allowlist) {
  IwaKeyDistribution key_distribution;
  for (const auto& bundle_id : managed_allowlist) {
    auto& managed_allowlist_proto =
        *key_distribution.mutable_iwa_access_control()
             ->mutable_managed_allowlist();
    managed_allowlist_proto[bundle_id.id()] = {};
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
