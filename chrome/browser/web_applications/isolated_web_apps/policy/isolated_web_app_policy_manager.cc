// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {
constexpr size_t kMaxUpdateManifestLength = 5 * 1024 * 1024;

base::File::Error CreateNonExistingDirectory(const base::FilePath& path) {
  if (base::PathExists(path)) {
    return base::File::FILE_ERROR_EXISTS;
  }
  base::File::Error err = base::File::FILE_OK;
  base::CreateDirectoryAndGetError(path, &err);
  return err;
}

}  // namespace

namespace web_app {
IsolatedWebAppPolicyManager::IwaInstallCommandWrapperImpl::
    IwaInstallCommandWrapperImpl(web_app::WebAppProvider* provider)
    : provider_(provider) {}

void IsolatedWebAppPolicyManager::IwaInstallCommandWrapperImpl::Install(
    const IsolationData& isolation_data,
    const IsolatedWebAppUrlInfo& isolation_info,
    WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) {
  provider_->scheduler().InstallIsolatedWebApp(isolation_info, isolation_data,
                                               std::move(callback));
}

IsolatedWebAppPolicyManager::IsolatedWebAppPolicyManager(
    const base::FilePath& context_dir,
    std::vector<IsolatedWebAppExternalInstallOptions> iwa_install_options,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<IwaInstallCommandWrapper> installer,
    base::OnceCallback<void(std::vector<EphemeralAppInstallResult>)>
        ephemeral_install_cb)
    : ephemeral_iwa_install_options_(std::move(iwa_install_options)),
      current_app_(ephemeral_iwa_install_options_.begin()),
      installation_dir_(context_dir.Append(kEphemeralIwaRootDirectory)),
      url_loader_factory_(std::move(url_loader_factory)),
      result_vector_(ephemeral_iwa_install_options_.size(),
                     EphemeralAppInstallResult::kUnknown),
      installer_(std::move(installer)),
      ephemeral_install_cb_(std::move(ephemeral_install_cb)) {}
IsolatedWebAppPolicyManager::~IsolatedWebAppPolicyManager() = default;

void IsolatedWebAppPolicyManager::InstallEphemeralApps() {
  if (!profiles::IsPublicSession()) {
    LOG(ERROR) << "The IWAs should be installed only in managed guest session.";
    SetResultForAllAndFinish(
        EphemeralAppInstallResult::kErrorNotEphemeralSession);
    return;
  }

  if (ephemeral_iwa_install_options_.empty()) {
    SetResultForAllAndFinish(EphemeralAppInstallResult::kSuccess);
    return;
  }

  CreateIwaEphemeralRootDirectory();
}

void IsolatedWebAppPolicyManager::CreateIwaEphemeralRootDirectory() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&CreateNonExistingDirectory, installation_dir_),
      base::BindOnce(
          &IsolatedWebAppPolicyManager::OnIwaEphemeralRootDirectoryCreated,
          weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppPolicyManager::OnIwaEphemeralRootDirectoryCreated(
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    LOG(ERROR) << "Error in creating the directory for ephemeral IWAs: "
               << base::File::ErrorToString(error);
    SetResultForAllAndFinish(
        EphemeralAppInstallResult::kErrorCantCreateRootDirectory);
    return;
  }

  DownloadUpdateManifest();
}

void IsolatedWebAppPolicyManager::DownloadUpdateManifest() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("iwa_policy_update_manifest", R"(
    semantics {
      sender: "Isolated Web App Update Manifest Downloader"
      description:
        "Downloads update manifest of the Isolated Web App that is provided "
        "in the enterprise policy by the administrator. The update manifest "
        "contains at least the list of the available versions of the IWA "
        "and the URL to the Signed Web Bundles that correspond to "
        "each version."
      trigger:
        "Installation/update of a IWA from the enterprise policy requires "
        "fetching of a IWA Update Manifest"
      data:
        "This request does not send any data. It loads update manifest "
        "by a URL provided by the admin."
      destination: OTHER
      internal {
        contacts {
          email: "peletskyi@google.com"
        }
      }
    }
    policy {
      cookies_allowed: NO
      setting: "This feature cannot be disabled in settings."
      chrome_policy {
        IsolatedWebAppInstallForceList {
          IsolatedWebAppInstallForceList: ""
        }
      }
    })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = current_app_->update_manifest_url();
  // Cookies are not allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       std::move(traffic_annotation));

  simple_loader->SetRetryOptions(
      /* max_retries=*/3,
      network::SimpleURLLoader::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  network::SimpleURLLoader* const simple_loader_ptr = simple_loader.get();
  base::OnceCallback<void(std::unique_ptr<std::string>)> cb =
      base::BindOnce(&IsolatedWebAppPolicyManager::OnUpdateManifestDownloaded,
                     weak_factory_.GetWeakPtr(), std::move(simple_loader));

  simple_loader_ptr->DownloadToString(url_loader_factory_.get(), std::move(cb),
                                      kMaxUpdateManifestLength);
}

void IsolatedWebAppPolicyManager::OnUpdateManifestDownloaded(
    std::unique_ptr<network::SimpleURLLoader> simple_loader,
    std::unique_ptr<std::string> update_manifest_string) {
  // We may extract some information from the loader about
  // downloading errors in the future.
  simple_loader.reset();

  if (!update_manifest_string) {
    SetResultAndContinue(
        EphemeralAppInstallResult::kErrorUpdateManifestDownloadFailed);
    return;
  }

  ParseUpdateManifest(*update_manifest_string);
}

void IsolatedWebAppPolicyManager::ContinueWithTheNextApp() {
  ++current_app_;
  if (current_app_ == ephemeral_iwa_install_options_.end()) {
    json_parser_.reset();
    std::move(ephemeral_install_cb_).Run(result_vector_);
    return;
  }

  DownloadUpdateManifest();
}

void IsolatedWebAppPolicyManager::SetResultAndContinue(
    EphemeralAppInstallResult result) {
  const auto index =
      std::distance(ephemeral_iwa_install_options_.begin(), current_app_);
  result_vector_.at(index) = result;

  // If the error occurs after the directory for an app had been created,
  // then we should wipe the directory.
  if (result != EphemeralAppInstallResult::kSuccess) {
    WipeCurrentIwaDirectory();
    return;
  }

  ContinueWithTheNextApp();
}

void IsolatedWebAppPolicyManager::SetResultForAllAndFinish(
    EphemeralAppInstallResult result) {
  base::ranges::fill(result_vector_, result);
  std::move(ephemeral_install_cb_).Run(result_vector_);
}

void IsolatedWebAppPolicyManager::ParseUpdateManifest(
    const std::string& manifest_content) {
  auto* json_parser_ptr = GetJsonParserPtr();
  json_parser_ptr->Parse(
      manifest_content, base::JSON_PARSE_RFC,
      base::BindOnce(&IsolatedWebAppPolicyManager::OnUpdateManifestParsed,
                     base::Unretained(this)));
}

void IsolatedWebAppPolicyManager::OnUpdateManifestParsed(
    absl::optional<base::Value> result,
    const absl::optional<std::string>& error) {
  if (!result.has_value()) {
    SetResultAndContinue(
        EphemeralAppInstallResult::kErrorUpdateManifestParsingFailed);
    return;
  }
  absl::optional<GURL> web_bundle_url = ExtractWebBundleURL(result.value());
  if (!web_bundle_url.has_value()) {
    SetResultAndContinue(
        EphemeralAppInstallResult::kErrorWebBundleUrlCantBeDetermined);
    return;
  }

  current_app_->set_web_bundle_url(web_bundle_url.value());
  CreateIwaDirectory();
}

void IsolatedWebAppPolicyManager::CreateIwaDirectory() {
  base::FilePath iwa_dir =
      installation_dir_.Append(current_app_->web_bundle_id().id());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&CreateNonExistingDirectory, iwa_dir),
      base::BindOnce(&IsolatedWebAppPolicyManager::OnIwaDirectoryCreated,
                     weak_factory_.GetWeakPtr(), iwa_dir));
}

void IsolatedWebAppPolicyManager::OnIwaDirectoryCreated(
    const base::FilePath& iwa_dir,
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    SetResultAndContinue(
        EphemeralAppInstallResult::kErrorCantCreateIwaDirectory);
    return;
  }

  current_app_->set_app_directory(iwa_dir);
  DownloadWebBundle();
}

void IsolatedWebAppPolicyManager::DownloadWebBundle() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("iwa_policy_signed_web_bundle", R"(
  semantics {
    sender: "Isolated Web App Signed Web Bundle Downloader"
    description:
      "Downloads the Signed Web Bundle of the Isolated Web App (IWA) "
      "by the URL taken form the Update Manifest."
    trigger:
      "Installing/update of every IWA (including policy-based installs) "
      "require in a Signed Web Bundle that we download here."
    data:
      "This request does not send any data. It just downloads a Web Bundle."
    destination: OTHER
    internal {
      contacts {
        email: "peletskyi@google.com"
      }
    }
  }
  policy {
    cookies_allowed: NO
    setting: "This feature cannot be disabled in settings."
    chrome_policy {
      IsolatedWebAppInstallForceList {
        IsolatedWebAppInstallForceList: ""
      }
    }
  })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = current_app_->web_bundle_url();
  // Cookies are not allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       std::move(traffic_annotation));

  simple_loader->SetRetryOptions(
      /* max_retries=*/3,
      network::SimpleURLLoader::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  network::SimpleURLLoader* const simple_loader_ptr = simple_loader.get();
  base::OnceCallback<void(base::FilePath path)> cb =
      base::BindOnce(&IsolatedWebAppPolicyManager::OnWebBundleDownloaded,
                     weak_factory_.GetWeakPtr(), std::move(simple_loader));

  base::FilePath swbn_path =
      current_app_->app_directory().Append(kMainSignedWebBundleFileName);
  simple_loader_ptr->DownloadToFile(url_loader_factory_.get(), std::move(cb),
                                    swbn_path);
}

void IsolatedWebAppPolicyManager::OnWebBundleDownloaded(
    std::unique_ptr<network::SimpleURLLoader> simple_loader,
    base::FilePath path) {
  if (path.empty()) {
    SetResultAndContinue(
        EphemeralAppInstallResult::kErrorCantDownloadWebBundle);
    return;
  }

  IsolationData isolation_data =
      IsolationData(IsolationData::InstalledBundle{.path = path});
  IsolatedWebAppUrlInfo isolation_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          current_app_->web_bundle_id());

  installer_->Install(
      isolation_data, isolation_info,
      base::BindOnce(&IsolatedWebAppPolicyManager::OnIwaInstalled,
                     weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppPolicyManager::OnIwaInstalled(
    base::expected<InstallIsolatedWebAppCommandSuccess,
                   InstallIsolatedWebAppCommandError> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Could not install the IWA "
               << current_app_->web_bundle_id().id();
    SetResultAndContinue(
        EphemeralAppInstallResult::kErrorCantInstallFromWebBundle);
    return;
  }

  SetResultAndContinue(EphemeralAppInstallResult::kSuccess);
}

void IsolatedWebAppPolicyManager::WipeCurrentIwaDirectory() {
  const base::FilePath iwa_path_to_delete(current_app_->app_directory());
  current_app_->reset_app_directory();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&base::DeletePathRecursively, iwa_path_to_delete),
      base::BindOnce(&IsolatedWebAppPolicyManager::OnCurrentIwaDirectoryWiped,
                     weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppPolicyManager::OnCurrentIwaDirectoryWiped(bool wipe_result) {
  if (!wipe_result) {
    LOG(ERROR) << "Could not wipe an IWA directory";
  }

  ContinueWithTheNextApp();
}

data_decoder::mojom::JsonParser*
IsolatedWebAppPolicyManager::GetJsonParserPtr() {
  if (!json_parser_) {
    data_decoder_.GetService()->BindJsonParser(
        json_parser_.BindNewPipeAndPassReceiver());
    json_parser_.set_disconnect_handler(
        base::BindOnce(&IsolatedWebAppPolicyManager::OnUpdateManifestParsed,
                       base::Unretained(this), absl::nullopt,
                       "Data Decoder terminated unexpectedly"));
  }

  return json_parser_.get();
}

// static
absl::optional<GURL> IsolatedWebAppPolicyManager::ExtractWebBundleURL(
    const base::Value& parsed_update_manifest) {
  if (!parsed_update_manifest.is_dict()) {
    return absl::nullopt;
  }

  const base::Value::List* versions =
      parsed_update_manifest.GetDict().FindList(kUpdateManifestAllVersionsKey);
  if (!versions) {
    return absl::nullopt;
  }

  base::Version latest_version;
  const std::string* latest_url_string = nullptr;
  for (const auto& version_entry : *versions) {
    if (!version_entry.is_dict()) {
      return absl::nullopt;
    }

    const std::string* const version_string =
        version_entry.FindStringKey(kUpdateManifestVersionKey);
    const std::string* const url_string =
        version_entry.FindStringKey(kUpdateManifestSrcKey);
    if (!version_string || !url_string) {
      // No version or Web Bundle URL. Let's return error as this update
      // manifest looks strange.
      return absl::nullopt;
    }

    base::Version version(*version_string);
    if (!version.IsValid()) {
      // We can't parse the version. It might be that exactly this version was
      // meant to be the latest but there was a typo. It is better not to
      // install any version of the app than install old and potentially
      // compromised one.
      return absl::nullopt;
    }

    if (!latest_version.IsValid() || version > latest_version) {
      latest_version = version;
      latest_url_string = url_string;
    } else if (version == latest_version) {
      // Several apps of the same version is definitely an error case
      return absl::nullopt;
    }
  }

  if (!latest_version.IsValid() || !latest_url_string) {
    return absl::nullopt;
  }

  GURL url(*latest_url_string);
  if (url.is_valid()) {
    return url;
  }
  return absl::nullopt;
}

}  // namespace web_app
