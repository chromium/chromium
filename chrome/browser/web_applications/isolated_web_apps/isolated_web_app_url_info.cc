// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"
#include "crypto/sha2.h"

namespace web_app {

namespace {
base::expected<IsolatedWebAppUrlInfo, std::string> MakeIsolatedWebAppUrlInfo(
    base::expected<web_package::SignedWebBundleId, UnusableSwbnFileError>
        bundle_id) {
  return bundle_id
      .transform([](const web_package::SignedWebBundleId& id) {
        return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(id);
      })
      .transform_error([](const UnusableSwbnFileError& error) {
        return "Failed to read the integrity block of the signed web bundle: " +
               error.message();
      });
}

void GetSignedWebBundleIdByPath(
    const base::FilePath& path,
    base::OnceCallback<void(base::expected<IsolatedWebAppUrlInfo, std::string>)>
        url_info_ready_callback) {
  UnsecureSignedWebBundleIdReader::WebBundleIdCallback id_read_callback =
      base::BindOnce(&MakeIsolatedWebAppUrlInfo)
          .Then(std::move(url_info_ready_callback));

  UnsecureSignedWebBundleIdReader::GetWebBundleId(path,
                                                  std::move(id_read_callback));
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
void IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppSource(
    const IwaSource& source,
    base::OnceCallback<void(base::expected<IsolatedWebAppUrlInfo, std::string>)>
        callback) {
  absl::visit(base::Overloaded{
                  [&](const IwaSourceBundle& bundle) {
                    GetSignedWebBundleIdByPath(bundle.path(),
                                               std::move(callback));
                  },
                  [&](const IwaSourceProxy&) {
                    std::move(callback).Run(
                        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                            web_package::SignedWebBundleId::
                                CreateRandomForProxyMode()));
                  }},
              source.variant());
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

const webapps::AppId& IsolatedWebAppUrlInfo::app_id() const {
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

bool IsolatedWebAppUrlInfo::operator==(
    const IsolatedWebAppUrlInfo& other) const {
  return origin_ == other.origin_ && app_id_ == other.app_id_ &&
         web_bundle_id_ == other.web_bundle_id_;
}

std::string IsolatedWebAppUrlInfo::partition_domain() const {
  // We add a prefix to `partition_domain` to distinguish from other users of
  // storage partitions.
  return "i" + base::Base64Encode(crypto::SHA256HashString(app_id_));
}

}  // namespace web_app
