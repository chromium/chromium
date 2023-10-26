// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_EDUCATION_BROWSER_FEATURE_PROMO_STORAGE_SERVICE_H_
#define CHROME_BROWSER_USER_EDUCATION_BROWSER_FEATURE_PROMO_STORAGE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;
class PrefRegistrySimple;

class BrowserFeaturePromoStorageService
    : public user_education::FeaturePromoStorageService {
 public:
  explicit BrowserFeaturePromoStorageService(Profile* profile);
  ~BrowserFeaturePromoStorageService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // FeaturePromoStorageService:
  void Reset(const base::Feature& iph_feature) override;
  absl::optional<PromoData> ReadPromoData(
      const base::Feature& iph_feature) const override;
  void SavePromoData(const base::Feature& iph_feature,
                     const PromoData& snooze_data) override;

 private:
  // TODO(crbug.com/1121399): refactor prefs code so friending tests
  // isn't necessary.
  friend class FeaturePromoStorageInteractiveTest;

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_USER_EDUCATION_BROWSER_FEATURE_PROMO_STORAGE_SERVICE_H_
