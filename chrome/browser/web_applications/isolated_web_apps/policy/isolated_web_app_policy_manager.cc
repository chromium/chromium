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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

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

IsolatedWebAppPolicyManager::IsolatedWebAppPolicyManager(
    const base::FilePath& context_dir,
    std::vector<IsolatedWebAppExternalInstallOptions> iwa_install_options,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::OnceCallback<void(std::vector<EphemeralAppInstallResult>)>
        ephemeral_install_cb)
    : ephemeral_iwa_install_options_(std::move(iwa_install_options)),
      current_app_(ephemeral_iwa_install_options_.begin()),
      installation_dir_(context_dir.Append(kEphemeralIwaRootDirectory)),
      url_loader_factory_(std::move(url_loader_factory)),
      result_vector_(ephemeral_iwa_install_options_.size(),
                     EphemeralAppInstallResult::kUnknown),
      ephemeral_install_cb_(std::move(ephemeral_install_cb)) {}
IsolatedWebAppPolicyManager::~IsolatedWebAppPolicyManager() = default;

void IsolatedWebAppPolicyManager::InstallEphemeralApps() {
  if (!profiles::IsPublicSession()) {
    LOG(ERROR) << "The IWAs should be installed only in managed guest session.";
    SetResultForAllEphemeralApps(
        EphemeralAppInstallResult::kErrorNotEphemeralSession);
    std::move(ephemeral_install_cb_).Run(result_vector_);
    return;
  }

  if (ephemeral_iwa_install_options_.empty()) {
    SetResultForAllEphemeralApps(EphemeralAppInstallResult::kSuccess);
    std::move(ephemeral_install_cb_).Run(result_vector_);
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
    SetResultForAllEphemeralApps(
        EphemeralAppInstallResult::kErrorCantCreateRootDirectory);
    std::move(ephemeral_install_cb_).Run(result_vector_);
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
      contacts {
        email: "peletskyi@google.com"
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
    SetResultForCurrentEphemeralApp(
        EphemeralAppInstallResult::kErrorUpdateManifestDownloadFailed);
    ContinueWithTheNextApp();
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

void IsolatedWebAppPolicyManager::SetResultForCurrentEphemeralApp(
    EphemeralAppInstallResult result) {
  const auto index =
      std::distance(ephemeral_iwa_install_options_.begin(), current_app_);
  result_vector_.at(index) = result;
}

void IsolatedWebAppPolicyManager::SetResultForAllEphemeralApps(
    EphemeralAppInstallResult result) {
  base::ranges::fill(result_vector_, result);
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
    SetResultForCurrentEphemeralApp(
        EphemeralAppInstallResult::kErrorUpdateManifestParsingFailed);
    ContinueWithTheNextApp();
    return;
  }
  LOG(ERROR) << "We have parsed update manifest of the app "
             << current_app_->web_bundle_id().id()
             << ". Further installation steps will be executed after "
                "the feature is complete.";
  // Even though the app is not installed because the feature is not yet
  // implemented, let's set the result to kSuccess as nothing went wrong for
  // the current app.
  SetResultForCurrentEphemeralApp(EphemeralAppInstallResult::kSuccess);
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

}  // namespace web_app
