// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_utils.h"

#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr char kUsername[] = "username";
constexpr char kPasswordTrigger[] = "PASSWORD_ENTRY";

}  // namespace

TEST(ReportingUtilsTest, GetPasswordBreachEventReturnsValidEvent) {
  std::vector<std::pair<GURL, std::u16string>> identities = {
      {GURL("https://google.com/"), u"username"}};
  ReportingSettings settings;
  std::map<std::string, std::vector<std::string>> enabled_opt_in_events;
  enabled_opt_in_events["passwordBreachEvent"].push_back("https://google.com/");
  settings.enabled_opt_in_events.insert(enabled_opt_in_events.begin(),
                                        enabled_opt_in_events.end());

  auto event = GetPasswordBreachEvent(kPasswordTrigger, identities, settings);
  ASSERT_EQ(
      event->trigger(),
      chrome::cros::reporting::proto::PasswordBreachEvent::PASSWORD_ENTRY);
  auto identity = event->identities()[0];
  ASSERT_EQ(identity.url(), "https://google.com/");
  ASSERT_EQ(identity.username(), "*****");
}

TEST(ReportingUtilsTest,
     GetPasswordBreachEventReturnsEmptyEventForNoMatchedUrl) {
  std::vector<std::pair<GURL, std::u16string>> identities = {
      {GURL("https://example.com/"), u"username"}};
  ReportingSettings settings;
  std::map<std::string, std::vector<std::string>> enabled_opt_in_events;
  enabled_opt_in_events["passwordBreachEvent"].push_back("https://google.com/");
  settings.enabled_opt_in_events.insert(enabled_opt_in_events.begin(),
                                        enabled_opt_in_events.end());

  auto event = GetPasswordBreachEvent(kPasswordTrigger, identities, settings);
  ASSERT_FALSE(event.has_value());
}

TEST(ReportingUtilsTest,
     GetPasswordBreachEventReturnsEmptyEventForEmptySettings) {
  std::vector<std::pair<GURL, std::u16string>> identities = {
      {GURL("https://google.com/"), u"username"}};
  ReportingSettings settings;

  auto event = GetPasswordBreachEvent(kPasswordTrigger, identities, settings);
  ASSERT_FALSE(event.has_value());
}

TEST(ReportingUtilsTest, GetPasswordReuseEventWithWarning) {
  auto event = GetPasswordReuseEvent(
      /*url=*/GURL("https://google.com/"), /*user_name=*/kUsername,
      /*is_phishing_url=*/false, /*warning_shown=*/true);
  ASSERT_EQ(event.url(), "https://google.com/");
  ASSERT_EQ(event.user_name(), kUsername);
  ASSERT_FALSE(event.is_phishing_url());
  ASSERT_EQ(event.event_result(),
            chrome::cros::reporting::proto::EVENT_RESULT_WARNED);
}

TEST(ReportingUtilsTest, GetPasswordReuseEventWithoutWarning) {
  auto event = GetPasswordReuseEvent(
      /*url=*/GURL("https://google.com/"), /*user_name=*/kUsername,
      /*is_phishing_url=*/false, /*warning_shown=*/true);
  ASSERT_EQ(event.url(), "https://google.com/");
  ASSERT_EQ(event.user_name(), kUsername);
  ASSERT_FALSE(event.is_phishing_url());
  ASSERT_EQ(event.event_result(),
            chrome::cros::reporting::proto::EVENT_RESULT_WARNED);
}

TEST(ReportingUtilsTest, GetPasswordChangedEvent) {
  auto event = GetPasswordChangedEvent(kUsername);
  ASSERT_EQ(event.user_name(), kUsername);
}

}  // namespace enterprise_connectors
