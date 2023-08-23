// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_SAFE_SEARCH_SERVICE_H_
#define COMPONENTS_POLICY_CONTENT_SAFE_SEARCH_SERVICE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace safe_search_api {
class URLChecker;
}  // namespace safe_search_api

// SafeSearchService and SafeSearchFactory provide a way for
// us to access a Context-specific instance of safe_search_api.
class SafeSearchService : public KeyedService {
 public:
  using CheckSafeSearchCallback = base::OnceCallback<void(bool is_safe)>;

  explicit SafeSearchService(content::BrowserContext* browser_context);
  SafeSearchService(const SafeSearchService&) = delete;
  SafeSearchService& operator=(const SafeSearchService&) = delete;

  ~SafeSearchService() override;

  // Starts a call to the Safe Search API for the given URL to determine whether
  // the URL is "safe" (not porn). Returns whether |callback| was run
  // synchronously.
  bool CheckSafeSearchURL(const GURL& url, CheckSafeSearchCallback callback);

  // Creates a SafeSearch URLChecker using a given URLLoaderFactory for testing.
  void SetSafeSearchURLCheckerForTest(
      std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker);

 private:
  const raw_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker_;
};

class SafeSearchFactory : public BrowserContextKeyedServiceFactory {
 public:
  SafeSearchFactory(const SafeSearchFactory&) = delete;
  SafeSearchFactory& operator=(const SafeSearchFactory&) = delete;

  static SafeSearchFactory* GetInstance();
  static SafeSearchService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  SafeSearchFactory();
  ~SafeSearchFactory() override;
  friend struct base::DefaultSingletonTraits<SafeSearchFactory>;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  // Finds which browser context (if any) to use.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // COMPONENTS_POLICY_CONTENT_SAFE_SEARCH_SERVICE_H_
