// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_service_impl.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_store.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/test_sync_service.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

using syncer::SyncServiceObserver;
using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::Mock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace {

const char kEmail[] = "foo_email";
const char kSuggestionsUrlPath[] = "/chromesuggestions";
const char kBlacklistUrlPath[] = "/chromesuggestions/blacklist";
const char kBlacklistClearUrlPath[] = "/chromesuggestions/blacklist/clear";
const char kTestTitle[] = "a title";
const char kTestUrl[] = "http://go.com";
const char kTestFaviconUrl[] =
    "https://s2.googleusercontent.com/s2/favicons?domain_url="
    "http://go.com&alt=s&sz=32";
const char kBlacklistedUrl[] = "http://blacklist.com";
const int64_t kTestSetExpiry = 12121212;  // This timestamp lies in the past.

// GMock matcher for protobuf equality.
MATCHER_P(EqualsProto, message, "") {
  // This implementation assumes protobuf serialization is deterministic, which
  // is true in practice but technically not something that code is supposed
  // to rely on.  However, it vastly simplifies the implementation.
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

}  // namespace

namespace suggestions {

SuggestionsProfile CreateSuggestionsProfile() {
  SuggestionsProfile profile;
  profile.set_timestamp(123);
  ChromeSuggestion* suggestion = profile.add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);
  return profile;
}

class TestSuggestionsStore : public suggestions::SuggestionsStore {
 public:
  TestSuggestionsStore() { cached_suggestions = CreateSuggestionsProfile(); }
  bool LoadSuggestions(SuggestionsProfile* suggestions) override {
    suggestions->CopyFrom(cached_suggestions);
    return cached_suggestions.suggestions_size();
  }
  bool StoreSuggestions(const SuggestionsProfile& suggestions) override {
    cached_suggestions.CopyFrom(suggestions);
    return true;
  }
  void ClearSuggestions() override {
    cached_suggestions = SuggestionsProfile();
  }

  SuggestionsProfile cached_suggestions;
};

class MockBlacklistStore : public suggestions::BlacklistStore {
 public:
  MOCK_METHOD1(BlacklistUrl, bool(const GURL&));
  MOCK_METHOD0(ClearBlacklist, void());
  MOCK_METHOD1(GetTimeUntilReadyForUpload, bool(base::TimeDelta*));
  MOCK_METHOD2(GetTimeUntilURLReadyForUpload,
               bool(const GURL&, base::TimeDelta*));
  MOCK_METHOD1(GetCandidateForUpload, bool(GURL*));
  MOCK_METHOD1(RemoveUrl, bool(const GURL&));
  MOCK_METHOD1(FilterSuggestions, void(SuggestionsProfile*));
};

class SuggestionsServiceTest : public testing::Test {
 protected:
  SuggestionsServiceTest() {
    identity_test_env_.MakePrimaryAccountAvailable(kEmail);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  ~SuggestionsServiceTest() override {}

  void SetUp() override {
    sync_service()->SetPreferredDataTypes({syncer::HISTORY_DELETE_DIRECTIVES});
    sync_service()->SetActiveDataTypes({syncer::HISTORY_DELETE_DIRECTIVES});

    // These objects are owned by the SuggestionsService, but we keep the
    // pointers around for testing.
    test_suggestions_store_ = new TestSuggestionsStore();
    mock_blacklist_store_ = new StrictMock<MockBlacklistStore>();
    suggestions_service_ = std::make_unique<SuggestionsServiceImpl>(
        identity_test_env_.identity_manager(), sync_service(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory()),
        base::WrapUnique(test_suggestions_store_),
        base::WrapUnique(mock_blacklist_store_),
        task_environment_.GetMockTickClock());
  }

  GURL GetCurrentlyQueriedUrl() {
    if (url_loader_factory()->NumPending() == 0)
      return GURL();

    return url_loader_factory()->pending_requests()->front().request.url;
  }

  bool RespondToSuggestionsFetch(const std::string& response_body,
                                 net::HttpStatusCode response_code,
                                 int net_error = net::OK) {
    return RespondToFetch(SuggestionsServiceImpl::BuildSuggestionsURL(),
                          response_body, response_code, net_error);
  }

  bool RespondToBlacklistFetch(const std::string& response_body,
                               net::HttpStatusCode response_code,
                               int net_error = net::OK) {
    return RespondToFetch(SuggestionsServiceImpl::BuildSuggestionsBlacklistURL(
                              GURL(kBlacklistedUrl)),
                          response_body, response_code, net_error);
  }

  bool RespondToFetchWithProfile(const SuggestionsProfile& suggestions) {
    return RespondToFetch(SuggestionsServiceImpl::BuildSuggestionsURL(),
                          suggestions.SerializeAsString(), net::HTTP_OK);
  }

  bool RespondToFetch(const GURL& url,
                      const std::string& response_body,
                      net::HttpStatusCode response_code,
                      int net_error = net::OK) {
    bool rv = url_loader_factory()->SimulateResponseForPendingRequest(
        url, network::URLLoaderCompletionStatus(net_error),
        network::CreateURLResponseHead(response_code), response_body);
    task_environment_.RunUntilIdle();
    return rv;
  }

  syncer::TestSyncService* sync_service() { return &test_sync_service_; }

  MockBlacklistStore* blacklist_store() { return mock_blacklist_store_; }

  TestSuggestionsStore* suggestions_store() { return test_suggestions_store_; }

  SuggestionsServiceImpl* suggestions_service() {
    return suggestions_service_.get();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  signin::IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService test_sync_service_;
  network::TestURLLoaderFactory url_loader_factory_;

  // Owned by the SuggestionsService.
  MockBlacklistStore* mock_blacklist_store_ = nullptr;
  TestSuggestionsStore* test_suggestions_store_ = nullptr;

  std::unique_ptr<SuggestionsServiceImpl> suggestions_service_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsServiceTest);
};

TEST_F(SuggestionsServiceTest, FetchSuggestionsData) {
  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. The data should be returned to the callback.
  suggestions_service()->FetchSuggestionsData();

  EXPECT_CALL(callback, Run(_));

  // Wait for the eventual network request.
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(GetCurrentlyQueriedUrl().is_valid());
  EXPECT_EQ(GetCurrentlyQueriedUrl().path(), kSuggestionsUrlPath);
  ASSERT_TRUE(RespondToFetchWithProfile(CreateSuggestionsProfile()));

  SuggestionsProfile suggestions;
  suggestions_store()->LoadSuggestions(&suggestions);
  ASSERT_EQ(1, suggestions.suggestions_size());
  EXPECT_EQ(kTestTitle, suggestions.suggestions(0).title());
  EXPECT_EQ(kTestUrl, suggestions.suggestions(0).url());
  EXPECT_EQ(kTestFaviconUrl, suggestions.suggestions(0).favicon_url());
}

TEST_F(SuggestionsServiceTest, IgnoresNoopSyncChange) {
  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // An no-op change should not result in a suggestions refresh.
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  // Wait for eventual (but unexpected) network requests.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->HasPendingRequestForTesting());
}

TEST_F(SuggestionsServiceTest, PersistentAuthErrorState) {
  // Put some suggestions in.
  suggestions_store()->StoreSuggestions(CreateSuggestionsProfile());

  GoogleServiceAuthError error =
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR);
  sync_service()->SetAuthError(std::move(error));
  // An no-op change should not result in a suggestions refresh.
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  // Wait for eventual (but unexpected) network requests.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->HasPendingRequestForTesting());

  SuggestionsProfile empty_suggestions;
  EXPECT_FALSE(suggestions_store()->LoadSuggestions(&empty_suggestions));
}

TEST_F(SuggestionsServiceTest, IgnoresUninterestingSyncChange) {
  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // An uninteresting change should not result in a network request (the
  // SyncState is INITIALIZED_ENABLED_HISTORY before and after).
  sync_service()->SetActiveDataTypes(
      {syncer::HISTORY_DELETE_DIRECTIVES, syncer::BOOKMARKS});
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  // Wait for eventual (but unexpected) network requests.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->HasPendingRequestForTesting());
}

// During startup, the state changes from NOT_INITIALIZED_ENABLED to
// INITIALIZED_ENABLED_HISTORY (for a signed-in user with history sync enabled).
// This should *not* result in an automatic fetch.
TEST_F(SuggestionsServiceTest, DoesNotFetchOnStartup) {
  // The sync service starts out inactive.
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  task_environment_.RunUntilIdle();
  ASSERT_FALSE(suggestions_service()->HasPendingRequestForTesting());

  // Sync getting enabled should not result in a fetch.
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  // Wait for eventual (but unexpected) network requests.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->HasPendingRequestForTesting());
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataSyncNotInitializedEnabled) {
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Try to fetch suggestions. Since sync is not active, no network request
  // should be sent.
  suggestions_service()->FetchSuggestionsData();

  // Wait for eventual (but unexpected) network requests.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->HasPendingRequestForTesting());

  // |suggestions_store()| should still contain the default values.
  SuggestionsProfile suggestions;
  suggestions_store()->LoadSuggestions(&suggestions);
  EXPECT_THAT(suggestions, EqualsProto(CreateSuggestionsProfile()));
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataSyncDisabled) {
  sync_service()->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_USER_CHOICE);
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::DISABLED);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Tell SuggestionsService that the sync state changed. The cache should be
  // cleared and empty data returned to the callback.
  EXPECT_CALL(callback, Run(EqualsProto(SuggestionsProfile())));
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  // Try to fetch suggestions. Since sync is not active, no network request
  // should be sent.
  suggestions_service()->FetchSuggestionsData();

  // Wait for eventual (but unexpected) network requests.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->HasPendingRequestForTesting());
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataNoAccessToken) {
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  suggestions_service()->FetchSuggestionsData();

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  // Wait for eventual (but unexpected) network requests.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->HasPendingRequestForTesting());
}

TEST_F(SuggestionsServiceTest, FetchingSuggestionsIgnoresRequestFailure) {
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  suggestions_service()->FetchSuggestionsData();

  // Wait for the eventual network request.
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(RespondToSuggestionsFetch("irrelevant", net::HTTP_OK,
                                        net::ERR_INVALID_RESPONSE));
}

TEST_F(SuggestionsServiceTest, FetchingSuggestionsClearsStoreIfResponseNotOK) {
  suggestions_store()->StoreSuggestions(CreateSuggestionsProfile());

  // Expect that an upload to the blacklist is scheduled.
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. Empty data will be returned to the callback.
  suggestions_service()->FetchSuggestionsData();

  // Wait for the eventual network request.
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(RespondToSuggestionsFetch("irrelevant", net::HTTP_BAD_REQUEST));

  SuggestionsProfile empty_suggestions;
  EXPECT_FALSE(suggestions_store()->LoadSuggestions(&empty_suggestions));
}

TEST_F(SuggestionsServiceTest, BlacklistURL) {
  const base::TimeDelta no_delay = base::TimeDelta::FromSeconds(0);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  EXPECT_CALL(*blacklist_store(), BlacklistUrl(GURL(kBlacklistedUrl)))
      .WillOnce(Return(true));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_)).Times(2);
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*blacklist_store(), GetCandidateForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(GURL(kBlacklistedUrl)), Return(true)));
  EXPECT_CALL(*blacklist_store(), RemoveUrl(GURL(kBlacklistedUrl)))
      .WillOnce(Return(true));

  EXPECT_CALL(callback, Run(_)).Times(2);

  EXPECT_TRUE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));

  // Wait on the upload task, the blacklist request and the next blacklist
  // scheduling task.
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(GetCurrentlyQueriedUrl().path(), kBlacklistUrlPath);
  // The blacklist fetch needs to contain a valid profile or the favicon will
  // not be set.
  ASSERT_TRUE(RespondToBlacklistFetch(
      CreateSuggestionsProfile().SerializeAsString(), net::HTTP_OK));
  task_environment_.RunUntilIdle();

  SuggestionsProfile suggestions;
  suggestions_store()->LoadSuggestions(&suggestions);
  ASSERT_EQ(1, suggestions.suggestions_size());
  EXPECT_EQ(kTestTitle, suggestions.suggestions(0).title());
  EXPECT_EQ(kTestUrl, suggestions.suggestions(0).url());
  EXPECT_EQ(kTestFaviconUrl, suggestions.suggestions(0).favicon_url());
}

TEST_F(SuggestionsServiceTest, BlacklistURLFails) {
  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  EXPECT_CALL(*blacklist_store(), BlacklistUrl(GURL(kBlacklistedUrl)))
      .WillOnce(Return(false));

  EXPECT_FALSE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));
}

TEST_F(SuggestionsServiceTest, RetryBlacklistURLRequestAfterFailure) {
  const base::TimeDelta no_delay = base::TimeDelta::FromSeconds(0);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Set expectations for first, failing request.
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .Times(AnyNumber())
      .WillRepeatedly(DoAll(SetArgPointee<0>(no_delay), Return(true)));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), BlacklistUrl(GURL(kBlacklistedUrl)))
      .WillOnce(Return(true));
  EXPECT_CALL(*blacklist_store(), GetCandidateForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(GURL(kBlacklistedUrl)), Return(true)));

  EXPECT_CALL(callback, Run(_)).Times(2);

  // Blacklist call, first request attempt.
  EXPECT_TRUE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));

  // Wait for the first scheduling receiving a failing response.
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(GetCurrentlyQueriedUrl().is_valid());
  EXPECT_EQ(GetCurrentlyQueriedUrl().path(), kBlacklistUrlPath);
  ASSERT_TRUE(RespondToBlacklistFetch("irrelevant", net::HTTP_OK,
                                      net::ERR_INVALID_RESPONSE));

  // Assert that the failure was processed as expected.
  Mock::VerifyAndClearExpectations(blacklist_store());

  // Now expect the retried request to succeed.
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .Times(AnyNumber())
      .WillRepeatedly(DoAll(SetArgPointee<0>(no_delay), Return(true)));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), GetCandidateForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(GURL(kBlacklistedUrl)), Return(true)));
  EXPECT_CALL(*blacklist_store(), RemoveUrl(GURL(kBlacklistedUrl)))
      .WillOnce(Return(true));

  // Wait for the second scheduling followed by a successful response.
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(suggestions_service()->HasPendingRequestForTesting());
  ASSERT_TRUE(GetCurrentlyQueriedUrl().is_valid());
  EXPECT_EQ(GetCurrentlyQueriedUrl().path(), kBlacklistUrlPath);
  ASSERT_TRUE(RespondToBlacklistFetch(
      CreateSuggestionsProfile().SerializeAsString(), net::HTTP_OK));

  SuggestionsProfile suggestions;
  suggestions_store()->LoadSuggestions(&suggestions);
  ASSERT_EQ(1, suggestions.suggestions_size());
  EXPECT_EQ(kTestTitle, suggestions.suggestions(0).title());
  EXPECT_EQ(kTestUrl, suggestions.suggestions(0).url());
  EXPECT_EQ(kTestFaviconUrl, suggestions.suggestions(0).favicon_url());
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURL) {
  // Ensure scheduling the request doesn't happen before undo.
  const base::TimeDelta delay = base::TimeDelta::FromHours(1);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Blacklist expectations.
  EXPECT_CALL(*blacklist_store(), BlacklistUrl(GURL(kBlacklistedUrl)))
      .WillOnce(Return(true));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_)).Times(AnyNumber());
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
  // Undo expectations.
  EXPECT_CALL(*blacklist_store(),
              GetTimeUntilURLReadyForUpload(GURL(kBlacklistedUrl), _))
      .WillOnce(DoAll(SetArgPointee<1>(delay), Return(true)));
  EXPECT_CALL(*blacklist_store(), RemoveUrl(GURL(kBlacklistedUrl)))
      .WillOnce(Return(true));

  EXPECT_CALL(callback, Run(_)).Times(2);
  EXPECT_TRUE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));
  EXPECT_TRUE(suggestions_service()->UndoBlacklistURL(GURL(kBlacklistedUrl)));
}

TEST_F(SuggestionsServiceTest, ClearBlacklist) {
  const base::TimeDelta delay = base::TimeDelta::FromHours(1);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Blacklist expectations.
  EXPECT_CALL(*blacklist_store(), BlacklistUrl(GURL(kBlacklistedUrl)))
      .WillOnce(Return(true));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_)).Times(AnyNumber());
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
  EXPECT_CALL(*blacklist_store(), ClearBlacklist());

  EXPECT_CALL(callback, Run(_)).Times(2);
  EXPECT_TRUE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));
  suggestions_service()->ClearBlacklist();

  // Wait for the eventual network request.
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(suggestions_service()->HasPendingRequestForTesting());
  EXPECT_EQ(GetCurrentlyQueriedUrl().path(), kBlacklistClearUrlPath);
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURLFailsIfNotInBlacklist) {
  // Ensure scheduling the request doesn't happen before undo.
  const base::TimeDelta delay = base::TimeDelta::FromHours(1);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Blacklist expectations.
  EXPECT_CALL(*blacklist_store(), BlacklistUrl(GURL(kBlacklistedUrl)))
      .WillOnce(Return(true));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
  // Undo expectations.
  // URL is not in local blacklist.
  EXPECT_CALL(*blacklist_store(),
              GetTimeUntilURLReadyForUpload(GURL(kBlacklistedUrl), _))
      .WillOnce(Return(false));

  EXPECT_CALL(callback, Run(_));

  EXPECT_TRUE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));
  EXPECT_FALSE(suggestions_service()->UndoBlacklistURL(GURL(kBlacklistedUrl)));
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURLFailsIfAlreadyCandidate) {
  // Ensure scheduling the request doesn't happen before undo.
  const base::TimeDelta delay = base::TimeDelta::FromHours(1);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Blacklist expectations.
  EXPECT_CALL(*blacklist_store(), BlacklistUrl(GURL(kBlacklistedUrl)))
      .WillOnce(Return(true));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));

  // URL is not yet candidate for upload.
  const base::TimeDelta negative_delay = base::TimeDelta::FromHours(-1);
  EXPECT_CALL(*blacklist_store(),
              GetTimeUntilURLReadyForUpload(GURL(kBlacklistedUrl), _))
      .WillOnce(DoAll(SetArgPointee<1>(negative_delay), Return(true)));

  EXPECT_CALL(callback, Run(_));

  EXPECT_TRUE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));
  EXPECT_FALSE(suggestions_service()->UndoBlacklistURL(GURL(kBlacklistedUrl)));
}

TEST_F(SuggestionsServiceTest, TemporarilyIncreasesBlacklistDelayOnFailure) {
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_)).Times(AnyNumber());
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  const base::TimeDelta initial_delay =
      suggestions_service()->BlacklistDelayForTesting();

  // Delay unchanged on success.
  suggestions_service()->FetchSuggestionsData();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(RespondToFetchWithProfile(CreateSuggestionsProfile()));
  EXPECT_EQ(initial_delay, suggestions_service()->BlacklistDelayForTesting());

  // Delay increases on failure.
  suggestions_service()->FetchSuggestionsData();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(RespondToSuggestionsFetch("irrelevant", net::HTTP_BAD_REQUEST));
  base::TimeDelta delay_after_fail =
      suggestions_service()->BlacklistDelayForTesting();
  EXPECT_GT(delay_after_fail, initial_delay);

  // Success resets future delays, but the current horizon remains. Since no
  // time has passed, the actual current delay stays the same.
  suggestions_service()->FetchSuggestionsData();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(RespondToFetchWithProfile(CreateSuggestionsProfile()));
  EXPECT_EQ(delay_after_fail,
            suggestions_service()->BlacklistDelayForTesting());

  // After the current horizon has passed, we're back at the initial delay.
  task_environment_.FastForwardBy(delay_after_fail);
  suggestions_service()->FetchSuggestionsData();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(RespondToFetchWithProfile(CreateSuggestionsProfile()));
  EXPECT_EQ(initial_delay, suggestions_service()->BlacklistDelayForTesting());
}

TEST_F(SuggestionsServiceTest, DoesNotOverrideDefaultExpiryTime) {
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  suggestions_service()->FetchSuggestionsData();

  task_environment_.RunUntilIdle();
  // Creates one suggestion without timestamp and adds a second with timestamp.
  SuggestionsProfile profile = CreateSuggestionsProfile();
  ChromeSuggestion* suggestion = profile.add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);
  suggestion->set_expiry_ts(kTestSetExpiry);
  ASSERT_TRUE(RespondToFetchWithProfile(profile));

  SuggestionsProfile suggestions;
  suggestions_store()->LoadSuggestions(&suggestions);
  ASSERT_EQ(2, suggestions.suggestions_size());
  // Suggestion[0] had no time stamp and should be ahead of the old suggestion.
  EXPECT_LT(kTestSetExpiry, suggestions.suggestions(0).expiry_ts());
  // Suggestion[1] had a very old time stamp but should not be updated.
  EXPECT_EQ(kTestSetExpiry, suggestions.suggestions(1).expiry_ts());
}

}  // namespace suggestions
