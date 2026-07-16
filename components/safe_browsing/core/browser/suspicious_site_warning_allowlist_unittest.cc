// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

class SuspiciousSiteWarningAllowlistTest : public testing::Test {
 public:
  SuspiciousSiteWarningAllowlistTest() = default;

  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(pref_service_.registry());
    host_content_settings_map_ = new HostContentSettingsMap(
        &pref_service_, /*is_off_the_record=*/false,
        /*store_last_modified=*/true, /*restore_session=*/false,
        /*should_record_metrics=*/false);

    allowlist_ = std::make_unique<SuspiciousSiteWarningAllowlist>(
        host_content_settings_map_,
        SuspiciousSiteWarningAllowlist::kDefaultExpirationTimeout);
  }

  void TearDown() override {
    allowlist_.reset();
    host_content_settings_map_->ShutdownOnUIThread();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  std::unique_ptr<SuspiciousSiteWarningAllowlist> allowlist_;
};

TEST_F(SuspiciousSiteWarningAllowlistTest, AllowAndCheckHost) {
  const std::string host = "example.com";

  // Initially not allowed.
  EXPECT_FALSE(allowlist_->IsSiteAllowedForHost(host));

  // Allow host.
  allowlist_->AllowSiteForHost(host);
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host));

  // Different host should not be allowed.
  EXPECT_FALSE(allowlist_->IsSiteAllowedForHost("other.com"));
}

TEST_F(SuspiciousSiteWarningAllowlistTest, RevokeHost) {
  const std::string host1 = "example.com";
  const std::string host2 = "other.com";

  allowlist_->AllowSiteForHost(host1);
  allowlist_->AllowSiteForHost(host2);
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host1));
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host2));

  allowlist_->RevokeUserAllowException(host1);
  EXPECT_FALSE(allowlist_->IsSiteAllowedForHost(host1));
  // Ensure revoking one host does not affect the other.
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host2));
}

TEST_F(SuspiciousSiteWarningAllowlistTest, Expiration) {
  const std::string host = "example.com";

  allowlist_->AllowSiteForHost(host);
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host));

  // Advance clock by 29 days (less than 30-day TTL).
  task_environment_.FastForwardBy(base::Days(29));
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host));

  // Advance clock by 2 days (total 31 days > 30-day TTL).
  task_environment_.FastForwardBy(base::Days(2));
  EXPECT_FALSE(allowlist_->IsSiteAllowedForHost(host));
}

TEST_F(SuspiciousSiteWarningAllowlistTest, ClearAll) {
  const std::string host1 = "example1.com";
  const std::string host2 = "example2.com";

  allowlist_->AllowSiteForHost(host1);
  allowlist_->AllowSiteForHost(host2);
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host1));
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host2));

  allowlist_->Clear(base::Time(), base::Time::Max());
  EXPECT_FALSE(allowlist_->IsSiteAllowedForHost(host1));
  EXPECT_FALSE(allowlist_->IsSiteAllowedForHost(host2));
}

TEST_F(SuspiciousSiteWarningAllowlistTest, ClearByTimeRange) {
  const std::string host1 = "old.com";
  const std::string host2 = "new.com";

  base::Time begin_time = base::Time::Now();
  allowlist_->AllowSiteForHost(host1);

  // Advance clock by 5 days before allowing host2.
  task_environment_.FastForwardBy(base::Days(5));
  base::Time split_time = base::Time::Now();

  task_environment_.FastForwardBy(base::Seconds(1));
  allowlist_->AllowSiteForHost(host2);

  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host1));
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host2));

  // Clear entries created after split_time.
  allowlist_->Clear(split_time, base::Time::Max());
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host1));
  EXPECT_FALSE(allowlist_->IsSiteAllowedForHost(host2));

  // Clear entries from the beginning up to split_time.
  allowlist_->Clear(begin_time, split_time);
  EXPECT_FALSE(allowlist_->IsSiteAllowedForHost(host1));
}

TEST_F(SuspiciousSiteWarningAllowlistTest, ClearByPatternFilter) {
  const std::string host1 = "example.com";
  const std::string host2 = "other.com";

  allowlist_->AllowSiteForHost(host1);
  allowlist_->AllowSiteForHost(host2);
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host1));
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host2));

  auto predicate =
      base::BindRepeating([](const ContentSettingsPattern& primary_pattern,
                             const ContentSettingsPattern& secondary_pattern) {
        return primary_pattern == ContentSettingsPattern::FromURLNoWildcard(
                                      GURL("http://example.com"));
      });

  allowlist_->Clear(base::Time(), base::Time::Max(), predicate);
  EXPECT_FALSE(allowlist_->IsSiteAllowedForHost(host1));
  EXPECT_TRUE(allowlist_->IsSiteAllowedForHost(host2));
}

}  // namespace safe_browsing
