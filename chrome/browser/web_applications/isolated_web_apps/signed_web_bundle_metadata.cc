// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "content/public/browser/web_contents.h"

namespace web_app {
namespace {

using WebAppInstalInfoCallback =
    base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>;
using SignedWebBundleMetadataCallback =
    SignedWebBundleMetadata::SignedWebBundleMetadataCallback;

class WebAppInstallInfoFetcher {
 public:
  explicit WebAppInstallInfoFetcher(Profile* profile,
                                    WebAppProvider* provider,
                                    const IsolatedWebAppUrlInfo& url_info,
                                    const IsolatedWebAppLocation& location)
      : profile_(profile),
        location_(location),
        web_contents_(
            IsolatedWebAppInstallCommandHelper::CreateIsolatedWebAppWebContents(
                *profile)),
        url_loader_(provider->web_contents_manager().CreateUrlLoader()) {
    CHECK(profile);
    CHECK(provider);

    helper_ = std::make_unique<IsolatedWebAppInstallCommandHelper>(
        url_info, provider->web_contents_manager().CreateDataRetriever(),
        IsolatedWebAppInstallCommandHelper::CreateDefaultResponseReaderFactory(
            *profile->GetPrefs()));
  }

  void FetchAndReply(WebAppInstalInfoCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    callback_ = std::move(callback);

    if (absl::holds_alternative<DevModeProxy>(location_)) {
      FailWithError("No Signed Web Bundle Metadata for dev-mode proxy.");
      return;
    }

    auto weak_ptr = weak_factory_.GetWeakPtr();
    RunChainedCallbacks(
        base::BindOnce(&WebAppInstallInfoFetcher::CheckTrustAndSignatures,
                       weak_ptr),
        base::BindOnce(&WebAppInstallInfoFetcher::LoadInstallUrl, weak_ptr),
        base::BindOnce(
            &WebAppInstallInfoFetcher::CheckInstallabilityAndRetrieveManifest,
            weak_ptr),
        base::BindOnce(
            &WebAppInstallInfoFetcher::ValidateManifestAndCreateInstallInfo,
            weak_ptr),
        base::BindOnce(
            &WebAppInstallInfoFetcher::RetrieveIconsAndPopulateInstallInfo,
            weak_ptr),
        base::BindOnce(&WebAppInstallInfoFetcher::CreateSignedWebBundleMetadata,
                       weak_ptr));
  }

 private:
  void FailWithError(base::StringPiece error_message) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(callback_);
    std::move(callback_).Run(base::unexpected(std::string(error_message)));
  }

  void CheckTrustAndSignatures(base::OnceClosure next_step_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(helper_);
    helper_->CheckTrustAndSignatures(
        location_, profile_,
        base::BindOnce(&WebAppInstallInfoFetcher::RunNextStepOnSuccess<void>,
                       weak_factory_.GetWeakPtr(),
                       std::move(next_step_callback)));
  }

  void LoadInstallUrl(base::OnceClosure next_step_callback) {
    CHECK(helper_);
    CHECK(web_contents_);
    CHECK(url_loader_);
    helper_->LoadInstallUrl(
        location_, *web_contents_.get(), *url_loader_.get(),
        base::BindOnce(&WebAppInstallInfoFetcher::RunNextStepOnSuccess<void>,
                       weak_factory_.GetWeakPtr(),
                       std::move(next_step_callback)));
  }

  void CheckInstallabilityAndRetrieveManifest(
      base::OnceCallback<
          void(IsolatedWebAppInstallCommandHelper::ManifestAndUrl)>
          next_step_callback) {
    CHECK(helper_);
    helper_->CheckInstallabilityAndRetrieveManifest(
        *web_contents_.get(),
        base::BindOnce(&WebAppInstallInfoFetcher::RunNextStepOnSuccess<
                           IsolatedWebAppInstallCommandHelper::ManifestAndUrl>,
                       weak_factory_.GetWeakPtr(),
                       std::move(next_step_callback)));
  }

  void ValidateManifestAndCreateInstallInfo(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      IsolatedWebAppInstallCommandHelper::ManifestAndUrl manifest_and_url) {
    CHECK(helper_);
    base::expected<WebAppInstallInfo, std::string> install_info =
        helper_->ValidateManifestAndCreateInstallInfo(absl::nullopt,
                                                      manifest_and_url);
    RunNextStepOnSuccess(std::move(next_step_callback),
                         std::move(install_info));
  }

  void RetrieveIconsAndPopulateInstallInfo(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      WebAppInstallInfo install_info) {
    CHECK(helper_);
    helper_->RetrieveIconsAndPopulateInstallInfo(
        std::move(install_info), *web_contents_.get(),
        base::BindOnce(
            &WebAppInstallInfoFetcher::RunNextStepOnSuccess<WebAppInstallInfo>,
            weak_factory_.GetWeakPtr(), std::move(next_step_callback)));
  }

  void CreateSignedWebBundleMetadata(WebAppInstallInfo install_info) {
    CHECK(callback_);
    std::move(callback_).Run(std::move(install_info));
  }

  template <typename T, std::enable_if_t<std::is_void_v<T>, bool> = true>
  void RunNextStepOnSuccess(base::OnceClosure next_step_callback,
                            base::expected<T, std::string> status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!status.has_value()) {
      FailWithError(status.error());
    } else {
      std::move(next_step_callback).Run();
    }
  }

  template <typename T, std::enable_if_t<!std::is_void_v<T>, bool> = true>
  void RunNextStepOnSuccess(base::OnceCallback<void(T)> next_step_callback,
                            base::expected<T, std::string> status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!status.has_value()) {
      FailWithError(status.error());
    } else {
      std::move(next_step_callback).Run(std::move(*status));
    }
  }

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<Profile> profile_;
  IsolatedWebAppLocation location_;
  WebAppInstalInfoCallback callback_;

  std::unique_ptr<IsolatedWebAppInstallCommandHelper> helper_ = nullptr;

  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<WebAppUrlLoader> url_loader_;

  base::WeakPtrFactory<WebAppInstallInfoFetcher> weak_factory_{this};
};

}  // namespace

// static
void SignedWebBundleMetadata::Create(Profile* profile,
                                     WebAppProvider* provider,
                                     const IsolatedWebAppUrlInfo& url_info,
                                     const IsolatedWebAppLocation& location,
                                     SignedWebBundleMetadataCallback callback) {
  auto fetcher = std::make_unique<WebAppInstallInfoFetcher>(profile, provider,
                                                            url_info, location);
  WebAppInstallInfoFetcher& fetcher_ref = *fetcher.get();

  fetcher_ref.FetchAndReply(base::BindOnce(
      [](const IsolatedWebAppUrlInfo& url_info,
         const IsolatedWebAppLocation& location,
         SignedWebBundleMetadataCallback callback,
         base::expected<WebAppInstallInfo, std::string> install_info) {
        std::move(callback).Run(install_info.transform(
            [&url_info, &location](const WebAppInstallInfo& install_info)
                -> SignedWebBundleMetadata {
              return SignedWebBundleMetadata(
                  url_info, location, install_info.title,
                  install_info.isolated_web_app_version,
                  install_info.icon_bitmaps);
            }));
      },
      url_info, location,
      std::move(callback).Then(base::OnceClosure(
          base::DoNothingWithBoundArgs(std::move(fetcher))))));
}

// static
SignedWebBundleMetadata SignedWebBundleMetadata::CreateForTesting(
    const IsolatedWebAppUrlInfo& url_info,
    const IsolatedWebAppLocation& location,
    const std::u16string& app_name,
    const base::Version& version,
    const IconBitmaps& icons) {
  return SignedWebBundleMetadata(url_info, location, app_name, version, icons);
}

SignedWebBundleMetadata::SignedWebBundleMetadata(
    const IsolatedWebAppUrlInfo& url_info,
    const IsolatedWebAppLocation& location,
    const std::u16string& app_name,
    const base::Version& version,
    const IconBitmaps& icons)
    : url_info_(url_info),
      location_(location),
      app_name_(app_name),
      version_(version),
      icons_(icons) {}

SignedWebBundleMetadata::~SignedWebBundleMetadata() = default;

SignedWebBundleMetadata::SignedWebBundleMetadata(
    const SignedWebBundleMetadata&) = default;

SignedWebBundleMetadata& SignedWebBundleMetadata::operator=(
    const SignedWebBundleMetadata&) = default;

bool SignedWebBundleMetadata::operator==(
    const SignedWebBundleMetadata& other) const {
  return url_info_ == other.url_info_ && app_name_ == other.app_name_ &&
         version_ == other.version_ && icons_ == other.icons_;
}

}  // namespace web_app
