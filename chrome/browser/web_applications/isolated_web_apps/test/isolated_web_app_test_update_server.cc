// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"

#include <variant>

#include "base/json/json_writer.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/bundle_versions_storage.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace web_app {

namespace {

std::unique_ptr<net::test_server::HttpResponse> HttpNotFound() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_NOT_FOUND);
  return response;
}

}  // namespace

IsolatedWebAppTestUpdateServer::IsolatedWebAppTestUpdateServer() {
  iwa_server_.RegisterRequestHandler(base::BindRepeating(
      &IsolatedWebAppTestUpdateServer::HandleRequest, base::Unretained(this)));
  EXPECT_TRUE(iwa_server_.Start());
  storage_.SetBaseUrl(iwa_server_.base_url());
}

IsolatedWebAppTestUpdateServer::~IsolatedWebAppTestUpdateServer() {
  if (iwa_server_.Started()) {
    EXPECT_TRUE(iwa_server_.ShutdownAndWaitUntilComplete());
  }
}

GURL IsolatedWebAppTestUpdateServer::GetUpdateManifestUrl(
    const web_package::SignedWebBundleId& web_bundle_id) const {
  return storage_.GetUpdateManifestUrl(web_bundle_id);
}

base::Value::Dict IsolatedWebAppTestUpdateServer::CreateForceInstallPolicyEntry(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::optional<UpdateChannel>& update_channel,
    const std::optional<IwaVersion>& pinned_version,
    const bool allow_downgrades) const {
  return test::CreateForceInstallIwaPolicyEntry(
      web_bundle_id, GetUpdateManifestUrl(web_bundle_id), update_channel,
      pinned_version, allow_downgrades);
}

base::Value::Dict IsolatedWebAppTestUpdateServer::GetUpdateManifest(
    const web_package::SignedWebBundleId& web_bundle_id) const {
  return storage_.GetUpdateManifest(web_bundle_id);
}

void IsolatedWebAppTestUpdateServer::AddBundle(
    std::unique_ptr<BundledIsolatedWebApp> bundle,
    std::optional<std::vector<UpdateChannel>> update_channels) {
  storage_.AddBundle(std::move(bundle), std::move(update_channels));
}

void IsolatedWebAppTestUpdateServer::RemoveBundle(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IwaVersion& version) {
  storage_.RemoveBundle(web_bundle_id, version);
}

std::unique_ptr<net::test_server::HttpResponse>
IsolatedWebAppTestUpdateServer::HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto resource = storage_.GetResource(request.GetURL().GetPath());
  if (!resource) {
    return HttpNotFound();
  }

  return std::visit(
      absl::Overload{
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
