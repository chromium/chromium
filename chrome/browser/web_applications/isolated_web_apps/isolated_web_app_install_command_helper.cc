// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"

#include <memory>
#include <string>
#include <vector>

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
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_dev_mode.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_version.h"
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "components/base32/base32.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "crypto/random.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  std::array<char, kRandomDirNameOctetsLength> random_array;
  crypto::RandBytes(random_array.data(), random_array.size());
  base::StringPiece random_string_piece(random_array.data(),
                                        random_array.size());
  std::string dir_name_upper_case = Base32Encode(
      random_string_piece, base32::Base32EncodePolicy::OMIT_PADDING);
  return base::ToLowerASCII(dir_name_upper_case);
}

base::expected<base::FilePath, std::string> CopySwbnToIwaDir(
    const base::FilePath& swbn_path,
    const base::FilePath& profile_dir) {
  const base::FilePath iwa_dir_path = profile_dir.Append(kIwaDirName);
  if (!base::DirectoryExists(iwa_dir_path)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(iwa_dir_path, &error)) {
      return base::unexpected("Failed to create a root IWA directory: " +
                              base::File::ErrorToString(error));
    }
  }

  const base::FilePath destination_dir =
      iwa_dir_path.AppendASCII(GenerateRandomDirName());
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
  if (!base::CopyFile(swbn_path, destination_swbn_path)) {
    base::DeletePathRecursively(destination_dir);
    return base::unexpected(
        "Failed to copy the " + swbn_path.AsUTF8Unsafe() + " file to the " +
        destination_swbn_path.AsUTF8Unsafe() + " IWA directory");
  }

  return destination_swbn_path;
}

void RemoveParentDirectory(const base::FilePath& path) {
  base::FilePath dir_path = path.DirName();
  if (!base::DeletePathRecursively(dir_path)) {
    LOG(ERROR) << "Could not delete " << dir_path;
  }
}

bool IsSwbnPathOwnedByChrome(const base::FilePath& profile_dir,
                             const base::FilePath& swbn_path) {
  const base::FilePath iwa_directory = profile_dir.Append(kIwaDirName);
  return iwa_directory.IsParent(swbn_path);
}

bool IsUrlLoadingResultSuccess(WebAppUrlLoader::Result result) {
  return result == WebAppUrlLoader::Result::kUrlLoaded;
}

base::expected<IsolatedWebAppLocation, std::string>
CreateUpdatedInstalledBundleLocation(
    base::expected<base::FilePath, std::string> new_path) {
  return new_path.transform(
      [](const base::FilePath& path) -> IsolatedWebAppLocation {
        return InstalledBundle{.path = path};
      });
}

}  // namespace

void CleanupLocationIfOwned(const base::FilePath& profile_dir,
                            const IsolatedWebAppLocation& location,
                            base::OnceClosure closure) {
  absl::visit(base::Overloaded{
                  [&](const InstalledBundle& location) {
                    if (IsSwbnPathOwnedByChrome(profile_dir, location.path)) {
                      base::ThreadPool::PostTaskAndReply(
                          FROM_HERE,
                          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
                           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                          base::BindOnce(RemoveParentDirectory, location.path),
                          std::move(closure));
                    } else {
                      std::move(closure).Run();
                    }
                  },
                  [&](const DevModeProxy& location) {
                    // Nothing to delete for IWA proxy mode.
                    std::move(closure).Run();
                  },
                  [&](const DevModeBundle& location) {
                    // So far we don't relocate dev mode web bundle to the IWA
                    // directory. So there is nothing to delete.
                    std::move(closure).Run();
                  }},
              location);
}

void CopyLocationToProfileDirectory(
    const base::FilePath& profile_dir,
    const IsolatedWebAppLocation& location,
    base::OnceCallback<
        void(base::expected<IsolatedWebAppLocation, std::string>)> callback) {
  absl::visit(
      base::Overloaded{
          [&](const InstalledBundle& location) {
            base::ThreadPool::PostTaskAndReplyWithResult(
                FROM_HERE,
                {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
                 base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                base::BindOnce(CopySwbnToIwaDir, location.path, profile_dir),
                base::BindOnce(&CreateUpdatedInstalledBundleLocation)
                    .Then(std::move(callback)));
          },
          [&](const DevModeBundle&) {
            // As soon as uninstallation/update is implemented, here we should
            // copy the .swbn file to the profile dir.
            std::move(callback).Run(location);
          },
          [&](const DevModeProxy&) {
            // Nothing to relocate for IWA proxy mode.
            std::move(callback).Run(location);
          }},
      location);
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
    const PrefService& prefs) {
  auto trust_checker = std::make_unique<IsolatedWebAppTrustChecker>(prefs);
  auto validator =
      std::make_unique<IsolatedWebAppValidator>(std::move(trust_checker));

  return std::make_unique<IsolatedWebAppResponseReaderFactory>(
      std::move(validator));
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
    const IsolatedWebAppLocation& location,
    Profile* profile,
    base::OnceCallback<void(base::expected<void, std::string>)> callback) {
  absl::visit(
      base::Overloaded{
          [&](const InstalledBundle& location) {
            CHECK_EQ(url_info_.web_bundle_id().type(),
                     web_package::SignedWebBundleId::Type::kEd25519PublicKey);
            CheckTrustAndSignaturesOfBundle(location.path, std::move(callback));
          },
          [&](const DevModeBundle& location) {
            CHECK_EQ(url_info_.web_bundle_id().type(),
                     web_package::SignedWebBundleId::Type::kEd25519PublicKey);
            if (!IsIwaDevModeEnabled(profile)) {
              std::move(callback).Run(
                  base::unexpected(std::string(kIwaDevModeNotEnabledMessage)));
              return;
            }
            CheckTrustAndSignaturesOfBundle(location.path, std::move(callback));
          },
          [&](const DevModeProxy& location) {
            CHECK_EQ(url_info_.web_bundle_id().type(),
                     web_package::SignedWebBundleId::Type::kDevelopment);
            if (!IsIwaDevModeEnabled(profile)) {
              std::move(callback).Run(
                  base::unexpected(std::string(kIwaDevModeNotEnabledMessage)));
              return;
            }
            // Dev mode proxy mode does not use Web Bundles, hence there is no
            // bundle to validate / trust and no signatures to check.
            std::move(callback).Run(base::ok());
          }},
      location);
}

void IsolatedWebAppInstallCommandHelper::CheckTrustAndSignaturesOfBundle(
    const base::FilePath& path,
    base::OnceCallback<void(base::expected<void, std::string>)> callback) {
  // To check whether the bundle is valid and trusted, we attempt to create a
  // `IsolatedWebAppResponseReader`. If a response reader is created
  // successfully, then this means that the Signed Web Bundle...
  // - ...is well formatted and uses a supported Web Bundle version.
  // - ...contains a valid integrity block with a trusted public key.
  // - ...has signatures that were verified successfully (as long as
  //   `skip_signature_verification` below is set to `false`).
  // - ...contains valid metadata / no invalid URLs.
  response_reader_factory_->CreateResponseReader(
      path, url_info_.web_bundle_id(),
      // During install and updates, we always want to verify signatures,
      // regardless of the OS.
      /*skip_signature_verification=*/false,
      base::BindOnce(&IsolatedWebAppInstallCommandHelper::
                         OnTrustAndSignaturesOfBundleChecked,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IsolatedWebAppInstallCommandHelper::OnTrustAndSignaturesOfBundleChecked(
    base::OnceCallback<void(base::expected<void, std::string>)> callback,
    base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                   UnusableSwbnFileError> result) {
  auto status =
      result
          .transform(
              [](const std::unique_ptr<IsolatedWebAppResponseReader>& reader)
                  -> void {})
          .transform_error([](const UnusableSwbnFileError& error) {
            return IsolatedWebAppResponseReaderFactory::ErrorToString(error);
          });
  std::unique_ptr<IsolatedWebAppResponseReader> reader;
  IsolatedWebAppResponseReader* raw_reader = nullptr;
  if (result.has_value()) {
    reader = std::move(result.value());
    raw_reader = reader.get();
  }

  base::OnceClosure run_result_callback = base::BindOnce(
      [](base::OnceCallback<void(base::expected<void, std::string>)> cb,
         base::expected<void, std::string> status,
         std::unique_ptr<IsolatedWebAppResponseReader>) {
        std::move(cb).Run(std::move(status));
      },
      std::move(callback), std::move(status), std::move(reader));
  if (raw_reader) {
    raw_reader->Close(std::move(run_result_callback));
  } else {
    std::move(run_result_callback).Run();
  }
}

void IsolatedWebAppInstallCommandHelper::CreateStoragePartitionIfNotPresent(
    Profile& profile) {
  profile.GetStoragePartition(url_info_.storage_partition_config(&profile),
                              /*can_create=*/true);
}

void IsolatedWebAppInstallCommandHelper::LoadInstallUrl(
    const IsolatedWebAppLocation& location,
    content::WebContents& web_contents,
    WebAppUrlLoader& url_loader,
    base::OnceCallback<void(base::expected<void, std::string>)> callback) {
  // |web_app::IsolatedWebAppURLLoaderFactory| uses the isolation data in
  // order to determine the current state of content serving (installation
  // process vs application data serving) and source of data (proxy, web
  // bundle, etc...).
  IsolatedWebAppPendingInstallInfo::FromWebContents(web_contents)
      .set_isolated_web_app_location(location);

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
      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&IsolatedWebAppInstallCommandHelper::OnLoadInstallUrl,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IsolatedWebAppInstallCommandHelper::OnLoadInstallUrl(
    base::OnceCallback<void(base::expected<void, std::string>)> callback,
    WebAppUrlLoaderResult result) {
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
    base::OnceCallback<void(base::expected<ManifestAndUrl, std::string>)>
        callback) {
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      &web_contents,
      base::BindOnce(&IsolatedWebAppInstallCommandHelper::
                         OnCheckInstallabilityAndRetrieveManifest,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IsolatedWebAppInstallCommandHelper::
    OnCheckInstallabilityAndRetrieveManifest(
        base::OnceCallback<void(base::expected<ManifestAndUrl, std::string>)>
            callback,
        blink::mojom::ManifestPtr opt_manifest,
        const GURL& manifest_url,
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

  // See |WebAppDataRetriever::CheckInstallabilityCallback| documentation for
  // details.
  DCHECK(!blink::IsEmptyManifest(opt_manifest))
      << "must not be empty when manifest is present.";

  // See |WebAppDataRetriever::CheckInstallabilityCallback| documentation for
  // details.
  DCHECK(!manifest_url.is_empty())
      << "must not be empty if manifest is not empty.";

  std::move(callback).Run(
      ManifestAndUrl(std::move(opt_manifest), manifest_url));
}

base::expected<WebAppInstallInfo, std::string>
IsolatedWebAppInstallCommandHelper::ValidateManifestAndCreateInstallInfo(
    const absl::optional<base::Version>& expected_version,
    const ManifestAndUrl& manifest_and_url) {
  const blink::mojom::Manifest& manifest = *manifest_and_url.manifest;
  const GURL& manifest_url = manifest_and_url.url;

  if (!manifest.id.is_valid()) {
    return base::unexpected(
        "Manifest `id` is not present or invalid. manifest_url: " +
        manifest_url.possibly_invalid_spec());
  }

  WebAppInstallInfo info(manifest.id);
  UpdateWebAppInfoFromManifest(manifest, manifest_url, &info);

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

  base::expected<std::vector<uint32_t>, IwaVersionParseError>
      version_components = ParseIwaVersionIntoComponents(version_string);
  if (!version_components.has_value()) {
    return base::unexpected(base::StrCat(
        {"Failed to parse `version` from the manifest: It must be in the form "
         "`x.y.z`, where `x`, `y`, and `z` are numbers without leading zeros. "
         "Detailed error: ",
         IwaVersionParseErrorToString(version_components.error()),
         " Got: ", version_string}));
  }
  base::Version version(
      std::vector(version_components->begin(), version_components->end()));

  if (expected_version.has_value() && *expected_version != version) {
    return base::unexpected(
        "Expected version (" + expected_version->GetString() +
        ") does not match the version provided in the manifest (" +
        version.GetString() + ")");
  }
  info.isolated_web_app_version = version;

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
  base::flat_set<GURL> icon_urls = GetValidIconUrlsToDownload(install_info);
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
    std::map<GURL, std::vector<SkBitmap>> icons_map,
    std::map<GURL, int /*http_status_code*/> unused_icons_http_results) {
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

IsolatedWebAppInstallCommandHelper::ManifestAndUrl::ManifestAndUrl(
    blink::mojom::ManifestPtr manifest,
    GURL url)
    : manifest(std::move(manifest)), url(std::move(url)) {}
IsolatedWebAppInstallCommandHelper::ManifestAndUrl::~ManifestAndUrl() = default;

IsolatedWebAppInstallCommandHelper::ManifestAndUrl::ManifestAndUrl(
    ManifestAndUrl&&) = default;
IsolatedWebAppInstallCommandHelper::ManifestAndUrl&
IsolatedWebAppInstallCommandHelper::ManifestAndUrl::operator=(
    ManifestAndUrl&&) = default;
}  // namespace web_app
