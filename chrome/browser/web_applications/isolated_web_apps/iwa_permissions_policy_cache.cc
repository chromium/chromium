// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/iwa_permissions_policy_cache.h"

#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/iwa_entitlements.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/url_loading/url_loader_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_features_generated.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"

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

IwaPermissionsPolicyCache::Policy::Policy(
    const std::vector<Entry>& unfiltered,
    const std::vector<Entry>& filtered,
    std::optional<IwaVersion> app_version_for_filtering,
    std::vector<ManifestWarning> manifest_warnings)
    : unfiltered(unfiltered),
      filtered(filtered),
      app_version_for_filtering(std::move(app_version_for_filtering)),
      manifest_warnings(std::move(manifest_warnings)) {}
IwaPermissionsPolicyCache::Policy::Policy(const Policy&) = default;
IwaPermissionsPolicyCache::Policy::Policy(Policy&&) = default;
IwaPermissionsPolicyCache::Policy& IwaPermissionsPolicyCache::Policy::operator=(
    const Policy&) = default;
IwaPermissionsPolicyCache::Policy::~Policy() = default;

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

std::string GetPermissionsPolicyFeatureName(
    network::mojom::PermissionsPolicyFeature feature) {
  const auto& map = blink::GetPermissionsPolicyFeatureToNameMap();
  auto it = map.find(feature);
  return it != map.end() ? std::string(it->second) : std::string();
}

std::optional<std::vector<std::string>> GetAllowedOrigins(
    const IwaPermissionsPolicyCache::CacheEntry& manifest,
    network::mojom::PermissionsPolicyFeature feature) {
  auto it = std::ranges::find_if(manifest, [&](const auto& entry) {
    return entry.feature == GetPermissionsPolicyFeatureName(feature);
  });

  if (it == manifest.end()) {
    return std::nullopt;
  }

  return it->allowed_origins;
}

void AddDirectSocketsPrivatePermissionPolicyWarningMessage(
    std::vector<IwaPermissionsPolicyCache::ManifestWarning>& manifest_warnings,
    network::mojom::PermissionsPolicyFeature feature) {
  std::string feature_name = GetPermissionsPolicyFeatureName(feature);

  manifest_warnings.emplace_back(
      blink::mojom::ConsoleMessageSource::kDeprecation,
      base::StrCat({"The 'permissions_policy' field in the manifest includes "
                    "'direct-sockets-private' but is missing the required '",
                    feature_name,
                    "' policy. While Chrome is automatically including '",
                    feature_name,
                    "' to maintain backward compatibility, this behavior is "
                    "deprecated and will be removed in Chrome 151. Please "
                    "update your manifest to include '",
                    feature_name, "' explicitly."}));
}

std::optional<IwaPermissionsPolicyCache::CacheEntry> ParseManifest(
    const std::string& manifest_content,
    std::vector<IwaPermissionsPolicyCache::ManifestWarning>&
        manifest_warnings) {
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

  const auto& name_to_feature_map =
      blink::GetPermissionsPolicyNameToFeatureMap();

  std::vector<IwaPermissionsPolicyCache::Entry> permissions_policy;
  for (const auto [key, val] : permissions_policy_dict) {
    if (!name_to_feature_map.contains(key)) {
      manifest_warnings.push_back(
          {blink::mojom::ConsoleMessageSource::kOther,
           base::StrCat({"The 'permissions_policy' field in the manifest "
                         "includes an unknown feature: '",
                         key, "'."})});
    }

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

  // TODO(b/492476083): Remove backward compatibility code by Chrome Milestone
  // 151. Now "direct-sockets-private" requires "loopback-network" and
  // "local-network" to function. So "direct-sockets-private" is temporarily
  // unpacked to "local-network" and "loopback-network" for backwards
  // compatibility to existing apps. A DevTools warning is shown in the console.
  // This behavior will be removed in Chrome Milestone 151.
  auto direct_sockets_private_origins = GetAllowedOrigins(
      permissions_policy,
      network::mojom::PermissionsPolicyFeature::kDirectSocketsPrivate);
  if (direct_sockets_private_origins) {
    if (!GetAllowedOrigins(
            permissions_policy,
            network::mojom::PermissionsPolicyFeature::kLocalNetwork)) {
      permissions_policy.emplace_back(
          std::string(GetPermissionsPolicyFeatureName(
              network::mojom::PermissionsPolicyFeature::kLocalNetwork)),
          *direct_sockets_private_origins);
      AddDirectSocketsPrivatePermissionPolicyWarningMessage(
          manifest_warnings,
          network::mojom::PermissionsPolicyFeature::kLocalNetwork);
    }
    if (!GetAllowedOrigins(
            permissions_policy,
            network::mojom::PermissionsPolicyFeature::kLoopbackNetwork)) {
      permissions_policy.emplace_back(
          std::string(GetPermissionsPolicyFeatureName(
              network::mojom::PermissionsPolicyFeature::kLoopbackNetwork)),
          *direct_sockets_private_origins);
      AddDirectSocketsPrivatePermissionPolicyWarningMessage(
          manifest_warnings,
          network::mojom::PermissionsPolicyFeature::kLoopbackNetwork);
    }
  }
  return permissions_policy;
}

using IwaRuntimeAllowlistData =
    ChromeIwaRuntimeDataProvider::UserInstallAllowlistItemData;

bool IsEntitlementGranted(const IwaRuntimeAllowlistData& allowlist_data,
                          const IwaVersion& app_version,
                          IwaEntitlement entitlement) {
  for (const auto& entitlement_set : allowlist_data.entitlements) {
    if (!std::ranges::contains(entitlement_set.entitlements, entitlement)) {
      continue;
    }

    if (!entitlement_set.version_range.begin().empty()) {
      auto min_version =
          IwaVersion::Create(entitlement_set.version_range.begin());
      if (!min_version.has_value() || app_version < *min_version) {
        continue;
      }
    }

    if (!entitlement_set.version_range.end().empty()) {
      auto max_version =
          IwaVersion::Create(entitlement_set.version_range.end());
      if (!max_version.has_value() || app_version >= *max_version) {
        continue;
      }
    }

    return true;
  }

  return false;
}

// Applies entitlements to the Permissions Policy of an Isolated Web App.
//
// Reasoning:
// Powerful APIs (typically guarded by `IsolatedContext`, like Direct Sockets)
// present a significant security and privacy risk. For user-installed IWAs,
// access to these APIs is restricted by an allowlist (entitlements) distributed
// via the Component Updater. Managed installs (via enterprise policy) and Dev
// Mode installs bypass these checks as they are implicitly trusted by the
// administrator or developer.
//
// Algorithm:
// 1. Retrieve the allowed entitlements from the `ChromeIwaRuntimeDataProvider`
//    based on the Web Bundle ID.
// 2. Iterate through each permissions policy feature requested by the app:
//    a. Check if the feature maps to a specific IWA entitlement.
//    b. If there is NO mapping:
//       - Allow it if it's a standard web feature (not `IsolatedContext`).
//       - Block it if it IS an `IsolatedContext` feature. This is intentional
//         for features that should never be available to user-installed IWAs
//         (e.g., all-screens-capture).
//    c. If there IS a mapping:
//       - Check if the retrieved allowlist data grants this entitlement and
//         if the app's version falls within the granted version range.
// 3. Finally, grant the feature if the previous checks passed.
std::vector<IwaPermissionsPolicyCache::Entry> ApplyEntitlements(
    const std::vector<IwaPermissionsPolicyCache::Entry>& policy,
    const IwaVersion& app_version,
    const web_package::SignedWebBundleId& web_bundle_id) {
  std::vector<IwaPermissionsPolicyCache::Entry> result;

  const IwaRuntimeAllowlistData* allowlist_data =
      ChromeIwaRuntimeDataProvider::GetInstance().GetUserInstallAllowlistData(
          web_bundle_id.id());

  const auto is_feature_allowed = [&allowlist_data,
                                   &app_version](const std::string& feature) {
    auto entitlement = GetEntitlementForFeature(feature);
    if (!entitlement) {
      return !network::IsPermissionsPolicyFeatureGuardedByIsolatedContext(
          feature);
    }
    return allowlist_data &&
           IsEntitlementGranted(*allowlist_data, app_version, *entitlement);
  };

  for (const auto& entry : policy) {
    if (is_feature_allowed(entry.feature)) {
      result.push_back(entry);
    }
  }

  return result;
}

}  // namespace

IwaPermissionsPolicyCache::IwaPermissionsPolicyCache(WebAppProvider& provider)
    : provider_(&provider) {
  // WebAppInstallManager is initialized within
  // WebAppProviderFactory::BuildServiceInstanceForBrowserContext and this
  // service depends on WebAppProvider so this is valid.
  install_manager_observation_.Observe(&provider_->install_manager());

  runtime_data_subscription_ =
      ChromeIwaRuntimeDataProvider::GetInstance().OnRuntimeDataChanged(
          base::BindRepeating(
              &IwaPermissionsPolicyCache::UpdateFilteredPolicies,
              weak_ptr_factory_.GetWeakPtr()));
}

IwaPermissionsPolicyCache::~IwaPermissionsPolicyCache() = default;

const IwaPermissionsPolicyCache::CacheEntry*
IwaPermissionsPolicyCache::GetPolicy(const IwaOrigin& iwa_origin) const {
  const Policy* policy = base::FindOrNull(cache_, iwa_origin);
  return policy ? &policy->filtered : nullptr;
}

std::vector<content::ConsoleMessage>
IwaPermissionsPolicyCache::GetViolationWarningMessages(
    const IwaOrigin& iwa_origin) const {
  const Policy* policy = base::FindOrNull(cache_, iwa_origin);
  if (!policy) {
    return {};
  }
  const auto filtered_set = base::MakeFlatSet<std::string>(
      policy->filtered, std::less(),
      [](const auto& entry) { return entry.feature; });

  const GURL& url = iwa_origin.origin().GetURL();
  std::u16string url_spec = base::UTF8ToUTF16(url.spec());

  std::vector<content::ConsoleMessage> messages;
  for (const auto& entry : policy->unfiltered) {
    if (!filtered_set.contains(entry.feature)) {
      std::u16string message_text =
          base::StrCat({u"IWA entitlement violation: feature '",
                        base::UTF8ToUTF16(entry.feature),
                        u"' is not granted to ", url_spec, u"."});

      messages.emplace_back(blink::mojom::ConsoleMessageSource::kViolation,
                            blink::mojom::ConsoleMessageLevel::kWarning,
                            message_text,
                            /*line_number=*/0, url);
    }
  }

  return messages;
}

std::vector<content::ConsoleMessage>
IwaPermissionsPolicyCache::GetManifestWarningMessages(
    const IwaOrigin& iwa_origin) const {
  const Policy* policy = base::FindOrNull(cache_, iwa_origin);
  if (!policy || policy->manifest_warnings.empty()) {
    return {};
  }

  const GURL& url = iwa_origin.origin().GetURL();
  std::vector<content::ConsoleMessage> messages;
  messages.reserve(policy->manifest_warnings.size());

  for (const auto& warning : policy->manifest_warnings) {
    messages.emplace_back(warning.source,
                          blink::mojom::ConsoleMessageLevel::kWarning,
                          base::UTF8ToUTF16(warning.message),
                          /*line_number=*/0, url);
  }

  return messages;
}

std::vector<content::ConsoleMessage>
IwaPermissionsPolicyCache::GetWarningMessages(
    const IwaOrigin& iwa_origin) const {
  std::vector<content::ConsoleMessage> messages =
      GetManifestWarningMessages(iwa_origin);
  std::vector<content::ConsoleMessage> violation_messages =
      GetViolationWarningMessages(iwa_origin);

  messages.reserve(messages.size() + violation_messages.size());

  std::move(violation_messages.begin(), violation_messages.end(),
            std::back_inserter(messages));

  return messages;
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
      provider_->profile(), iwa_origin.origin(),
      /*enforce_same_origin=*/true));

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

void IwaPermissionsPolicyCache::SetPolicyForTesting(const IwaOrigin& iwa_origin,
                                                    CacheEntry policy) {
  cache_[iwa_origin] =
      Policy{/*unfiltered=*/policy, /*filtered=*/std::move(policy)};
}

void IwaPermissionsPolicyCache::SetPolicyWithViolationsForTesting(
    const IwaOrigin& iwa_origin,
    CacheEntry unfiltered_policy,
    CacheEntry filtered_policy) {
  cache_[iwa_origin] = Policy{/*unfiltered=*/std::move(unfiltered_policy),
                              /*filtered=*/std::move(filtered_policy)};
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
    CacheEntry unfiltered_policy,
    std::vector<ManifestWarning> manifest_warnings) {
  CacheEntry filtered_policy = unfiltered_policy;
  UpdateFilteredPolicy(
      iwa_origin,
      cache_[iwa_origin] = Policy{/*unfiltered=*/std::move(unfiltered_policy),
                                  /*filtered=*/std::move(filtered_policy),
                                  std::nullopt, std::move(manifest_warnings)});
}

bool IwaPermissionsPolicyCache::ParseManifestAndSetPolicy(
    const IwaOrigin& iwa_origin,
    const std::string& manifest_content) {
  std::vector<ManifestWarning> manifest_warnings;
  std::optional<CacheEntry> policy =
      ParseManifest(manifest_content, manifest_warnings);
  if (!policy) {
    return false;
  }
  SetPolicy(iwa_origin, std::move(*policy), std::move(manifest_warnings));
  return true;
}

void IwaPermissionsPolicyCache::ClearPolicy(const IwaOrigin& iwa_origin) {
  cache_.erase(iwa_origin);
}

void IwaPermissionsPolicyCache::UpdateFilteredPolicy(const IwaOrigin& origin,
                                                     Policy& policy) {
  if (!provider_) {
    return;
  }

  if (!policy.app_version_for_filtering) {
    auto app_id = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                      origin.web_bundle_id())
                      .app_id();
    const WebAppRegistrar& registrar = provider_->registrar_unsafe();
    const WebApp* iwa =
        registrar.GetAppById(app_id, WebAppFilter::IsIsolatedApp());
    if (!iwa) {
      return;
    }

    if (registrar.AppMatches(
            app_id, WebAppFilter::IsIsolatedWebAppWithOnlyUserManagement() &
                        !WebAppFilter::IsDevModeIsolatedApp()) &&
        !IsolatedWebAppTrustChecker::IsTrustedForTesting(
            origin.web_bundle_id())) {
      policy.app_version_for_filtering = iwa->isolation_data()->version();
    }
  }

  if (policy.app_version_for_filtering) {
    policy.filtered =
        ApplyEntitlements(policy.unfiltered, *policy.app_version_for_filtering,
                          origin.web_bundle_id());
  } else {
    policy.filtered = policy.unfiltered;
  }
}

void IwaPermissionsPolicyCache::UpdateFilteredPolicies() {
  if (!provider_) {
    return;
  }

  for (auto& [origin, policy] : cache_) {
    UpdateFilteredPolicy(origin, policy);
  }
}

void IwaPermissionsPolicyCache::Shutdown() {
  install_manager_observation_.Reset();
  runtime_data_subscription_ = {};
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
