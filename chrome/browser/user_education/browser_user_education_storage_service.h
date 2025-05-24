// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_EDUCATION_BROWSER_USER_EDUCATION_STORAGE_SERVICE_H_
#define CHROME_BROWSER_USER_EDUCATION_BROWSER_USER_EDUCATION_STORAGE_SERVICE_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"

class Profile;
class PrefRegistrySimple;

struct RecentSessionData {
  RecentSessionData();
  RecentSessionData(const RecentSessionData&);
  RecentSessionData& operator=(const RecentSessionData&);
  ~RecentSessionData();

  // Recent session start times, in descending order.
  std::vector<base::Time> recent_session_start_times;

  // When session start times started being recorded.
  std::optional<base::Time> enabled_time;
};

// Interface that provides recent session data.
class RecentSessionDataStorageService {
 public:
  // Read recent session data from the store.
  virtual RecentSessionData ReadRecentSessionData() const = 0;

  // Write recent session data to the store.
  virtual void SaveRecentSessionData(
      const RecentSessionData& recent_session_data) = 0;
};

class BrowserUserEducationStorageService
    : public user_education::UserEducationStorageService,
      public RecentSessionDataStorageService {
 public:
  explicit BrowserUserEducationStorageService(Profile* profile);
  ~BrowserUserEducationStorageService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Clear any data that should be wiped when the user elects to erase some or
  // all of their browsing history.
  static void ClearUsageHistory(Profile* profile);

  // UserEducationStorageService:
  void Reset(const base::Feature& iph_feature) override;
  std::optional<user_education::FeaturePromoData> ReadPromoData(
      const base::Feature& iph_feature) const override;
  void SavePromoData(
      const base::Feature& iph_feature,
      const user_education::FeaturePromoData& snooze_data) override;
  void ResetSession() override;
  user_education::UserEducationSessionData ReadSessionData() const override;
  void SaveSessionData(
      const user_education::UserEducationSessionData& session_data) override;
  user_education::FeaturePromoPolicyData ReadPolicyData() const override;
  void SavePolicyData(
      const user_education::FeaturePromoPolicyData& policy_data) override;
  void ResetPolicy() override;
  user_education::NewBadgeData ReadNewBadgeData(
      const base::Feature& new_badge_feature) const override;
  void SaveNewBadgeData(
      const base::Feature& new_badge_feature,
      const user_education::NewBadgeData& new_badge_data) override;
  void ResetNewBadge(const base::Feature& new_badge_feature) override;
  user_education::ProductMessagingData ReadProductMessagingData()
      const override;
  void SaveProductMessagingData(const user_education::ProductMessagingData&
                                    product_messaging_data) override;
  void ResetProductMessagingData() override;

  // RecentSessionDataStorageService:
  RecentSessionData ReadRecentSessionData() const override;
  void SaveRecentSessionData(
      const RecentSessionData& recent_session_data) override;
  void ResetRecentSessionData();

 private:
  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_USER_EDUCATION_BROWSER_USER_EDUCATION_STORAGE_SERVICE_H_
