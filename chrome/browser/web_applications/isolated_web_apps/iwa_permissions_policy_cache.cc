// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/iwa_permissions_policy_cache.h"

#include "base/containers/map_util.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"

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

std::optional<IwaPermissionsPolicyCache::CacheEntry> ParseManifest(
    const std::string& manifest_content) {
  // Yes, this parses untrusted data in the browser process. But it's Rust JSON
  // parser so it's alright.
  std::optional<base::Value> json_value =
      base::JSONReader::Read(manifest_content, 0);
  if (!json_value || !json_value->is_dict()) {
    return std::nullopt;
  }

  const base::Value::Dict* manifest_dict = json_value->GetIfDict();
  const base::Value* permissions_policy_value =
      manifest_dict->Find("permissions_policy");

  if (!permissions_policy_value) {
    return IwaPermissionsPolicyCache::CacheEntry{};
  }

  if (!permissions_policy_value->is_dict()) {
    return std::nullopt;
  }

  const base::Value::Dict& permissions_policy_dict =
      permissions_policy_value->GetDict();

  std::vector<IwaPermissionsPolicyCache::Entry> permissions_policy;
  for (const auto [key, val] : permissions_policy_dict) {
    if (!val.is_list()) {
      return std::nullopt;
    }
    const base::Value::List& list = val.GetList();
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
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app || !web_app->isolation_data()) {
    return;
  }
  auto iwa_origin = IwaOrigin::Create(web_app->start_url());
  // We checked that isolation_data exists, this has to be an IWA.
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
