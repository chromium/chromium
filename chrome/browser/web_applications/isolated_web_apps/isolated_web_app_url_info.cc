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
#include "components/webapps/isolated_web_apps/bundle_operations/bundle_operations.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"
#include "crypto/sha2.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace web_app {

namespace {
base::expected<IsolatedWebAppUrlInfo, std::string> MakeIsolatedWebAppUrlInfo(
    base::expected<web_package::SignedWebBundleId, std::string> bundle_id) {
  return bundle_id.transform([](const web_package::SignedWebBundleId& id) {
    return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(id);
  });
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
            ReadSignedWebBundleIdInsecurely(
                bundle.path(), base::BindOnce(&MakeIsolatedWebAppUrlInfo)
                                   .Then(std::move(callback)));
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
  return iwa_origin_.storage_partition_config(browser_context);
}

content::StoragePartitionConfig
IsolatedWebAppUrlInfo::GetStoragePartitionConfigForControlledFrame(
    content::BrowserContext* browser_context,
    const std::string& partition_name,
    bool in_memory) const {
  CHECK(!partition_name.empty() || in_memory);
  return iwa_origin_.storage_partition_config(
      browser_context, IwaOrigin::StoragePartitionConfigOptions{
                           .partition_name = partition_name,
                           .in_memory = in_memory,
                       });
}

}  // namespace web_app
