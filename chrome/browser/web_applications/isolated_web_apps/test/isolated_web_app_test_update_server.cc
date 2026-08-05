// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"

#include <variant>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/bundle_versions_storage.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/common/chrome_switches.h"
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

std::string GetBaseTestName() {
  const auto* test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  if (!test_info) {
    return "";
  }
  std::string_view name = test_info->name();
  while (name.starts_with("PRE_")) {
    name.remove_prefix(4);
  }
  size_t slash_pos = name.find('/');
  if (slash_pos != std::string_view::npos) {
    name = name.substr(0, slash_pos);
  }
  return std::string(name);
}

// Returns the file path used to persist the test update server's HTTP port across
// test restarts (e.g., across PRE_ steps in multi-stage browser tests).
base::FilePath GetPortFilePath() {
  base::FilePath user_data_dir =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kUserDataDir);
  if (user_data_dir.empty()) {
    return base::FilePath();
  }
  std::string test_name = GetBaseTestName();
  std::string filename =
      test_name.empty() ? "iwa_test_update_server_port.txt"
                        : "iwa_test_update_server_port_" + test_name + ".txt";
  return user_data_dir.AppendASCII(filename);
}

// Reads the saved port from `port_file` if it exists and contains a valid port number,
// otherwise returns 0 (allowing EmbeddedTestServer to pick an available port).
int ReadSavedPort(const base::FilePath& port_file) {
  if (port_file.empty()) {
    return 0;
  }
  std::string port_str;
  int port = 0;
  if (base::ReadFileToString(port_file, &port_str) &&
      base::StringToInt(base::TrimWhitespaceASCII(port_str, base::TRIM_ALL),
                        &port) &&
      port > 0) {
    return port;
  }
  return 0;
}

}  // namespace

IsolatedWebAppTestUpdateServer::IsolatedWebAppTestUpdateServer(
    bool reuse_port_across_restarts) {
  iwa_server_.RegisterRequestHandler(base::BindRepeating(
      &IsolatedWebAppTestUpdateServer::HandleRequest, base::Unretained(this)));

  base::FilePath port_file =
      reuse_port_across_restarts ? GetPortFilePath() : base::FilePath();
  int port_to_use = ReadSavedPort(port_file);

  // Try starting the server on the saved port first. If binding fails or no port
  // was saved, fall back to binding an ephemeral port (0).
  bool started = iwa_server_.Start(port_to_use);
  if (!started && port_to_use != 0) {
    started = iwa_server_.Start(0);
  }
  EXPECT_TRUE(started);

  if (reuse_port_across_restarts && !port_file.empty()) {
    base::WriteFile(port_file, base::NumberToString(iwa_server_.port()));
  }

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

base::DictValue IsolatedWebAppTestUpdateServer::CreateForceInstallPolicyEntry(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::optional<UpdateChannel>& update_channel,
    const std::optional<IwaVersion>& pinned_version,
    const bool allow_downgrades) const {
  return test::CreateForceInstallIwaPolicyEntry(
      web_bundle_id, GetUpdateManifestUrl(web_bundle_id), update_channel,
      pinned_version, allow_downgrades);
}

base::DictValue IsolatedWebAppTestUpdateServer::GetUpdateManifest(
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
          [](base::DictValue& update_manifest)
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
