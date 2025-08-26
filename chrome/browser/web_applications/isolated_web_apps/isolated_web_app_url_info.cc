// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"

#include <utility>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "components/webapps/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/webapps/isolated_web_apps/reading/signed_web_bundle_reader.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"
#include "crypto/sha2.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

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
  return IwaOrigin::Create(url).transform(
      [](const auto& iwa_origin) { return IsolatedWebAppUrlInfo(iwa_origin); });
}

// static
IsolatedWebAppUrlInfo IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
    const web_package::SignedWebBundleId& web_bundle_id) {
  return IsolatedWebAppUrlInfo(IwaOrigin(web_bundle_id));
}

// static
void IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppSource(
    const IwaSource& source,
    base::OnceCallback<void(base::expected<IsolatedWebAppUrlInfo, std::string>)>
        callback) {
  std::visit(
      absl::Overload{
          [&](const IwaSourceBundle& bundle) {
            GetSignedWebBundleIdByPath(bundle.path(), std::move(callback));
          },
          [&](const IwaSourceProxy& proxy) {
            const web_package::SignedWebBundleId bundle_id = [&] {
              if (proxy.explicit_bundle_id()) {
                CHECK(proxy.explicit_bundle_id()->is_for_proxy_mode());
                return *proxy.explicit_bundle_id();
              } else {
                return web_package::SignedWebBundleId::
                    CreateRandomForProxyMode();
              }
            }();

            std::move(callback).Run(
                IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id));
          }},
      source.variant());
}

IsolatedWebAppUrlInfo::IsolatedWebAppUrlInfo(const IwaOrigin& iwa_origin)
    : iwa_origin_(iwa_origin),
      // The manifest id of Isolated Web Apps must resolve to the app's origin.
      // The manifest parser will resolve "id" relative the origin of the app's
      // start_url, and then sets Manifest::id to the path of this resolved URL,
      // not including a leading slash. Because of this, the resolved manifest
      // id will always be empty string.
      app_id_(
          GenerateAppId(/*manifest_id=*/"", iwa_origin_.origin().GetURL())) {}

const url::Origin& IsolatedWebAppUrlInfo::origin() const {
  return iwa_origin_.origin();
}

const webapps::AppId& IsolatedWebAppUrlInfo::app_id() const {
  return app_id_;
}

const web_package::SignedWebBundleId& IsolatedWebAppUrlInfo::web_bundle_id()
    const {
  return iwa_origin_.web_bundle_id();
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
  // We add a prefix to `partition_domain` to distinguish from other users of
  // storage partitions.
  return "i" + base::Base64Encode(crypto::SHA256HashString(app_id_));
}

}  // namespace web_app
