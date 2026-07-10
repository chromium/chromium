// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
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
#include "components/webapps/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"

namespace web_app::test {

namespace {

using ComponentMetadataOrError =
    base::expected<IwaComponentMetadata, IwaComponentUpdateError>;

using ComponentUpdateFuture = base::test::TestFuture<ComponentMetadataOrError>;

}  // namespace

base::expected<void, IwaComponentUpdateError>
KeyDistributionComponent::UploadFromComponentFolder() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return UpdateKeyDistributionInfo(metadata.version, component_data);
}

void KeyDistributionComponent::InjectComponentDataDirectly() {
  IwaKeyDistributionInfoProvider::GetInstanceForTesting()
      .SetComponentDataForTesting(metadata.version, metadata.is_preloaded,
                                  component_data);
}

KeyDistributionComponentBuilder::KeyDistributionComponentBuilder(
    const base::Version& component_version,
    bool is_preloaded)
    : component_(
          /*metadata=*/IwaComponentMetadata{component_version, is_preloaded},
          /*component_data=*/IwaKeyDistribution{}) {}

KeyDistributionComponentBuilder::~KeyDistributionComponentBuilder() = default;

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::AddToKeyRotations(
    const web_package::SignedWebBundleId& web_bundle_id,
    base::span<const uint8_t> expected_key) & {
  IwaKeyRotations::KeyRotationInfo kr_info_proto;
  kr_info_proto.set_expected_key(base::Base64Encode(expected_key));
  (*component_.component_data.mutable_key_rotation_data()
        ->mutable_key_rotations())[web_bundle_id.id()] =
      std::move(kr_info_proto);
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::AddToKeyRotations(
    const web_package::SignedWebBundleId& web_bundle_id,
    base::span<const uint8_t> expected_key) && {
  return std::move(AddToKeyRotations(web_bundle_id, std::move(expected_key)));
}

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::AddToSpecialAppPermissions(
    const web_package::SignedWebBundleId& web_bundle_id,
    SpecialAppPermissions special_app_permissions) & {
  IwaSpecialAppPermissions_SpecialAppPermissions special_app_permissions_proto;
  if (special_app_permissions.skip_capture_started_notification) {
    special_app_permissions_proto.mutable_multi_screen_capture()
        ->set_skip_capture_started_notification(true);
  }
  if (special_app_permissions.allow_set_shape) {
    special_app_permissions_proto.mutable_chrome_os_permissions()
        ->set_allow_set_shape(true);
  }
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
  for (const auto& bundle_id : bundle_ids) {
    AddToBlocklist(bundle_id);
  }
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::WithBlocklist(
    const std::vector<web_package::SignedWebBundleId>& bundle_ids) && {
  return std::move(WithBlocklist(bundle_ids));
}

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::AddToBlocklist(
    const web_package::SignedWebBundleId& web_bundle_id) & {
  component_.component_data.mutable_iwa_access_control()
      ->mutable_blocklist()
      ->emplace(web_bundle_id.id(), IwaAccessControl_BlocklistItemData{});
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::AddToBlocklist(
    const web_package::SignedWebBundleId& web_bundle_id) && {
  return std::move(AddToBlocklist(web_bundle_id));
}

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::WithManagedAllowlist(
    const std::vector<web_package::SignedWebBundleId>& bundle_ids) & {
  for (const auto& bundle_id : bundle_ids) {
    AddToManagedAllowlist(bundle_id);
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
  component_.component_data.mutable_iwa_access_control()
      ->mutable_managed_allowlist()
      ->emplace(web_bundle_id.id(),
                IwaAccessControl_ManagedAllowlistItemData{});
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
  auto waiter = SetOnComponentUpdatedForTesting(future.GetRepeatingCallback());
  IwaKeyDistributionInfoProvider::GetInstanceForTesting()
      .LoadKeyDistributionData(version, path, /*is_preloaded=*/false);
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

base::CallbackListSubscription SetOnComponentUpdatedForTesting(
    base::RepeatingCallback<void(ComponentMetadataOrError)> callback) {
  return IwaKeyDistributionInfoProvider::GetInstanceForTesting()
      .OnComponentUpdatedForTesting(
          base::BindRepeating([](base::expected<void, IwaComponentUpdateError>
                                     result) {
            return result.transform([]() -> IwaComponentMetadata {
              auto& instance =
                  IwaKeyDistributionInfoProvider::GetInstanceForTesting();
              return {.version = *instance.GetVersion(),
                      .is_preloaded = *instance.IsPreloadedForTesting()};
            });
          }).Then(callback));
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
