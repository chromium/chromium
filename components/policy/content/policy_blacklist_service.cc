// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blacklist_service.h"

#include <utility>

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/url_util.h"
#include "components/safe_search_api/safe_search/safe_search_url_checker_client.h"
#include "components/safe_search_api/url_checker.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/net_errors.h"

namespace {

// Calls the PolicyBlacklistService callback with the result of the Safe Search
// API check.
void OnCheckURLDone(PolicyBlacklistService::CheckSafeSearchCallback callback,
                    const GURL& /* url */,
                    safe_search_api::Classification classification,
                    bool /* uncertain */) {
  std::move(callback).Run(classification ==
                          safe_search_api::Classification::SAFE);
}

}  // namespace

PolicyBlacklistService::PolicyBlacklistService(
    content::BrowserContext* browser_context,
    std::unique_ptr<policy::URLBlacklistManager> url_blacklist_manager)
    : browser_context_(browser_context),
      url_blacklist_manager_(std::move(url_blacklist_manager)) {}

PolicyBlacklistService::~PolicyBlacklistService() = default;

policy::URLBlacklist::URLBlacklistState
PolicyBlacklistService::GetURLBlacklistState(const GURL& url) const {
  return url_blacklist_manager_->GetURLBlacklistState(url);
}

bool PolicyBlacklistService::CheckSafeSearchURL(
    const GURL& url,
    CheckSafeSearchCallback callback) {
  if (!safe_search_url_checker_) {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("policy_blacklist_service", R"(
          semantics {
            sender: "Cloud Policy"
            description:
              "Checks whether a given URL (or set of URLs) is considered safe "
              "by Google SafeSearch."
            trigger:
              "If the policy for safe sites is enabled, this is sent for every "
              "top-level navigation if the result isn't already cached."
            data: "URL to be checked."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "This feature is off by default and cannot be controlled in "
              "settings."
            chrome_policy {
              SafeSitesFilterBehavior {
                SafeSitesFilterBehavior: 0
              }
            }
          })");

    safe_search_url_checker_ = std::make_unique<safe_search_api::URLChecker>(
        std::make_unique<safe_search_api::SafeSearchURLCheckerClient>(
            content::BrowserContext::GetDefaultStoragePartition(
                browser_context_)
                ->GetURLLoaderFactoryForBrowserProcess(),
            traffic_annotation));
  }

  return safe_search_url_checker_->CheckURL(
      policy::url_util::Normalize(url),
      base::BindOnce(&OnCheckURLDone, std::move(callback)));
}

void PolicyBlacklistService::SetSafeSearchURLCheckerForTest(
    std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker) {
  safe_search_url_checker_ = std::move(safe_search_url_checker);
}

// static
PolicyBlacklistFactory* PolicyBlacklistFactory::GetInstance() {
  return base::Singleton<PolicyBlacklistFactory>::get();
}

// static
PolicyBlacklistService* PolicyBlacklistFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PolicyBlacklistService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

PolicyBlacklistFactory::PolicyBlacklistFactory()
    : BrowserContextKeyedServiceFactory(
          "PolicyBlacklist",
          BrowserContextDependencyManager::GetInstance()) {}

PolicyBlacklistFactory::~PolicyBlacklistFactory() = default;

KeyedService* PolicyBlacklistFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  PrefService* pref_service = user_prefs::UserPrefs::Get(context);
  auto url_blacklist_manager =
      std::make_unique<policy::URLBlacklistManager>(pref_service);
  return new PolicyBlacklistService(context, std::move(url_blacklist_manager));
}

content::BrowserContext* PolicyBlacklistFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // TODO(crbug.com/701326): This DCHECK should be moved to GetContextToUse().
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return context;
}
