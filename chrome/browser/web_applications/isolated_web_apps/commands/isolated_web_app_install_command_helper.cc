// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/base32/base32.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/isolated_web_apps/bundle_operations/bundle_operations.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"
#include "crypto/random.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

namespace {

constexpr unsigned kRandomDirNameOctetsLength = 10;

using web_package::SignedWebBundleId;
using web_package::SignedWebBundleIntegrityBlock;

// Returns a base32 representation of 80 random bits. This leads
// to the 16 characters long directory name. 80 bits should be long
// enough not to care about collisions.
std::string GenerateRandomDirName() {
  std::array<uint8_t, kRandomDirNameOctetsLength> random_array;
  crypto::RandBytes(random_array);
  return base::ToLowerASCII(base32::Base32Encode(
      random_array, base32::Base32EncodePolicy::OMIT_PADDING));
}

enum class Operation { kCopy, kMove };

base::expected<IsolatedWebAppStorageLocation, std::string>
CopyOrMoveSwbnToIwaDir(const base::FilePath& swbn_path,
                       const base::FilePath& profile_dir,
                       bool dev_mode,
                       Operation operation) {
  const base::FilePath iwa_dir_path = profile_dir.Append(kIwaDirName);
  if (!base::DirectoryExists(iwa_dir_path)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(iwa_dir_path, &error)) {
      return base::unexpected("Failed to create a root IWA directory: " +
                              base::File::ErrorToString(error));
    }
  }

  std::string dir_name_ascii = GenerateRandomDirName();
  const base::FilePath destination_dir =
      iwa_dir_path.AppendASCII(dir_name_ascii);
  if (base::DirectoryExists(destination_dir)) {
    base::unexpected("The unique destination directory exists: " +
                     destination_dir.AsUTF8Unsafe());
  }

  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(destination_dir, &error)) {
    return base::unexpected(
        "Failed to create a directory " + destination_dir.AsUTF8Unsafe() +
        " for the IWA: " + base::File::ErrorToString(error));
  }

  const base::FilePath destination_swbn_path =
      destination_dir.Append(kMainSwbnFileName);
  switch (operation) {
    case Operation::kCopy:
      if (!base::CopyFile(swbn_path, destination_swbn_path)) {
        base::DeletePathRecursively(destination_dir);
        return base::unexpected(
            "Failed to copy the " + swbn_path.AsUTF8Unsafe() + " file to the " +
            destination_swbn_path.AsUTF8Unsafe() + " IWA directory");
      }
      break;
    case Operation::kMove:
      if (!base::Move(swbn_path, destination_swbn_path)) {
        base::DeletePathRecursively(destination_dir);
        return base::unexpected(
            "Failed to move the " + swbn_path.AsUTF8Unsafe() + " file to the " +
            destination_swbn_path.AsUTF8Unsafe() + " IWA directory");
      }
      break;
  }
  return IwaStorageOwnedBundle{dir_name_ascii, dev_mode};
}

void RemoveParentDirectory(const base::FilePath& path) {
  base::FilePath dir_path = path.DirName();
  if (!base::DeletePathRecursively(dir_path)) {
    LOG(ERROR) << "Could not delete " << dir_path;
  }
}

bool IntegrityBlockDataHasRotatedKey(
    base::optional_ref<const IsolatedWebAppIntegrityBlockData>
        integrity_block_data,
    base::span<const uint8_t> rotated_key) {
  return integrity_block_data &&
         integrity_block_data->HasPublicKey(rotated_key);
}

base::expected<std::optional<SignedWebBundleIntegrityBlock>, std::string>
ExpectedToExpectedOptional(
    base::expected<SignedWebBundleIntegrityBlock, std::string> result) {
  return result.transform([](SignedWebBundleIntegrityBlock value) {
    return std::make_optional(value);
  });
}

}  // namespace

void CleanupLocationIfOwned(const base::FilePath& profile_dir,
                            const IsolatedWebAppStorageLocation& location,
                            base::OnceClosure closure) {
  std::visit(
      absl::Overload{
          [&](const IwaStorageOwnedBundle& location) {
            base::ThreadPool::PostTaskAndReply(
                FROM_HERE,
                {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
                 base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                base::BindOnce(RemoveParentDirectory,
                               location.GetPath(profile_dir)),
                std::move(closure));
          },
          [&](const IwaStorageUnownedBundle& location) {
            std::move(closure).Run();
          },
          [&](const IwaStorageProxy& location) { std::move(closure).Run(); }},
      location.variant());
}

void UpdateBundlePathAndCreateStorageLocation(
    const base::FilePath& profile_dir,
    const IwaSourceWithModeAndFileOp& source,
    base::OnceCallback<void(
        base::expected<IsolatedWebAppStorageLocation, std::string>)> callback) {
  auto copy_or_move = [&callback, &profile_dir](
                          const base::FilePath& bundle_path, bool dev_mode,
                          Operation operation) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(CopyOrMoveSwbnToIwaDir, bundle_path, profile_dir,
                       dev_mode, operation),
        std::move(callback));
  };

  std::visit(absl::Overload{
                 [&](const IwaSourceBundleWithModeAndFileOp& bundle) {
                   switch (bundle.mode_and_file_op()) {
                     case IwaSourceBundleModeAndFileOp::kDevModeCopy:
                       copy_or_move(bundle.path(), /*dev_mode=*/true,
                                    Operation::kCopy);
                       break;
                     case IwaSourceBundleModeAndFileOp::kDevModeMove:
                       copy_or_move(bundle.path(), /*dev_mode=*/true,
                                    Operation::kMove);
                       break;
                     case IwaSourceBundleModeAndFileOp::kProdModeCopy:
                       copy_or_move(bundle.path(), /*dev_mode=*/false,
                                    Operation::kCopy);
                       break;
                     case IwaSourceBundleModeAndFileOp::kProdModeMove:
                       copy_or_move(bundle.path(), /*dev_mode=*/false,
                                    Operation::kMove);
                       break;
                   }
                 },
                 [&](const IwaSourceProxy& proxy) {
                   std::move(callback).Run(IwaStorageProxy(proxy.proxy_url()));
                 },
             },
             source.variant());
}

std::optional<KeyRotationData> GetKeyRotationData(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IsolationData& isolation_data) {
  const auto* kr_info =
      ChromeIwaRuntimeDataProvider::GetInstance().GetKeyRotationInfo(
          web_bundle_id.id());
  if (!kr_info) {
    return std::nullopt;
  }
  const auto& rotated_key = kr_info->public_key;

  // Checks whether `rotated_key` is contained in
  // `isolation_data.integrity_block_data`.
  const bool current_installation_has_rk = IntegrityBlockDataHasRotatedKey(
      isolation_data.integrity_block_data(), rotated_key);
  const auto& pending_update = isolation_data.pending_update_info();

  // Checks whether `rotated_key` is contained in
  // `isolation_data.pending_update_info.integrity_block_data`.
  const bool pending_update_has_rk =
      pending_update && IntegrityBlockDataHasRotatedKey(
                            pending_update->integrity_block_data, rotated_key);

  return {{.rotated_key = rotated_key,
           .current_installation_has_rk = current_installation_has_rk,
           .pending_update_has_rk = pending_update_has_rk}};
}

VersionChangeValidationResult ValidateVersionChangeFeasibility(
    const IwaVersion& expected_version,
    const IwaVersion& installed_version,
    bool allow_downgrades,
    bool same_version_update_allowed_by_key_rotation) {
  if (expected_version < installed_version && !allow_downgrades) {
    return VersionChangeValidationResult::kDowngradeDisallowed;
  }
  if (expected_version == installed_version &&
      !same_version_update_allowed_by_key_rotation) {
    return VersionChangeValidationResult::kSameVersionUpdateDisallowed;
  }
  return VersionChangeValidationResult::kAllowed;
}

IsolatedWebAppInstallCommandHelper::IsolatedWebAppInstallCommandHelper(
    IsolatedWebAppUrlInfo url_info)
    : url_info_(std::move(url_info)) {}

IsolatedWebAppInstallCommandHelper::~IsolatedWebAppInstallCommandHelper() =
    default;

void IsolatedWebAppInstallCommandHelper::CheckTrustAndSignatures(
    const IwaSourceWithMode& location,
    const IwaOperation& operation,
    Profile* profile,
    base::OnceCallback<
        void(base::expected<
             std::optional<web_package::SignedWebBundleIntegrityBlock>,
             std::string>)> callback) {
  RETURN_IF_ERROR(
      IsolatedWebAppTrustChecker::IsOperationAllowed(
          *profile, url_info_.web_bundle_id(), location.dev_mode(), operation),
      [&](const std::string& error) {
        std::move(callback).Run(base::unexpected(error));
      });

  std::visit(
      absl::Overload{
          [&](const IwaSourceBundleWithMode& location) {
            CHECK(!url_info_.web_bundle_id().is_for_proxy_mode());
            ValidateSignedWebBundleSignatures(
                profile, location.path(), url_info_.web_bundle_id(),
                base::BindOnce(&ExpectedToExpectedOptional)
                    .Then(std::move(callback)));
          },
          [&](const IwaSourceProxy& location) {
            CHECK(url_info_.web_bundle_id().is_for_proxy_mode());
            // Dev mode proxy mode does not use Web Bundles, hence there is no
            // bundle to validate / trust and no signatures to check.
            std::move(callback).Run(base::ok(std::nullopt));
          }},
      location.variant());
}

void IsolatedWebAppInstallCommandHelper::CheckTrustAndSignatures(
    const IwaSourceWithMode& location,
    const IwaOperation& operation,
    Profile* profile,
    base::OnceCallback<void(base::expected<void, std::string>)> callback) {
  CheckTrustAndSignatures(
      location, operation, profile,
      base::BindOnce(
          [](base::expected<
              std::optional<web_package::SignedWebBundleIntegrityBlock>,
              std::string> result) {
            return result.transform([](const auto&) -> void {});
          })
          .Then(std::move(callback)));
}

void IsolatedWebAppInstallCommandHelper::CreateStoragePartitionIfNotPresent(
    Profile& profile) {
  profile.GetStoragePartition(url_info_.storage_partition_config(&profile),
                              /*can_create=*/true);
}


}  // namespace web_app
