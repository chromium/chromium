// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONTENT_CLIPBOARD_RESTRICTION_SERVICE_H_
#define COMPONENTS_ENTERPRISE_CONTENT_CLIPBOARD_RESTRICTION_SERVICE_H_

#include <map>
#include <memory>

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/url_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/url_matcher/url_matcher.h"

namespace content {
class BrowserContext;
}

class PrefService;

class ClipboardRestrictionService : KeyedService {
 public:
  ClipboardRestrictionService(const ClipboardRestrictionService&) = delete;
  ClipboardRestrictionService& operator=(const ClipboardRestrictionService&) =
      delete;

  ~ClipboardRestrictionService() override;

  bool IsUrlAllowedToCopy(const GURL& url, int data_size) const;

 private:
  friend class ClipboardRestrictionServiceTest;
  friend class ClipboardRestrictionServiceFactory;

  explicit ClipboardRestrictionService(PrefService* pref_service);

  void UpdateSettings();

  PrefChangeRegistrar pref_change_registrar_;
  PrefService* pref_service_;

  url_matcher::URLMatcherConditionSet::ID next_id_;
  std::unique_ptr<url_matcher::URLMatcher> enable_url_matcher_;
  std::unique_ptr<url_matcher::URLMatcher> disable_url_matcher_;

  int min_data_size_;
};

class ClipboardRestrictionServiceFactory : BrowserContextKeyedServiceFactory {
 public:
  ClipboardRestrictionServiceFactory(
      const ClipboardRestrictionServiceFactory&) = delete;
  ClipboardRestrictionServiceFactory& operator=(
      const ClipboardRestrictionServiceFactory&) = delete;

  static ClipboardRestrictionServiceFactory* GetInstance();
  static ClipboardRestrictionService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  ClipboardRestrictionServiceFactory();
  ~ClipboardRestrictionServiceFactory() override;
  friend struct base::DefaultSingletonTraits<
      ClipboardRestrictionServiceFactory>;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // COMPONENTS_ENTERPRISE_CONTENT_CLIPBOARD_RESTRICTION_SERVICE_H_
