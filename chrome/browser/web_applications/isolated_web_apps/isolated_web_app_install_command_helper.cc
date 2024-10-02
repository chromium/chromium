// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/overloaded.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_version.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/base32/base32.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "crypto/random.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

namespace {

constexpr static char kGeneratedInstallPagePath[] =
    "/.well-known/_generated_install_page.html";

constexpr unsigned kRandomDirNameOctetsLength = 10;

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

}  // namespace

void CleanupLocationIfOwned(const base::FilePath& profile_dir,
                            const IsolatedWebAppStorageLocation& location,
                            base::OnceClosure closure) {
  absl::visit(
      base::Overloaded{
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

  absl::visit(
      base::Overloaded{
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

base::flat_map<web_package::SignedWebBundleId,
               std::reference_wrapper<const WebApp>>
GetInstalledIwas(const WebAppRegistrar& registrar) {
  base::flat_map<web_package::SignedWebBundleId,
                 std::reference_wrapper<const WebApp>>
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
    const web_package::SignedWebBundleId& web_bundle_id,
    base::optional_ref<base::Value::Dict> debug_log) {
  auto log_rotated_key = [&](const std::string& value) {
    if (debug_log) {
      debug_log->Set("rotated_key", value);
    }
  };

  const auto* kr_info =
      IwaKeyDistributionInfoProvider::GetInstance()->GetKeyRotationInfo(
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

KeyRotationData GetKeyRotationData(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IsolationData& isolation_data) {
  const auto* kr_info =
      IwaKeyDistributionInfoProvider::GetInstance()->GetKeyRotationInfo(
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

  return {.rotated_key = raw_ref(rotated_key),
          .current_installation_has_rk = current_installation_has_rk,
          .pending_update_has_rk = pending_update_has_rk};
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

// static
std::unique_ptr<IsolatedWebAppResponseReaderFactory>
IsolatedWebAppInstallCommandHelper::CreateDefaultResponseReaderFactory(
    Profile& profile) {
  auto validator = std::make_unique<IsolatedWebAppValidator>();

  return std::make_unique<IsolatedWebAppResponseReaderFactory>(
      profile, std::move(validator));
}

IsolatedWebAppInstallCommandHelper::IsolatedWebAppInstallCommandHelper(
    IsolatedWebAppUrlInfo url_info,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    std::unique_ptr<IsolatedWebAppResponseReaderFactory>
        response_reader_factory)
    : url_info_(std::move(url_info)),
      data_retriever_(std::move(data_retriever)),
      response_reader_factory_(std::move(response_reader_factory)) {}

IsolatedWebAppInstallCommandHelper::~IsolatedWebAppInstallCommandHelper() =
    default;

void IsolatedWebAppInstallCommandHelper::CheckTrustAndSignatures(
    const IwaSourceWithMode& location,
    Profile* profile,
    base::OnceCallback<
        void(base::expected<
             std::optional<web_package::SignedWebBundleIntegrityBlock>,
             std::string>)> callback) {
  absl::visit(
      base::Overloaded{
          [&](const IwaSourceBundleWithMode& location) {
            CHECK(!url_info_.web_bundle_id().is_for_proxy_mode());
            if (location.dev_mode() && !IsIwaDevModeEnabled(profile)) {
              std::move(callback).Run(
                  base::unexpected(std::string(kIwaDevModeNotEnabledMessage)));
              return;
            }
            CheckTrustAndSignaturesOfBundle(
                location.path(), location.dev_mode(), std::move(callback));
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
          [](base::expected<
              std::optional<web_package::SignedWebBundleIntegrityBlock>,
              std::string> result) {
            return result.transform([](const auto&) -> void {});
          })
          .Then(std::move(callback)));
}

void IsolatedWebAppInstallCommandHelper::CheckTrustAndSignaturesOfBundle(
    const base::FilePath& path,
    bool dev_mode,
    base::OnceCallback<
        void(base::expected<
             std::optional<web_package::SignedWebBundleIntegrityBlock>,
             std::string>)> callback) {
  // To check whether the bundle is valid and trusted, we attempt to create a
  // `IsolatedWebAppResponseReader`. If a response reader is created
  // successfully, then this means that the Signed Web Bundle...
  // - ...is well formatted and uses a supported Web Bundle version.
  // - ...contains a valid integrity block with a trusted public key.
  // - ...has signatures that were verified successfully (as long as
  //   `skip_signature_verification` below is set to `false`).
  // - ...contains valid metadata / no invalid URLs.
  IsolatedWebAppResponseReaderFactory::Flags flags;
  if (dev_mode) {
    flags.Put(IsolatedWebAppResponseReaderFactory::Flag::kDevModeBundle);
  }
  response_reader_factory_->CreateResponseReader(
      path, url_info_.web_bundle_id(), flags,
      base::BindOnce(&IsolatedWebAppInstallCommandHelper::
                         OnTrustAndSignaturesOfBundleChecked,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IsolatedWebAppInstallCommandHelper::OnTrustAndSignaturesOfBundleChecked(
    base::OnceCallback<
        void(base::expected<
             std::optional<web_package::SignedWebBundleIntegrityBlock>,
             std::string>)> callback,
    base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                   UnusableSwbnFileError> result) {
  ASSIGN_OR_RETURN(
      auto reader, std::move(result), [&](const UnusableSwbnFileError& error) {
        std::move(callback).Run(base::unexpected(
            IsolatedWebAppResponseReaderFactory::ErrorToString(error)));
      });

  auto* reader_ptr = reader.get();
  base::OnceClosure reader_keep_alive =
      base::DoNothingWithBoundArgs(std::move(reader));
  reader_ptr->Close(std::move(reader_keep_alive)
                        .Then(base::BindOnce(std::move(callback),
                                             reader_ptr->GetIntegrityBlock())));
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
        base::StrCat({"Error during URL loading: ",
                      ConvertUrlLoaderResultToString(result)})));
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

base::expected<WebAppInstallInfo, std::string>
IsolatedWebAppInstallCommandHelper::ValidateManifestAndCreateInstallInfo(
    const std::optional<base::Version>& expected_version,
    const blink::mojom::Manifest& manifest) {
  const GURL& manifest_url = manifest.manifest_url;

  ASSIGN_OR_RETURN(
      auto info,
      WebAppInstallInfo::Create(manifest_url, manifest.id, manifest.start_url));

  UpdateWebAppInfoFromManifest(manifest, &info);

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
      auto version, ParseIwaVersion(version_string),
      [&](auto error) -> base::expected<WebAppInstallInfo, std::string> {
        return base::unexpected(base::StrCat(
            {"Failed to parse `version` from the manifest: It must be in the "
             "form `x.y.z`, where `x`, `y`, and `z` are numbers without "
             "leading zeros. Detailed error: ",
             IwaVersionParseErrorToString(error), " Got: ", version_string}));
      });

  if (expected_version.has_value() && *expected_version != version) {
    return base::unexpected(
        "Expected version (" + expected_version->GetString() +
        ") does not match the version provided in the manifest (" +
        version.GetString() + ")");
  }
  info.isolated_web_app_version = std::move(version);

  std::string encoded_id = manifest.id.path();

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

  if (info.title.empty()) {
    return base::unexpected(base::StrCat(
        {"App manifest must have either 'name' or 'short_name'. manifest_url: ",
         manifest_url.possibly_invalid_spec()}));
  }

  info.user_display_mode = mojom::UserDisplayMode::kStandalone;

  return info;
}

void IsolatedWebAppInstallCommandHelper::RetrieveIconsAndPopulateInstallInfo(
    WebAppInstallInfo install_info,
    content::WebContents& web_contents,
    base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>
        callback) {
  IconUrlSizeSet icon_urls = GetValidIconUrlsToDownload(install_info);
  data_retriever_->GetIcons(
      &web_contents, std::move(icon_urls),
      /*skip_page_favicons=*/true,
      // IWAs should not refer to resources which don't exist.
      /*fail_all_if_any_fail=*/true,
      base::BindOnce(&IsolatedWebAppInstallCommandHelper::OnRetrieveIcons,
                     weak_factory_.GetWeakPtr(), std::move(install_info),
                     std::move(callback)));
}

void IsolatedWebAppInstallCommandHelper::OnRetrieveIcons(
    WebAppInstallInfo install_info,
    base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>
        callback,
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults unused_icons_http_results) {
  if (result != IconsDownloadedResult::kCompleted) {
    std::move(callback).Run(base::unexpected(
        base::StrCat({"Error during icon downloading: ",
                      IconsDownloadedResultToString(result)})));
    return;
  }

  PopulateProductIcons(&install_info, &icons_map);
  PopulateOtherIcons(&install_info, icons_map);

  std::move(callback).Run(std::move(install_info));
}

}  // namespace web_app
