// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/safe_search_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_search_api/safe_search/safe_search_url_checker_client.h"
#include "components/safe_search_api/url_checker.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/net_errors.h"

namespace {

// Calls the SafeSearchService callback with the result of the Safe Search
// API check.
void OnCheckURLDone(SafeSearchService::CheckSafeSearchCallback callback,
                    const GURL& /* url */,
                    safe_search_api::Classification classification,
                    safe_search_api::ClassificationDetails details) {
  std::move(callback).Run(classification ==
                          safe_search_api::Classification::SAFE);
}

}  // namespace

SafeSearchService::SafeSearchService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

SafeSearchService::~SafeSearchService() = default;

bool SafeSearchService::CheckSafeSearchURL(const GURL& url,
                                           CheckSafeSearchCallback callback) {
  if (!safe_search_url_checker_) {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("safe_search_service", R"(
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
            browser_context_->GetDefaultStoragePartition()
                ->GetURLLoaderFactoryForBrowserProcess(),
            traffic_annotation));
  }

  return safe_search_url_checker_->CheckURL(
      url_matcher::util::Normalize(url),
      base::BindOnce(&OnCheckURLDone, std::move(callback)));
}

void SafeSearchService::SetSafeSearchURLCheckerForTest(
    std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker) {
  safe_search_url_checker_ = std::move(safe_search_url_checker);
}

// static
SafeSearchFactory* SafeSearchFactory::GetInstance() {
  return base::Singleton<SafeSearchFactory>::get();
}

// static
SafeSearchService* SafeSearchFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SafeSearchService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

SafeSearchFactory::SafeSearchFactory()
    : BrowserContextKeyedServiceFactory(
          "SafeSearch",
          BrowserContextDependencyManager::GetInstance()) {}

SafeSearchFactory::~SafeSearchFactory() = default;

std::unique_ptr<KeyedService>
SafeSearchFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SafeSearchService>(context);
}

content::BrowserContext* SafeSearchFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
