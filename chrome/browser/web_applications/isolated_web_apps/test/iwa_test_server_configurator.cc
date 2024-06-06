// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/iwa_test_server_configurator.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"

namespace {

// TODO(peletskyi): Read manifest from the signed web bundle file.
blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& application_url) {
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = application_url.DeprecatedGetOriginAsURL();
  manifest->scope = application_url.Resolve("/");
  manifest->start_url = application_url.Resolve("/testing-start-url.html");
  manifest->display = web_app::DisplayMode::kStandalone;
  manifest->short_name = u"App name";
  manifest->version = u"1.0.0";

  return manifest;
}

void ConfigureFakeWebContentsManager(
    const web_package::SignedWebBundleId& id,
    web_app::FakeWebContentsManager& fake_web_contents_manager) {
  const GURL origin_url =
      web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(id)
          .origin()
          .GetURL();

  const GURL install_url =
      origin_url.Resolve("/.well-known/_generated_install_page.html");

  auto& page_state =
      fake_web_contents_manager.GetOrCreatePageState(install_url);
  page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
  page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
  page_state.manifest_url = origin_url.Resolve("manifest.webmanifest");
  page_state.valid_manifest_for_web_app = true;
  page_state.manifest_before_default_processing =
      CreateDefaultManifest(origin_url);
}
}  // namespace

namespace web_app {

IwaTestServerConfigurator::IwaTestServerConfigurator() = default;
IwaTestServerConfigurator::~IwaTestServerConfigurator() = default;

void IwaTestServerConfigurator::AddUpdateManifest(std::string relative_url,
                                                  std::string manifest_value) {
  served_update_manifests_.push_back(
      {.relative_url_ = std::move(relative_url),
       .manifest_value_ = std::move(manifest_value)});
}

void IwaTestServerConfigurator::AddSignedWebBundle(
    std::string relative_url,
    web_app::TestSignedWebBundle web_bundle) {
  served_signed_web_bundles_.push_back(
      {.relative_url_ = std::move(relative_url),
       .web_bundle_ = std::move(web_bundle)});
}

void IwaTestServerConfigurator::ConfigureURLLoader(
    const GURL& base_url,
    network::TestURLLoaderFactory& test_factory,
    FakeWebContentsManager& fake_web_contents_manager) {
  for (const auto& served_update_manifest : served_update_manifests_) {
    const GURL url = base_url.Resolve(served_update_manifest.relative_url_);

    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HttpStatusCode::HTTP_OK);
    head->mime_type = "application/json";
    const network::URLLoaderCompletionStatus status;
    test_factory.AddResponse(url, std::move(head),
                             served_update_manifest.manifest_value_, status);
  }

  for (const auto& served_web_bundle : served_signed_web_bundles_) {
    const GURL url = base_url.Resolve(served_web_bundle.relative_url_);
    const std::string web_bundle_str(served_web_bundle.web_bundle_.data.begin(),
                                     served_web_bundle.web_bundle_.data.end());
    test_factory.AddResponse(url.spec(), web_bundle_str);

    ConfigureFakeWebContentsManager(served_web_bundle.web_bundle_.id,
                                    fake_web_contents_manager);
  }
}

}  // namespace web_app
