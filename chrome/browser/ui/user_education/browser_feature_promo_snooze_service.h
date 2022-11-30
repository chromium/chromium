// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_BROWSER_FEATURE_PROMO_SNOOZE_SERVICE_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_BROWSER_FEATURE_PROMO_SNOOZE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_snooze_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;
class PrefRegistrySimple;

class BrowserFeaturePromoSnoozeService
    : public user_education::FeaturePromoSnoozeService {
 public:
  explicit BrowserFeaturePromoSnoozeService(Profile* profile);
  ~BrowserFeaturePromoSnoozeService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // FeaturePromoSnoozeService:
  void Reset(const base::Feature& iph_feature) override;
  absl::optional<SnoozeData> ReadSnoozeData(
      const base::Feature& iph_feature) override;
  void SaveSnoozeData(const base::Feature& iph_feature,
                      const SnoozeData& snooze_data) override;

 private:
  // TODO(crbug.com/1121399): refactor prefs code so friending tests
  // isn't necessary.
  friend class FeaturePromoSnoozeInteractiveTest;

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_BROWSER_FEATURE_PROMO_SNOOZE_SERVICE_H_
