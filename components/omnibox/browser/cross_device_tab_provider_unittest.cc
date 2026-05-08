// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/cross_device_tab_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/mock_open_tabs_ui_delegate.h"
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
  testing::NiceMock<sync_sessions::MockOpenTabsUIDelegate>
      open_tabs_ui_delegate_;
  std::unique_ptr<MockAutocompleteProviderClient> client_;
  scoped_refptr<CrossDeviceTabProvider> provider_;
};

TEST_F(CrossDeviceTabProviderTest, NoRemoteSessions) {
  EXPECT_CALL(open_tabs_ui_delegate_, GetAllForeignSessions(_))
      .WillOnce(Return(false));

  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), IsEmpty());
}

TEST_F(CrossDeviceTabProviderTest, MostRecentTab) {
  std::vector<std::unique_ptr<sync_sessions::SyncedSession>> foreign_sessions;
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();

  tab->timestamp = base::Time::Now() - base::Minutes(1);
  tab->navigations.push_back(sessions::SerializedNavigationEntry());
  tab->navigations.back().set_virtual_url(GURL("https://example.com/"));
  tab->navigations.back().set_title(u"Example");

  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);
  foreign_sessions.push_back(std::move(session));

  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      session_ptrs;
  for (const auto& s : foreign_sessions) {
    session_ptrs.push_back(s.get());
  }

  EXPECT_CALL(open_tabs_ui_delegate_, GetAllForeignSessions(_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(session_ptrs),
                               Return(true)));

  provider_->Start(CreateZeroSuggestInput(), false);

  ASSERT_THAT(provider_->matches(), SizeIs(1u));
  EXPECT_EQ(provider_->matches()[0].destination_url,
            GURL("https://example.com/"));
  EXPECT_EQ(provider_->matches()[0].description, u"Example");
}

TEST_F(CrossDeviceTabProviderTest, AgeLimit) {
  std::vector<std::unique_ptr<sync_sessions::SyncedSession>> foreign_sessions;
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();

  // Age is 10 minutes, default limit is 5 minutes.
  tab->timestamp = base::Time::Now() - base::Minutes(10);
  tab->navigations.push_back(sessions::SerializedNavigationEntry());
  tab->navigations.back().set_virtual_url(GURL("https://example.com/"));

  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);
  foreign_sessions.push_back(std::move(session));

  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      session_ptrs;
  for (const auto& s : foreign_sessions) {
    session_ptrs.push_back(s.get());
  }

  EXPECT_CALL(open_tabs_ui_delegate_, GetAllForeignSessions(_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(session_ptrs),
                               Return(true)));

  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), IsEmpty());
}

TEST_F(CrossDeviceTabProviderTest, CustomAgeLimit) {
  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOmniboxCrossDeviceTabZeroSuggest, {{"max_age_minutes", "15"}});

  std::vector<std::unique_ptr<sync_sessions::SyncedSession>> foreign_sessions;
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();

  // Age is 10 minutes, custom limit is 15 minutes.
  tab->timestamp = base::Time::Now() - base::Minutes(10);
  tab->navigations.push_back(sessions::SerializedNavigationEntry());
  tab->navigations.back().set_virtual_url(GURL("https://example.com/"));

  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);
  foreign_sessions.push_back(std::move(session));

  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      session_ptrs;
  for (const auto& s : foreign_sessions) {
    session_ptrs.push_back(s.get());
  }

  EXPECT_CALL(open_tabs_ui_delegate_, GetAllForeignSessions(_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(session_ptrs),
                               Return(true)));

  provider_->Start(CreateZeroSuggestInput(), false);

  EXPECT_THAT(provider_->matches(), SizeIs(1u));
}

}  // namespace
