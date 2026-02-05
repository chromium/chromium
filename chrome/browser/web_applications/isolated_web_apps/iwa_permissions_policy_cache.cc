// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/iwa_permissions_policy_cache.h"

#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/url_loading/url_loader_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace web_app {

IwaPermissionsPolicyCache::Entry::Entry(
    std::string feature,
    std::vector<std::string> allowed_origins)
    : feature(std::move(feature)),
      allowed_origins(std::move(allowed_origins)) {}

IwaPermissionsPolicyCache::Entry::Entry(const Entry&) = default;
IwaPermissionsPolicyCache::Entry::Entry(Entry&&) = default;

IwaPermissionsPolicyCache::Entry& IwaPermissionsPolicyCache::Entry::operator=(
    const Entry&) = default;

IwaPermissionsPolicyCache::Entry::~Entry() = default;

namespace {

// Maximum size of the manifest file. 1MB.
constexpr int kMaxManifestSizeInBytes = 1024 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("isolated_web_app_throttle",
                                        R"(
      semantics {
        sender: "Isolated Web App Throttle"
        description:
          "Load manifest from an installed Signed Web Bundle to parse "
          "permissions policy."
        trigger:
          "Requests are sent as part of the navigation to an IWA."
        internal: {
          contacts {
            email: "iwa-team@google.com"
          }
        }
        user_data: {
          type: NONE
        }
        data: "None"
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2025-12-11"
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled by settings."
        policy_exception_justification:
          "This feature is required to deliver core user experiences and "
          "cannot be disabled by policy."
      }
    )");

std::optional<IwaPermissionsPolicyCache::CacheEntry> ParseManifest(
    const std::string& manifest_content) {
  // Yes, this parses untrusted data in the browser process. But it's Rust JSON
  // parser so it's alright.
  std::optional<base::Value> json_value =
      base::JSONReader::Read(manifest_content, 0);
  if (!json_value || !json_value->is_dict()) {
    return std::nullopt;
  }

  const base::DictValue* manifest_dict = json_value->GetIfDict();
  const base::Value* permissions_policy_value =
      manifest_dict->Find("permissions_policy");

  if (!permissions_policy_value) {
    return IwaPermissionsPolicyCache::CacheEntry{};
  }

  if (!permissions_policy_value->is_dict()) {
    return std::nullopt;
  }

  const base::DictValue& permissions_policy_dict =
      permissions_policy_value->GetDict();

  std::vector<IwaPermissionsPolicyCache::Entry> permissions_policy;
  for (const auto [key, val] : permissions_policy_dict) {
    if (!val.is_list()) {
      return std::nullopt;
    }
    const base::ListValue& list = val.GetList();
    std::vector<std::string> allowed_origins;
    for (const auto& item : list) {
      if (!item.is_string()) {
        return std::nullopt;
      }
      // We expect 4 types of origin strings:
      // - "self": wrapped in single quotes (as in a header)
      // - "none": wrapped in single quotes (as in a header)
      // - "*" (asterisk): not wrapped
      // - "<origin>": actual origin names should not be wrapped in single
      //        quotes
      // The "src" origin string type can be ignored here as it's only used in
      // the iframe "allow" attribute.
      //
      // Sidenote: Actual origin names ("<origin>") are parsed using
      // OriginWithPossibleWildcards::Parse() which fails if the origin string
      // contains any non-alphanumeric characters, such as a single quote. For
      // this reason, actual origin names must not be wrapped since the parser
      // will just drop them as being improperly formatted (i.e. they would be
      // the equivalent to some manifest containing an origin wrapped in single
      // quotes, which is invalid).
      auto allowlist_item = item.GetString();
      if (base::EqualsCaseInsensitiveASCII(allowlist_item, "none") ||
          base::EqualsCaseInsensitiveASCII(allowlist_item, "self")) {
        allowlist_item = "'" + allowlist_item + "'";
      }
      allowed_origins.push_back(allowlist_item);
    }
    permissions_policy.emplace_back(key, std::move(allowed_origins));
  }
  return permissions_policy;
}

}  // namespace

IwaPermissionsPolicyCache::IwaPermissionsPolicyCache(WebAppProvider& provider)
    : provider_(&provider) {
  // WebAppInstallManager is initialized within
  // WebAppProviderFactory::BuildServiceInstanceForBrowserContext and this
  // service depends on WebAppProvider so this is valid.
  install_manager_observation_.Observe(&provider_->install_manager());
}

IwaPermissionsPolicyCache::~IwaPermissionsPolicyCache() = default;

const IwaPermissionsPolicyCache::CacheEntry*
IwaPermissionsPolicyCache::GetPolicy(const IwaOrigin& iwa_origin) const {
  return base::FindOrNull(cache_, iwa_origin);
}

void IwaPermissionsPolicyCache::ObtainManifestAndCache(
    const IwaOrigin& iwa_origin,
    base::OnceCallback<void(bool success)> callback) {
  if (GetPolicy(iwa_origin)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }

  // We don't want to do the caching if navigating to a nonexistent IWA.
  const WebApp* iwa = provider_->registrar_unsafe().GetAppById(
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          iwa_origin.web_bundle_id())
          .app_id(),
      WebAppFilter::IsIsolatedApp());
  if (!iwa) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }

  // If the IWA is not trusted, we skip caching the manifest. The main
  // navigation will handle the trust failure and show an appropriate error
  // page. Fetching the manifest here would result in an opaque network
  // error.
  RETURN_IF_ERROR(IsolatedWebAppTrustChecker::IsResourceLoadingAllowed(
                      *provider_->profile(), iwa_origin.web_bundle_id(), *iwa),
                  [&](const auto&) {
                    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                        FROM_HERE, base::BindOnce(std::move(callback), true));
                    return;
                  });

  // Policy not cached, fetch the manifest.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = iwa_origin.origin().GetURL().Resolve(
      R"(/.well-known/manifest.webmanifest)");
  auto manifest_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);

  auto loader_factory =
      std::make_unique<mojo::Remote<network::mojom::URLLoaderFactory>>();
  loader_factory->Bind(web_app::IsolatedWebAppURLLoaderFactory::Create(
      provider_->profile(), iwa_origin.origin()));

  network::SimpleURLLoader* raw_loader = manifest_loader.get();
  auto* raw_loader_factory = loader_factory->get();
  raw_loader->DownloadToString(
      raw_loader_factory,
      base::BindOnce(&IwaPermissionsPolicyCache::OnManifestLoaded,
                     weak_ptr_factory_.GetWeakPtr(), iwa_origin,
                     std::move(callback))
          .Then(base::OnceClosure(base::DoNothingWithBoundArgs(
              std::move(manifest_loader), std::move(loader_factory)))),
      kMaxManifestSizeInBytes);
}

void IwaPermissionsPolicyCache::OnManifestLoaded(
    const IwaOrigin& iwa_origin,
    base::OnceCallback<void(bool success)> callback,
    std::optional<std::string> manifest_content) {
  std::move(callback).Run(
      !!manifest_content &&
      ParseManifestAndSetPolicy(iwa_origin, *manifest_content));
}

void IwaPermissionsPolicyCache::SetPolicy(
    const IwaOrigin& iwa_origin,
    IwaPermissionsPolicyCache::CacheEntry policy) {
  cache_[iwa_origin] = std::move(policy);
}

bool IwaPermissionsPolicyCache::ParseManifestAndSetPolicy(
    const IwaOrigin& iwa_origin,
    const std::string& manifest_content) {
  std::optional<CacheEntry> policy = ParseManifest(manifest_content);
  if (!policy) {
    return false;
  }
  SetPolicy(iwa_origin, std::move(*policy));
  return true;
}

void IwaPermissionsPolicyCache::ClearPolicy(const IwaOrigin& iwa_origin) {
  cache_.erase(iwa_origin);
}

void IwaPermissionsPolicyCache::Shutdown() {
  install_manager_observation_.Reset();
  provider_ = nullptr;
}

void IwaPermissionsPolicyCache::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  ClearCacheForApp(app_id);
}

void IwaPermissionsPolicyCache::OnWebAppManifestUpdated(
    const webapps::AppId& app_id) {
  ClearCacheForApp(app_id);
}

void IwaPermissionsPolicyCache::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void IwaPermissionsPolicyCache::ClearCacheForApp(const webapps::AppId& app_id) {
  if (!provider_) {
    return;
  }
  const WebApp* iwa = provider_->registrar_unsafe().GetAppById(
      app_id, WebAppFilter::IsIsolatedApp());
  if (!iwa) {
    return;
  }
  auto iwa_origin = IwaOrigin::Create(iwa->start_url());
  CHECK(iwa_origin.has_value());
  ClearPolicy(*iwa_origin);
}

// static
IwaPermissionsPolicyCache* IwaPermissionsPolicyCacheFactory::GetForProfile(
    Profile* profile) {
  return static_cast<IwaPermissionsPolicyCache*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
IwaPermissionsPolicyCacheFactory*
IwaPermissionsPolicyCacheFactory::GetInstance() {
  static base::NoDestructor<IwaPermissionsPolicyCacheFactory> instance;
  return instance.get();
}

IwaPermissionsPolicyCacheFactory::IwaPermissionsPolicyCacheFactory()
    : IsolatedWebAppBrowserContextServiceFactory("IwaPermissionsPolicyCache") {
  DependsOn(WebAppProviderFactory::GetInstance());
}

IwaPermissionsPolicyCacheFactory::~IwaPermissionsPolicyCacheFactory() = default;

std::unique_ptr<KeyedService>
IwaPermissionsPolicyCacheFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* provider = WebAppProvider::GetForWebApps(profile);
  CHECK(provider);
  return std::make_unique<IwaPermissionsPolicyCache>(*provider);
}

}  // namespace web_app
