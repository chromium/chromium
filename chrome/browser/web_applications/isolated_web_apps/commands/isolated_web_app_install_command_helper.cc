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
#include "chrome/browser/web_applications/isolated_web_apps/install/pending_install_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
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
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/isolated_web_apps/bundle_operations/bundle_operations.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "crypto/random.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

namespace {

constexpr static char kGeneratedInstallPagePath[] =
    "/.well-known/_generated_install_page.html";

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

bool IsUrlLoadingResultSuccess(webapps::WebAppUrlLoaderResult result) {
  return result == webapps::WebAppUrlLoaderResult::kUrlLoaded;
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

base::expected<std::reference_wrapper<const WebApp>, std::string>
GetIsolatedWebAppById(const WebAppRegistrar& registrar,
                      const webapps::AppId& iwa_id) {
  auto* iwa = registrar.GetAppById(iwa_id);
  if (!iwa) {
    return base::unexpected("App is no longer installed.");
  }
  if (!iwa->isolation_data()) {
    return base::unexpected("Installed app is not an Isolated Web App.");
  }
  return *iwa;
}

base::flat_map<SignedWebBundleId, std::reference_wrapper<const WebApp>>
GetInstalledIwas(const WebAppRegistrar& registrar) {
  base::flat_map<SignedWebBundleId, std::reference_wrapper<const WebApp>>
      installed_iwas;
  for (const WebApp& web_app : registrar.GetApps()) {
    if (!web_app.isolation_data().has_value()) {
      continue;
    }
    auto url_info = IsolatedWebAppUrlInfo::Create(web_app.start_url());
    if (!url_info.has_value()) {
      LOG(ERROR) << "Unable to calculate IsolatedWebAppUrlInfo from "
                 << web_app.start_url();
      continue;
    }

    installed_iwas.try_emplace(url_info->web_bundle_id(), std::ref(web_app));
  }

  return installed_iwas;
}

KeyRotationLookupResult LookupRotatedKey(
    const SignedWebBundleId& web_bundle_id,
    base::optional_ref<base::Value::Dict> debug_log) {
  auto log_rotated_key = [&](const std::string& value) {
    if (debug_log) {
      debug_log->Set("rotated_key", value);
    }
  };

  const auto* kr_info =
      IwaKeyDistributionInfoProvider::GetInstance().GetKeyRotationInfo(
          web_bundle_id.id());
  if (!kr_info) {
    return KeyRotationLookupResult::kNoKeyRotation;
  }

  if (!kr_info->public_key) {
    log_rotated_key("<disabled>");
    return KeyRotationLookupResult::kKeyBlocked;
  }
  log_rotated_key(base::Base64Encode(*kr_info->public_key));
  return KeyRotationLookupResult::kKeyFound;
}

KeyRotationData GetKeyRotationData(const SignedWebBundleId& web_bundle_id,
                                   const IsolationData& isolation_data) {
  const auto* kr_info =
      IwaKeyDistributionInfoProvider::GetInstance().GetKeyRotationInfo(
          web_bundle_id.id());
  CHECK(kr_info && kr_info->public_key)
      << "`GetKeyRotationData()` must only be called if `LookupRotatedKey()` "
         "has previously reported `KeyRotationLookupResult::kKeyFound`.";

  const auto& rotated_key = *kr_info->public_key;

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

  return {.rotated_key = rotated_key,
          .current_installation_has_rk = current_installation_has_rk,
          .pending_update_has_rk = pending_update_has_rk};
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

// static
std::unique_ptr<content::WebContents>
IsolatedWebAppInstallCommandHelper::CreateIsolatedWebAppWebContents(
    Profile& profile) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          /*context=*/&profile));

  webapps::InstallableManager::CreateForWebContents(web_contents.get());

  return web_contents;
}

IsolatedWebAppInstallCommandHelper::IsolatedWebAppInstallCommandHelper(
    IsolatedWebAppUrlInfo url_info,
    std::unique_ptr<WebAppDataRetriever> data_retriever)
    : url_info_(std::move(url_info)),
      data_retriever_(std::move(data_retriever)) {}

IsolatedWebAppInstallCommandHelper::~IsolatedWebAppInstallCommandHelper() =
    default;

void IsolatedWebAppInstallCommandHelper::CheckTrustAndSignatures(
    const IwaSourceWithMode& location,
    Profile* profile,
    base::OnceCallback<
        void(base::expected<std::optional<SignedWebBundleIntegrityBlock>,
                            std::string>)> callback) {
  std::visit(
      absl::Overload{
          [&](const IwaSourceBundleWithMode& location) {
            CHECK(!url_info_.web_bundle_id().is_for_proxy_mode());
            if (location.dev_mode() && !IsIwaDevModeEnabled(profile)) {
              std::move(callback).Run(
                  base::unexpected(std::string(kIwaDevModeNotEnabledMessage)));
              return;
            }
            ValidateSignedWebBundleTrustAndSignatures(
                profile, location.path(), url_info_.web_bundle_id(),
                location.dev_mode(),
                base::BindOnce(&ExpectedToExpectedOptional)
                    .Then(std::move(callback)));
          },
          [&](const IwaSourceProxy& location) {
            CHECK(url_info_.web_bundle_id().is_for_proxy_mode());
            if (!IsIwaDevModeEnabled(profile)) {
              std::move(callback).Run(
                  base::unexpected(std::string(kIwaDevModeNotEnabledMessage)));
              return;
            }
            // Dev mode proxy mode does not use Web Bundles, hence there is no
            // bundle to validate / trust and no signatures to check.
            std::move(callback).Run(base::ok(std::nullopt));
          }},
      location.variant());
}

void IsolatedWebAppInstallCommandHelper::CheckTrustAndSignatures(
    const IwaSourceWithMode& location,
    Profile* profile,
    base::OnceCallback<void(base::expected<void, std::string>)> callback) {
  CheckTrustAndSignatures(
      location, profile,
      base::BindOnce(
          [](base::expected<std::optional<SignedWebBundleIntegrityBlock>,
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

void IsolatedWebAppInstallCommandHelper::LoadInstallUrl(
    const IwaSourceWithMode& source,
    content::WebContents& web_contents,
    webapps::WebAppUrlLoader& url_loader,
    base::OnceCallback<void(base::expected<void, std::string>)> callback) {
  // |web_app::IsolatedWebAppURLLoaderFactory| uses the isolation data in
  // order to determine the current state of content serving (installation
  // process vs application data serving) and source of data (proxy, web
  // bundle, etc...).
  IsolatedWebAppPendingInstallInfo::FromWebContents(web_contents)
      .set_source(source);

  GURL install_page_url =
      url_info_.origin().GetURL().Resolve(kGeneratedInstallPagePath);

  content::NavigationController::LoadURLParams load_params(install_page_url);
  load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  // It is important to bypass a potentially registered Service Worker for two
  // reasons:
  // 1. `IsolatedWebAppPendingInstallInfo` is attached to a `WebContents` and
  //    retrieved inside `IsolatedWebAppURLLoaderFactory` based on a frame tree
  //    node id. There is no frame tree node id for requests that are
  //    intercepted by Service Workers.
  // 2. We want to make sure that a Service Worker cannot tamper with the
  //    install page.
  load_params.reload_type = content::ReloadType::BYPASSING_CACHE;

  url_loader.LoadUrl(
      std::move(load_params), &web_contents,
      webapps::WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&IsolatedWebAppInstallCommandHelper::OnLoadInstallUrl,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IsolatedWebAppInstallCommandHelper::OnLoadInstallUrl(
    base::OnceCallback<void(base::expected<void, std::string>)> callback,
    webapps::WebAppUrlLoaderResult result) {
  if (!IsUrlLoadingResultSuccess(result)) {
    std::move(callback).Run(base::unexpected(
        base::StrCat({"Error during URL loading: ", base::ToString(result)})));
    return;
  }

  std::move(callback).Run(base::ok());
}

void IsolatedWebAppInstallCommandHelper::CheckInstallabilityAndRetrieveManifest(
    content::WebContents& web_contents,
    base::OnceCallback<void(
        base::expected<blink::mojom::ManifestPtr, std::string>)> callback) {
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      &web_contents,
      base::BindOnce(&IsolatedWebAppInstallCommandHelper::
                         OnCheckInstallabilityAndRetrieveManifest,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IsolatedWebAppInstallCommandHelper::
    OnCheckInstallabilityAndRetrieveManifest(
        base::OnceCallback<void(
            base::expected<blink::mojom::ManifestPtr, std::string>)> callback,
        blink::mojom::ManifestPtr opt_manifest,
        bool valid_manifest_for_web_app,
        webapps::InstallableStatusCode error_code) {
  if (error_code != webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
    std::move(callback).Run(base::unexpected(base::StrCat(
        {"App is not installable: ", webapps::GetErrorMessage(error_code),
         "."})));
    return;
  }

  // See |WebAppDataRetriever::CheckInstallabilityCallback| documentation for
  // details.
  DCHECK(valid_manifest_for_web_app)
      << "must be true when no error is detected.";

  if (!opt_manifest) {
    std::move(callback).Run(base::unexpected("Manifest is null."));
    return;
  }

  if (opt_manifest->manifest_url.is_empty()) {
    std::move(callback).Run(
        base::unexpected("Manifest is the default manifest."));
    return;
  }

  // See |WebAppDataRetriever::CheckInstallabilityCallback| documentation for
  // details.
  DCHECK(!blink::IsEmptyManifest(opt_manifest))
      << "must not be empty when manifest is present.";

  std::move(callback).Run(std::move(opt_manifest));
}

base::expected<IwaVersion, std::string>
IsolatedWebAppInstallCommandHelper::ValidateManifestAndGetVersion(
    const std::optional<IwaVersion>& expected_version,
    const blink::mojom::Manifest& manifest) {
  const GURL& manifest_url = manifest.manifest_url;

  if (!manifest.version.has_value()) {
    return base::unexpected(
        "Manifest `version` is not present. manifest_url: " +
        manifest_url.possibly_invalid_spec());
  }
  std::string version_string;
  if (!base::UTF16ToUTF8(manifest.version->data(), manifest.version->length(),
                         &version_string)) {
    return base::unexpected(
        "Failed to convert manifest `version` from UTF16 to UTF8.");
  }

  ASSIGN_OR_RETURN(
      web_app::IwaVersion iwa_version,
      web_app::IwaVersion::Create(version_string),
      [version_string](web_app::IwaVersion::IwaVersionParseError error) {
        return base::StrCat(
            {"Failed to parse `version` from the manifest: It must be in the "
             "form `x.y.z`, where `x`, `y`, and `z` are numbers without "
             "leading zeros. Detailed error: ",
             web_app::IwaVersion::GetErrorString(error),
             ". Got: ", version_string});
      });

  if (expected_version.has_value() && *expected_version != iwa_version) {
    return base::unexpected(
        "Expected version (" + expected_version->GetString() +
        ") does not match the version provided in the manifest (" +
        iwa_version.GetString() + ")");
  }

  std::string encoded_id = manifest.id.GetPath();

  if (encoded_id != "/") {
    // Recommend to use "/" for manifest id and not empty manifest id because
    // the manifest parser does additional work on resolving manifest id taking
    // `start_url` into account. (See https://w3c.github.io/manifest/#id-member
    // on how the manifest parser resolves the `id` field).
    //
    // It is required for Isolated Web Apps to have app id based on origin of
    // the application and do not include other information in order to be able
    // to identify Isolated Web Apps by origin because there is always only 1
    // app per origin.
    return base::unexpected(
        R"(Manifest `id` must be "/". Resolved manifest id: )" + encoded_id);
  }

  url::Origin origin = url_info_.origin();
  if (manifest.scope != origin.GetURL()) {
    return base::unexpected(
        base::StrCat({"Scope should resolve to the origin. scope: ",
                      manifest.scope.possibly_invalid_spec(),
                      ", origin: ", origin.Serialize()}));
  }
  return iwa_version;
}

void IsolatedWebAppInstallCommandHelper::
    RetrieveInstallInfoWithIconsFromManifest(
        const blink::mojom::Manifest& manifest,
        content::WebContents& web_contents,
        IwaVersion parsed_version,
        base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>
            callback) {
  // install source needs to be peaced together properly.
  WebAppInstallInfoConstructOptions construct_options;
  construct_options.fail_all_if_any_fail = true;
  manifest_to_install_info_job_ =
      ManifestToWebAppInstallInfoJob::CreateAndStart(
          manifest, *data_retriever_.get(), /*background_installation=*/false,
          webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER,
          web_contents.GetWeakPtr(), [](IconUrlSizeSet& icon_url_size_set) {},
          manifest_to_info_debug_data_,
          base::BindOnce(&IsolatedWebAppInstallCommandHelper::
                             OnGettingInstallInfoFromManifest,
                         weak_factory_.GetWeakPtr(), std::move(parsed_version),
                         std::move(callback)),
          construct_options);
}

void IsolatedWebAppInstallCommandHelper::OnGettingInstallInfoFromManifest(
    IwaVersion parsed_version,
    base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>
        callback,
    std::unique_ptr<WebAppInstallInfo> install_info) {
  install_info->set_isolated_web_app_version(std::move(parsed_version));

  if (install_info->title.empty()) {
    std::move(callback).Run(base::unexpected(base::StrCat(
        {"App manifest must have either 'name' or 'short_name'. manifest_url: ",
         install_info->manifest_url.possibly_invalid_spec()})));
    return;
  }

  // The existence of generated icons mean that icon downloads had failed.
  if (install_info->is_generated_icon) {
    std::move(callback).Run(base::unexpected(base::StrCat(
        {"Error during icon downloading, stopping installation."})));
    return;
  }

  install_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

  std::move(callback).Run(std::move(*install_info.get()));
}

}  // namespace web_app
