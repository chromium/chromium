// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/cross_device_tab_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/fake_open_tabs_ui_delegate.h"
#include "components/sync_sessions/mock_session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "url/gurl.h"

namespace {

using testing::_;
using testing::IsEmpty;
using testing::Return;
using testing::SizeIs;

class CrossDeviceTabProviderTest : public testing::Test {
 public:
  CrossDeviceTabProviderTest() {
    feature_list_.InitAndEnableFeature(
        omnibox::kOmniboxCrossDeviceTabZeroSuggest);

    client_ = std::make_unique<MockAutocompleteProviderClient>();
    client_->set_session_sync_service(&session_sync_service_);

    provider_ = base::MakeRefCounted<CrossDeviceTabProvider>(client_.get());

    ON_CALL(session_sync_service_, GetOpenTabsUIDelegate())
        .WillByDefault(Return(&open_tabs_ui_delegate_));
  }

 protected:
  AutocompleteInput CreateZeroSuggestInput() {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP,
                            TestSchemeClassifier());
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    return input;
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<sync_sessions::MockSessionSyncService>
      session_sync_service_;
  sync_sessions::FakeOpenTabsUIDelegate open_tabs_ui_delegate_;
  std::unique_ptr<MockAutocompleteProviderClient> client_;
  scoped_refptr<CrossDeviceTabProvider> provider_;
};

TEST_F(CrossDeviceTabProviderTest, NoRemoteSessions) {
  base::HistogramTester histogram_tester;
  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), IsEmpty());
  EXPECT_TRUE(provider_->most_recent_tab_timestamp().is_null());
  histogram_tester.ExpectUniqueSample(
      "Omnibox.CrossDeviceTab.Provider.Eligibility",
      CrossDeviceTabProvider::Eligibility::kNoForeignSessions, 1);
}

TEST_F(CrossDeviceTabProviderTest, MostRecentTab) {
  base::HistogramTester histogram_tester;
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();

  const base::Time tab_timestamp = base::Time::Now() - base::Minutes(1);
  tab->timestamp = tab_timestamp;
  tab->navigations.push_back(sessions::SerializedNavigationEntry());
  tab->navigations.back().set_virtual_url(GURL("https://example.com/"));
  tab->navigations.back().set_title(u"Example");

  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);

  open_tabs_ui_delegate_.AddForeignSession(std::move(session));

  provider_->Start(CreateZeroSuggestInput(), false);

  ASSERT_THAT(provider_->matches(), SizeIs(1u));
  EXPECT_EQ(provider_->matches()[0].destination_url,
            GURL("https://example.com/"));
  EXPECT_EQ(provider_->matches()[0].description, u"Example");
  EXPECT_EQ(provider_->most_recent_tab_timestamp(), tab_timestamp);

  histogram_tester.ExpectUniqueSample(
      "Omnibox.CrossDeviceTab.Provider.Eligibility",
      CrossDeviceTabProvider::Eligibility::kMatchCreated, 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.CrossDeviceTab.Provider.MostRecentTabAge",
      base::Minutes(1).InMilliseconds(), 1);
}

TEST_F(CrossDeviceTabProviderTest, AgeLimit) {
  task_environment_.FastForwardBy(base::Minutes(10));

  base::HistogramTester histogram_tester;
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();

  // Age is 10 minutes, default limit is 5 minutes.
  tab->timestamp = base::Time::Now() - base::Minutes(10);
  tab->navigations.push_back(sessions::SerializedNavigationEntry());
  tab->navigations.back().set_virtual_url(GURL("https://example.com/"));

  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);

  open_tabs_ui_delegate_.AddForeignSession(std::move(session));

  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), IsEmpty());
  EXPECT_TRUE(provider_->most_recent_tab_timestamp().is_null());

  histogram_tester.ExpectUniqueSample(
      "Omnibox.CrossDeviceTab.Provider.Eligibility",
      CrossDeviceTabProvider::Eligibility::kLocalSessionNotRecent, 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.CrossDeviceTab.Provider.MostRecentTabAge",
      base::Minutes(10).InMilliseconds(), 1);
}

TEST_F(CrossDeviceTabProviderTest, CustomAgeLimit) {
  task_environment_.FastForwardBy(base::Minutes(10));

  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOmniboxCrossDeviceTabZeroSuggest,
      {{omnibox::kOmniboxCrossDeviceTabZeroSuggestMaxAgeMinutes.name, "15"}});

  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();

  // Age is 10 minutes, custom limit is 15 minutes.
  const base::Time tab_timestamp = base::Time::Now() - base::Minutes(10);
  tab->timestamp = tab_timestamp;
  tab->navigations.push_back(sessions::SerializedNavigationEntry());
  tab->navigations.back().set_virtual_url(GURL("https://example.com/"));

  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);

  open_tabs_ui_delegate_.AddForeignSession(std::move(session));

  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), SizeIs(1u));
  EXPECT_EQ(provider_->most_recent_tab_timestamp(), tab_timestamp);
}

TEST_F(CrossDeviceTabProviderTest, NoSyncService) {
  base::HistogramTester histogram_tester;
  client_->set_session_sync_service(nullptr);

  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), IsEmpty());
  EXPECT_TRUE(provider_->most_recent_tab_timestamp().is_null());
  histogram_tester.ExpectUniqueSample(
      "Omnibox.CrossDeviceTab.Provider.Eligibility",
      CrossDeviceTabProvider::Eligibility::kNoSyncService, 1);
}

TEST_F(CrossDeviceTabProviderTest, NoOpenTabsDelegate) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(session_sync_service_, GetOpenTabsUIDelegate())
      .WillOnce(Return(nullptr));

  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), IsEmpty());
  EXPECT_TRUE(provider_->most_recent_tab_timestamp().is_null());
  histogram_tester.ExpectUniqueSample(
      "Omnibox.CrossDeviceTab.Provider.Eligibility",
      CrossDeviceTabProvider::Eligibility::kNoOpenTabsDelegate, 1);
}

TEST_F(CrossDeviceTabProviderTest, NoTabs) {
  base::HistogramTester histogram_tester;
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  // Window has no tabs.
  session->windows[SessionID::NewUnique()] = std::move(window);
  open_tabs_ui_delegate_.AddForeignSession(std::move(session));

  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), IsEmpty());
  EXPECT_TRUE(provider_->most_recent_tab_timestamp().is_null());
  histogram_tester.ExpectUniqueSample(
      "Omnibox.CrossDeviceTab.Provider.Eligibility",
      CrossDeviceTabProvider::Eligibility::kNoTabs, 1);
}

TEST_F(CrossDeviceTabProviderTest, InvalidURL) {
  base::HistogramTester histogram_tester;
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();

  tab->timestamp = base::Time::Now() - base::Minutes(1);
  tab->navigations.push_back(sessions::SerializedNavigationEntry());
  // Invalid URL (empty GURL is invalid).
  tab->navigations.back().set_virtual_url(GURL());

  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);
  open_tabs_ui_delegate_.AddForeignSession(std::move(session));

  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), IsEmpty());
  EXPECT_TRUE(provider_->most_recent_tab_timestamp().is_null());
  histogram_tester.ExpectUniqueSample(
      "Omnibox.CrossDeviceTab.Provider.Eligibility",
      CrossDeviceTabProvider::Eligibility::kInvalidUrl, 1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.CrossDeviceTab.Provider.MostRecentTabAge", 0);
}

TEST_F(CrossDeviceTabProviderTest, NonHttpOrHttpsUrl) {
  for (const char* url_str : {"javascript:alert(1)", "data:text/html,test",
                              "file:///etc/passwd", "chrome://settings/"}) {
    base::HistogramTester histogram_tester;
    auto* session = open_tabs_ui_delegate_.AddForeignSession("test");
    auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
    auto tab = std::make_unique<sessions::SessionTab>();

    tab->timestamp = base::Time::Now() - base::Minutes(1);
    tab->navigations.push_back(sessions::SerializedNavigationEntry());
    tab->navigations.back().set_virtual_url(GURL(url_str));

    window->wrapped_window.tabs.push_back(std::move(tab));
    session->windows[SessionID::NewUnique()] = std::move(window);

    provider_->Start(CreateZeroSuggestInput(), false);

    EXPECT_THAT(provider_->matches(), IsEmpty()) << url_str;
    EXPECT_TRUE(provider_->most_recent_tab_timestamp().is_null()) << url_str;
    histogram_tester.ExpectUniqueSample(
        "Omnibox.CrossDeviceTab.Provider.Eligibility",
        CrossDeviceTabProvider::Eligibility::kInvalidUrl, 1);
    histogram_tester.ExpectTotalCount(
        "Omnibox.CrossDeviceTab.Provider.MostRecentTabAge", 0);

    open_tabs_ui_delegate_.DeleteForeignSession("test");
  }
}

TEST_F(CrossDeviceTabProviderTest, MeetsDelayedContinuationCriterion) {
  base::HistogramTester histogram_tester;
  // Fast forward to 2 minutes after provider creation.
  task_environment_.FastForwardBy(base::Minutes(2));

  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();

  // Age is 1 hour, default limit is 5 minutes, so fails simultaneous use
  // criterion. But delayed continuation limit is 720 minutes (12 hours) and
  // profile uptime is 2 mins (<= 5 mins), so meets delayed continuation
  // criterion.
  tab->timestamp = base::Time::Now() - base::Hours(1);
  tab->navigations.push_back(sessions::SerializedNavigationEntry());
  tab->navigations.back().set_virtual_url(GURL("https://example.com/"));

  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);

  open_tabs_ui_delegate_.AddForeignSession(std::move(session));

  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), SizeIs(1u));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.CrossDeviceTab.Provider.Eligibility",
      CrossDeviceTabProvider::Eligibility::kMatchCreated, 1);
}

TEST_F(CrossDeviceTabProviderTest, FailsBothAgeCriteria) {
  base::HistogramTester histogram_tester;
  // Fast forward to 2 minutes after provider creation.
  task_environment_.FastForwardBy(base::Minutes(2));

  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();

  // Age is 13 hours, default limit is 5 minutes (fails simultaneous use).
  // Delayed continuation limit is 720 minutes (12 hours) (fails delayed
  // continuation too).
  tab->timestamp = base::Time::Now() - base::Hours(13);
  tab->navigations.push_back(sessions::SerializedNavigationEntry());
  tab->navigations.back().set_virtual_url(GURL("https://example.com/"));

  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);

  open_tabs_ui_delegate_.AddForeignSession(std::move(session));

  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), IsEmpty());
  histogram_tester.ExpectUniqueSample(
      "Omnibox.CrossDeviceTab.Provider.Eligibility",
      CrossDeviceTabProvider::Eligibility::kTabTooOld, 1);
}

TEST_F(CrossDeviceTabProviderTest, FailsDelayedContinuationUptime) {
  base::HistogramTester histogram_tester;
  // Fast forward to 10 minutes after provider creation.
  task_environment_.FastForwardBy(base::Minutes(10));

  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();

  // Age is 1 hour, default limit is 5 minutes (fails simultaneous use).
  // Delayed continuation limit is 720 minutes (12 hours) (meets age part), but
  // profile uptime is 10 mins (> 5 min limit), so fails delayed continuation
  // criterion.
  tab->timestamp = base::Time::Now() - base::Hours(1);
  tab->navigations.push_back(sessions::SerializedNavigationEntry());
  tab->navigations.back().set_virtual_url(GURL("https://example.com/"));

  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);

  open_tabs_ui_delegate_.AddForeignSession(std::move(session));

  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), IsEmpty());
  histogram_tester.ExpectUniqueSample(
      "Omnibox.CrossDeviceTab.Provider.Eligibility",
      CrossDeviceTabProvider::Eligibility::kLocalSessionNotRecent, 1);
}

}  // namespace
