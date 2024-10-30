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
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/jobs/prepare_install_info_job.h"
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
      : profile_(*profile),
        provider_(*provider),
        source_(source),
        helper_(std::make_unique<IsolatedWebAppInstallCommandHelper>(
            url_info,
            provider->web_contents_manager().CreateDataRetriever(),
            IsolatedWebAppInstallCommandHelper::
                CreateDefaultResponseReaderFactory(*profile))),
        web_contents_(
            IsolatedWebAppInstallCommandHelper::CreateIsolatedWebAppWebContents(
                *profile)) {}

  void FetchAndReply(WebAppInstalInfoCallback callback) {
    callback_ = std::move(callback);

    RunChainedWeakCallbacks(
        weak_factory_.GetWeakPtr(),
        &WebAppInstallInfoFetcher::CheckTrustAndSignatures,
        &WebAppInstallInfoFetcher::PrepareInstallInfo,
        &WebAppInstallInfoFetcher::CreateSignedWebBundleMetadata);
  }

 private:
  using TrustCheckResult = base::expected<void, std::string>;

  void FailWithError(const std::string& error_message) {
    CHECK(callback_);
    std::move(callback_).Run(base::unexpected(error_message));
  }

  void CheckTrustAndSignatures(base::OnceClosure next_step_callback) {
    helper_->CheckTrustAndSignatures(
        source_, &*profile_,
        base::BindOnce(&WebAppInstallInfoFetcher::OnTrustAndSignaturesChecked,
                       weak_factory_.GetWeakPtr(),
                       std::move(next_step_callback)));
  }

  void OnTrustAndSignaturesChecked(base::OnceClosure next_step_callback,
                                   TrustCheckResult trust_check_result) {
    RETURN_IF_ERROR(trust_check_result,
                    [&](const std::string& error) { FailWithError(error); });
    std::move(next_step_callback).Run();
  }

  void PrepareInstallInfo(
      base::OnceCallback<void(PrepareInstallInfoJob::InstallInfoOrFailure)>
          next_step_callback) {
    prepare_install_info_job_ = PrepareInstallInfoJob::CreateAndStart(
        *profile_, source_,
        /*expected_version=*/std::nullopt, *web_contents_, *helper_,
        provider_->web_contents_manager().CreateUrlLoader(),
        std::move(next_step_callback));
  }

  void CreateSignedWebBundleMetadata(
      PrepareInstallInfoJob::InstallInfoOrFailure result) {
    prepare_install_info_job_.reset();

    ASSIGN_OR_RETURN(
        WebAppInstallInfo install_info, std::move(result),
        [&](const auto& failure) { FailWithError(failure.message); });

    CHECK(callback_);
    std::move(callback_).Run(std::move(install_info));
  }

  const raw_ref<Profile> profile_;
  const raw_ref<WebAppProvider> provider_;

  IwaSourceBundleWithMode source_;
  WebAppInstalInfoCallback callback_;

  std::unique_ptr<IsolatedWebAppInstallCommandHelper> helper_;
  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<PrepareInstallInfoJob> prepare_install_info_job_;

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
