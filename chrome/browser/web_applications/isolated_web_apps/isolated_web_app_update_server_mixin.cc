// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/containers/to_value_list.h"
#include "base/json/json_writer.h"
#include "base/strings/string_split.h"
#include "base/types/expected_macros.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "net/http/http_status_code.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#endif

namespace web_app {

BundleInfo::BundleInfo() = default;
BundleInfo::~BundleInfo() = default;
BundleInfo::BundleInfo(BundleInfo&& other) = default;

BundleInfo& BundleInfo::operator=(BundleInfo&& other) = default;
BundleInfo::BundleInfo(
    std::unique_ptr<BundledIsolatedWebApp> bundled_app,
    std::optional<std::vector<UpdateChannel>> update_channels)
    : bundle(std::move(bundled_app)),
      update_channels(std::move(update_channels)) {}

namespace {
constexpr std::string_view kUpdateManifestFileName = "update_manifest.json";

std::unique_ptr<net::test_server::HttpResponse> HttpNotFound() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_NOT_FOUND);
  return response;
}

}  // namespace

IsolatedWebAppUpdateServerMixin::IsolatedWebAppUpdateServerMixin(
    InProcessBrowserTestMixinHost* mixin_host)
    : InProcessBrowserTestMixin(mixin_host) {}

IsolatedWebAppUpdateServerMixin::~IsolatedWebAppUpdateServerMixin() = default;

void IsolatedWebAppUpdateServerMixin::SetUpOnMainThread() {
  iwa_server_.RegisterRequestHandler(base::BindRepeating(
      &IsolatedWebAppUpdateServerMixin::HandleRequest, base::Unretained(this)));
  EXPECT_TRUE(iwa_server_.Start());
}

GURL IsolatedWebAppUpdateServerMixin::GetUpdateManifestUrl(
    const web_package::SignedWebBundleId& web_bundle_id) const {
  return iwa_server_.GetURL(
      base::StrCat({"/", web_bundle_id.id(), "/", kUpdateManifestFileName}));
}

#if BUILDFLAG(IS_CHROMEOS)
base::Value::Dict
IsolatedWebAppUpdateServerMixin::CreateForceInstallPolicyEntry(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::optional<UpdateChannel>& update_channel,
    const std::optional<base::Version>& pinned_version) const {
  base::Value::Dict policy_entry =
      base::Value::Dict()
          .Set(kPolicyWebBundleIdKey, web_bundle_id.id())
          .Set(kPolicyUpdateManifestUrlKey,
               GetUpdateManifestUrl(web_bundle_id).spec());

  if (update_channel) {
    policy_entry.Set(kPolicyUpdateChannelKey, update_channel->ToString());
  }

  if (pinned_version) {
    policy_entry.Set(kPolicyPinnedVersionKey,
                     pinned_version.value().GetString());
  }

  return policy_entry;
}
#endif

void IsolatedWebAppUpdateServerMixin::AddBundle(
    std::unique_ptr<BundledIsolatedWebApp> bundle,
    std::optional<std::vector<UpdateChannel>> update_channels) {
  auto* bundle_ptr = bundle.get();
  bundle_versions_per_id_[bundle_ptr->web_bundle_id()][bundle_ptr->version()] =
      BundleInfo(std::move(bundle), update_channels);
}

void IsolatedWebAppUpdateServerMixin::RemoveBundle(
    const web_package::SignedWebBundleId& web_bundle_id,
    const base::Version& version) {
  CHECK(base::Contains(bundle_versions_per_id_, web_bundle_id));
  auto& bundle_versions = bundle_versions_per_id_[web_bundle_id];
  CHECK(base::Contains(bundle_versions, version));
  bundle_versions.erase(version);
  if (bundle_versions.empty()) {
    bundle_versions_per_id_.erase(web_bundle_id);
  }
}

std::unique_ptr<net::test_server::HttpResponse>
IsolatedWebAppUpdateServerMixin::HandleRequest(
    const net::test_server::HttpRequest& request) {
  // Parses /<web_bundle_id>/<file_name> into { <web_bundle_id>, <file_name> }.
  auto pieces =
      base::SplitString(request.GetURL().path(), "/", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (pieces.size() != 2) {
    return HttpNotFound();
  }

  ASSIGN_OR_RETURN(auto web_bundle_id,
                   web_package::SignedWebBundleId::Create(pieces[0]),
                   [](auto) { return HttpNotFound(); });
  const auto* bundle_versions =
      base::FindOrNull(bundle_versions_per_id_, web_bundle_id);
  if (!bundle_versions) {
    return HttpNotFound();
  }

  const auto& path = pieces[1];
  if (path == kUpdateManifestFileName) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("application/json");
    auto update_manifest = base::Value::Dict().Set(
        "versions",
        base::ToValueList(*bundle_versions, [&](const auto& bundle_meta) {
          const auto& [version, bundle_info] = bundle_meta;

          auto relative_bundle_path = base::StrCat(
              {"/", web_bundle_id.id(), "/", version.GetString(), ".swbn"});
          auto dict =
              base::Value::Dict()
                  .Set("version", version.GetString())
                  .Set("src", iwa_server_.GetURL(relative_bundle_path).spec());

          if (bundle_info.update_channels) {
            dict.Set("channels",
                     base::ToValueList(bundle_info.update_channels.value(),
                                       &UpdateChannel::ToString));
          }
          return dict;
        }));

    response->set_content(*base::WriteJson(update_manifest));
    return response;
  } else if (path.ends_with(".swbn")) {
    base::Version version(path.substr(0, path.size() - 5));
    if (version.IsValid()) {
      if (auto* bundle_info = base::FindOrNull(*bundle_versions, version)) {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content_type("application/octet-stream");
        response->set_content(bundle_info->bundle->GetBundleData());
        return response;
      }
    }
  }

  return HttpNotFound();
}

}  // namespace web_app
