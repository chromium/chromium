// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONTENT_CLIPBOARD_RESTRICTION_SERVICE_H_
#define COMPONENTS_ENTERPRISE_CONTENT_CLIPBOARD_RESTRICTION_SERVICE_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
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

  // Returns true if the user is allowed to write data to the clipboard from
  // `url` as per the value of the CopyPreventionSettings policy, false
  // otherwise. This check is only performed if `data_size` is larger than the
  // `minimum_data_size` specified in the policy.
  //
  // If this returns false and `replacement_data` is supplied, a message will
  // be written to `replacement_data` that is meant to be written to the
  // clipboard instead of the intended data.
  bool IsUrlAllowedToCopy(const GURL& url,
                          size_t data_size_in_bytes,
                          std::u16string* replacement_data = nullptr) const;

 private:
  friend class ClipboardRestrictionServiceTest;
  friend class ClipboardRestrictionServiceFactory;

  explicit ClipboardRestrictionService(PrefService* pref_service);

  void UpdateSettings();

  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<PrefService> pref_service_;

  base::MatcherStringPattern::ID next_id_;
  std::unique_ptr<url_matcher::URLMatcher> enable_url_matcher_;
  std::unique_ptr<url_matcher::URLMatcher> disable_url_matcher_;

  size_t min_data_size_;
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
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // COMPONENTS_ENTERPRISE_CONTENT_CLIPBOARD_RESTRICTION_SERVICE_H_
