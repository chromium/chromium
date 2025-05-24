// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"

#include <variant>

#include "base/functional/overloaded.h"
#include "base/json/json_writer.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/bundle_versions_storage.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "components/webapps/isolated_web_apps/update_channel.h"

namespace web_app {

namespace {

std::unique_ptr<net::test_server::HttpResponse> HttpNotFound() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_NOT_FOUND);
  return response;
}

}  // namespace

IsolatedWebAppUpdateServerMixin::IsolatedWebAppUpdateServerMixin(
    InProcessBrowserTestMixinHost* mixin_host)
    : InProcessBrowserTestMixin(mixin_host) {
  iwa_server_.RegisterRequestHandler(base::BindRepeating(
      &IsolatedWebAppUpdateServerMixin::HandleRequest, base::Unretained(this)));
  EXPECT_TRUE(iwa_server_.Start());
  storage_.SetBaseUrl(iwa_server_.base_url());
}

IsolatedWebAppUpdateServerMixin::~IsolatedWebAppUpdateServerMixin() = default;

GURL IsolatedWebAppUpdateServerMixin::GetUpdateManifestUrl(
    const web_package::SignedWebBundleId& web_bundle_id) const {
  return storage_.GetUpdateManifestUrl(web_bundle_id);
}

base::Value::Dict
IsolatedWebAppUpdateServerMixin::CreateForceInstallPolicyEntry(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::optional<UpdateChannel>& update_channel,
    const std::optional<base::Version>& pinned_version,
    const bool allow_downgrades) const {
  return test::CreateForceInstallIwaPolicyEntry(
      web_bundle_id, GetUpdateManifestUrl(web_bundle_id), update_channel,
      pinned_version, allow_downgrades);
}

base::Value::Dict IsolatedWebAppUpdateServerMixin::GetUpdateManifest(
    const web_package::SignedWebBundleId& web_bundle_id) const {
  return storage_.GetUpdateManifest(web_bundle_id);
}

void IsolatedWebAppUpdateServerMixin::AddBundle(
    std::unique_ptr<BundledIsolatedWebApp> bundle,
    std::optional<std::vector<UpdateChannel>> update_channels) {
  storage_.AddBundle(std::move(bundle), std::move(update_channels));
}

void IsolatedWebAppUpdateServerMixin::RemoveBundle(
    const web_package::SignedWebBundleId& web_bundle_id,
    const base::Version& version) {
  storage_.RemoveBundle(web_bundle_id, version);
}

std::unique_ptr<net::test_server::HttpResponse>
IsolatedWebAppUpdateServerMixin::HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto resource = storage_.GetResource(request.GetURL().path());
  if (!resource) {
    return HttpNotFound();
  }

  return std::visit(
      base::Overloaded{
          [](BundledIsolatedWebApp* bundle)
              -> std::unique_ptr<net::test_server::HttpResponse> {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type("application/octet-stream");
            response->set_content(bundle->GetBundleData());
            return response;
          },
          [](base::Value::Dict& update_manifest)
              -> std::unique_ptr<net::test_server::HttpResponse> {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type("application/json");
            response->set_content(*base::WriteJson(update_manifest));
            return response;
          }},
      *resource);
}

}  // namespace web_app
