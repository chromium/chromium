// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"

namespace web_app {
namespace {

using WebAppInstalInfoCallback =
    base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>;

class WebAppInstallInfoFetcher {
 public:
  explicit WebAppInstallInfoFetcher(Profile* profile,
                                    WebAppProvider* provider,
                                    const IsolatedWebAppUrlInfo& url_info,
                                    const IwaSourceBundleWithMode& source)
      : profile_(profile),
        source_(source),
        web_contents_(
            IsolatedWebAppInstallCommandHelper::CreateIsolatedWebAppWebContents(
                *profile)),
        url_loader_(provider->web_contents_manager().CreateUrlLoader()) {
    CHECK(profile);
    CHECK(provider);

    helper_ = std::make_unique<IsolatedWebAppInstallCommandHelper>(
        url_info, provider->web_contents_manager().CreateDataRetriever(),
        IsolatedWebAppInstallCommandHelper::CreateDefaultResponseReaderFactory(
            *profile));
  }

  void FetchAndReply(WebAppInstalInfoCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    callback_ = std::move(callback);

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
  void FailWithError(std::string_view error_message) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(callback_);
    std::move(callback_).Run(base::unexpected(std::string(error_message)));
  }

  void CheckTrustAndSignatures(base::OnceClosure next_step_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(helper_);
    helper_->CheckTrustAndSignatures(
        source_, profile_,
        base::BindOnce(&WebAppInstallInfoFetcher::RunNextStepOnSuccess<void>,
                       weak_factory_.GetWeakPtr(),
                       std::move(next_step_callback)));
  }

  void LoadInstallUrl(base::OnceClosure next_step_callback) {
    CHECK(helper_);
    CHECK(web_contents_);
    CHECK(url_loader_);
    helper_->LoadInstallUrl(
        source_, *web_contents_.get(), *url_loader_.get(),
        base::BindOnce(&WebAppInstallInfoFetcher::RunNextStepOnSuccess<void>,
                       weak_factory_.GetWeakPtr(),
                       std::move(next_step_callback)));
  }

  void CheckInstallabilityAndRetrieveManifest(
      base::OnceCallback<void(blink::mojom::ManifestPtr)> next_step_callback) {
    CHECK(helper_);
    helper_->CheckInstallabilityAndRetrieveManifest(
        *web_contents_.get(),
        base::BindOnce(&WebAppInstallInfoFetcher::RunNextStepOnSuccess<
                           blink::mojom::ManifestPtr>,
                       weak_factory_.GetWeakPtr(),
                       std::move(next_step_callback)));
  }

  void ValidateManifestAndCreateInstallInfo(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      blink::mojom::ManifestPtr manifest) {
    CHECK(helper_);
    base::expected<WebAppInstallInfo, std::string> install_info =
        helper_->ValidateManifestAndCreateInstallInfo(std::nullopt, *manifest);
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
  IwaSourceBundleWithMode source_;
  WebAppInstalInfoCallback callback_;

  std::unique_ptr<IsolatedWebAppInstallCommandHelper> helper_ = nullptr;

  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;

  base::WeakPtrFactory<WebAppInstallInfoFetcher> weak_factory_{this};
};

}  // namespace

// static
void SignedWebBundleMetadata::Create(
    Profile* profile,
    WebAppProvider* provider,
    const IsolatedWebAppUrlInfo& url_info,
    const IwaSourceBundleWithMode& source,
    SignedWebBundleMetadata::SignedWebBundleMetadataCallback callback) {
  auto fetcher = std::make_unique<WebAppInstallInfoFetcher>(profile, provider,
                                                            url_info, source);
  WebAppInstallInfoFetcher& fetcher_ref = *fetcher.get();

  fetcher_ref.FetchAndReply(base::BindOnce(
      [](const IsolatedWebAppUrlInfo& url_info,
         const IwaSourceBundleWithMode& source,
         SignedWebBundleMetadata::SignedWebBundleMetadataCallback callback,
         base::expected<WebAppInstallInfo, std::string> install_info) {
        std::move(callback).Run(install_info.transform(
            [&url_info, &source](const WebAppInstallInfo& install_info)
                -> SignedWebBundleMetadata {
              return SignedWebBundleMetadata(
                  url_info, source, install_info.title,
                  install_info.isolated_web_app_version,
                  install_info.icon_bitmaps);
            }));
      },
      url_info, source,
      std::move(callback).Then(base::OnceClosure(
          base::DoNothingWithBoundArgs(std::move(fetcher))))));
}

// static
SignedWebBundleMetadata SignedWebBundleMetadata::CreateForTesting(
    const IsolatedWebAppUrlInfo& url_info,
    const IwaSourceBundleWithMode& source,
    const std::u16string& app_name,
    const base::Version& version,
    const IconBitmaps& icons) {
  return SignedWebBundleMetadata(url_info, source, app_name, version, icons);
}

SignedWebBundleMetadata::SignedWebBundleMetadata(
    const IsolatedWebAppUrlInfo& url_info,
    const IwaSourceBundleWithMode& source,
    const std::u16string& app_name,
    const base::Version& version,
    const IconBitmaps& icons)
    : url_info_(url_info),
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
