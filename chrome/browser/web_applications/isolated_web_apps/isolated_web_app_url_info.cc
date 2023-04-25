// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"

namespace web_app {

namespace {

void GetSignedWebBundleIdByPath(
    const base::FilePath& path,
    base::OnceCallback<void(base::expected<IsolatedWebAppUrlInfo, std::string>)>
        callback) {
  std::unique_ptr<SignedWebBundleReader> reader =
      SignedWebBundleReader::Create(path, /*base_url=*/absl::nullopt);

  SignedWebBundleReader* reader_raw_ptr = reader.get();

  auto [callback_first, callback_second] =
      base::SplitOnceCallback(std::move(callback));

  SignedWebBundleReader::IntegrityBlockReadResultCallback
      integrity_block_result_callback = base::BindOnce(
          [](base::OnceCallback<void(
                 base::expected<IsolatedWebAppUrlInfo, std::string>)> callback,
             web_package::SignedWebBundleIntegrityBlock integrity_block,
             base::OnceCallback<void(
                 SignedWebBundleReader::SignatureVerificationAction)>
                 verify_callback) {
            std::move(verify_callback)
                .Run(SignedWebBundleReader::SignatureVerificationAction::Abort(
                    "Stopped after reading the integrity block."));
            web_package::SignedWebBundleId bundle_id =
                integrity_block.signature_stack().derived_web_bundle_id();
            std::move(callback).Run(
                IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id));
          },
          std::move(callback_first));

  SignedWebBundleReader::ReadErrorCallback read_error_callback = base::BindOnce(
      [](std::unique_ptr<SignedWebBundleReader> reader_ownership,
         base::OnceCallback<void(
             base::expected<IsolatedWebAppUrlInfo, std::string>)> callback,
         absl::optional<
             SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>
             read_error) {
        DCHECK(read_error.has_value());

        if (!absl::holds_alternative<SignedWebBundleReader::AbortedByCaller>(
                read_error.value())) {
          web_package::mojom::BundleIntegrityBlockParseErrorPtr* error_ptr =
              absl::get_if<
                  web_package::mojom::BundleIntegrityBlockParseErrorPtr>(
                  &read_error.value());
          // only other possible variant, as the other 2 variants shouldn't be
          // reachable.
          DCHECK(error_ptr);

          std::move(callback).Run(base::unexpected(
              "Failed to read the integrity block of the signed web bundle: " +
              (*error_ptr)->message));
        }
      },
      std::move(reader), std::move(callback_second));
  ;

  reader_raw_ptr->StartReading(std::move(integrity_block_result_callback),
                               std::move(read_error_callback));
}

}  // namespace

// static
base::expected<IsolatedWebAppUrlInfo, std::string>
IsolatedWebAppUrlInfo::Create(const GURL& url) {
  if (!url.is_valid()) {
    return base::unexpected("Invalid URL");
  }
  if (!url.SchemeIs(chrome::kIsolatedAppScheme)) {
    return base::unexpected(
        base::StrCat({"The URL scheme must be ", chrome::kIsolatedAppScheme,
                      ", but was ", url.scheme()}));
  }

  // Valid isolated-app:// `GURL`s can never include credentials or ports, since
  // the scheme is configured as `url::SCHEME_WITH_HOST`. The `DCHECK` is here
  // just in case, but should never trigger as long as the scheme is configured
  // correctly.
  DCHECK(!url.has_username() && !url.has_password() && !url.has_port() &&
         url.IsStandard());

  auto web_bundle_id = web_package::SignedWebBundleId::Create(url.host());
  if (!web_bundle_id.has_value()) {
    return base::unexpected(
        base::StrCat({"The host of isolated-app:// URLs must be a valid Signed "
                      "Web Bundle ID (got ",
                      url.host(), "): ", web_bundle_id.error()}));
  }

  return IsolatedWebAppUrlInfo(*web_bundle_id);
}

// static
IsolatedWebAppUrlInfo IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
    const web_package::SignedWebBundleId& web_bundle_id) {
  return IsolatedWebAppUrlInfo(web_bundle_id);
}

// static
void IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppLocation(
    const IsolatedWebAppLocation& location,
    base::OnceCallback<void(base::expected<IsolatedWebAppUrlInfo, std::string>)>
        callback) {
  absl::visit(base::Overloaded{
                  [&](const InstalledBundle&) {
                    std::move(callback).Run(base::unexpected(
                        "Getting IsolationInfo from |InstalledBundle| is not "
                        "implemented"));
                  },
                  [&](const DevModeBundle& dev_mode_bundle) {
                    GetSignedWebBundleIdByPath(dev_mode_bundle.path,
                                               std::move(callback));
                  },
                  [&](const DevModeProxy&) {
                    std::move(callback).Run(
                        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                            web_package::SignedWebBundleId::
                                CreateRandomForDevelopment()));
                  }},
              location);
}

IsolatedWebAppUrlInfo::IsolatedWebAppUrlInfo(
    const web_package::SignedWebBundleId& web_bundle_id)
    : origin_(url::Origin::CreateFromNormalizedTuple(chrome::kIsolatedAppScheme,
                                                     web_bundle_id.id(),
                                                     /*port=*/0)),
      // The manifest id of Isolated Web Apps must resolve to the app's origin.
      // The manifest parser will resolve "id" relative the origin of the app's
      // start_url, and then sets Manifest::id to the path of this resolved URL,
      // not including a leading slash. Because of this, the resolved manifest
      // id will always be empty string.
      app_id_(GenerateAppId(/*manifest_id=*/"", origin_.GetURL())),
      web_bundle_id_(web_bundle_id) {}

const url::Origin& IsolatedWebAppUrlInfo::origin() const {
  return origin_;
}

const AppId& IsolatedWebAppUrlInfo::app_id() const {
  return app_id_;
}

const web_package::SignedWebBundleId& IsolatedWebAppUrlInfo::web_bundle_id()
    const {
  return web_bundle_id_;
}

content::StoragePartitionConfig IsolatedWebAppUrlInfo::storage_partition_config(
    content::BrowserContext* browser_context) const {
  DCHECK(browser_context != nullptr);
  return content::StoragePartitionConfig::Create(browser_context,
                                                 partition_domain(),
                                                 /*partition_name=*/"",
                                                 /*in_memory=*/false);
}

content::StoragePartitionConfig
IsolatedWebAppUrlInfo::GetStoragePartitionConfigForControlledFrame(
    content::BrowserContext* browser_context,
    const std::string& partition_name,
    bool in_memory) const {
  DCHECK(browser_context);
  DCHECK(!partition_name.empty() || in_memory);
  return content::StoragePartitionConfig::Create(
      browser_context, partition_domain(), partition_name, in_memory);
}

std::string IsolatedWebAppUrlInfo::partition_domain() const {
  constexpr char kIsolatedWebAppPartitionPrefix[] = "iwa-";
  // We add a prefix to `partition_domain` to avoid potential name conflicts
  // with Chrome Apps, which use their id/hostname as `partition_domain`.
  return kIsolatedWebAppPartitionPrefix + origin().host();
}

}  // namespace web_app
