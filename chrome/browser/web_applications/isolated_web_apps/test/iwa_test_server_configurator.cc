// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/iwa_test_server_configurator.h"

#include <string>

#include "base/json/json_writer.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/bundle_versions_storage.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"

namespace web_app {

namespace {

constexpr std::string_view kServerBaseUrl = "https://bundle-server.com";

}  // namespace

IwaTestServerConfigurator::IwaTestServerConfigurator(
    network::TestURLLoaderFactory& factory)
    : factory_(factory) {
  storage_.SetBaseUrl(GURL(kServerBaseUrl));
}

IwaTestServerConfigurator::~IwaTestServerConfigurator() = default;

void IwaTestServerConfigurator::AddBundle(
    std::unique_ptr<BundledIsolatedWebApp> bundle,
    std::optional<std::vector<UpdateChannel>> update_channels) {
  auto& bundle_ref = *bundle;

  auto web_bundle_url =
      storage_.AddBundle(std::move(bundle), std::move(update_channels));
  const auto& web_bundle_id = bundle_ref.web_bundle_id();

  factory_->AddResponse(web_bundle_url.spec(), bundle_ref.GetBundleData());

  RegenerateServedUpdateManifest(web_bundle_id);
}

void IwaTestServerConfigurator::RegenerateServedUpdateManifest(
    const web_package::SignedWebBundleId& web_bundle_id) {
  network::mojom::URLResponseHeadPtr head =
      network::CreateURLResponseHead(net::HttpStatusCode::HTTP_OK);
  head->mime_type = "application/json";
  network::URLLoaderCompletionStatus status;

  factory_->AddResponse(
      storage_.GetUpdateManifestUrl(web_bundle_id), std::move(head),
      *base::WriteJson(storage_.GetUpdateManifest(web_bundle_id)), status);
}

// static
base::Value::Dict IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::optional<UpdateChannel>& update_channel,
    const std::optional<base::Version>& pinned_version) {
  return test::CreateForceInstallIwaPolicyEntry(
      web_bundle_id,
      test::BundleVersionsStorage::GetUpdateManifestUrl(GURL(kServerBaseUrl),
                                                        web_bundle_id),
      update_channel, pinned_version);
}

}  // namespace web_app
