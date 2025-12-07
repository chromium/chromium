// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_page_handler.h"

#include <memory>

#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_observer.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_shared_ui.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockDocument
    : public new_tab_page::mojom::MicrosoftAuthUntrustedDocument {
 public:
  MockDocument() = default;
  ~MockDocument() override = default;

  mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
  BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD0(AcquireTokenPopup, void());
  MOCK_METHOD0(AcquireTokenSilent, void());
  MOCK_METHOD0(SignOut, void());

  mojo::Receiver<new_tab_page::mojom::MicrosoftAuthUntrustedDocument> receiver_{
      this};
};

class MockMicrosoftAuthService : public MicrosoftAuthService {
 public:
  MOCK_METHOD(void, AddObserver, (MicrosoftAuthServiceObserver*), (override));
  MOCK_METHOD0(ClearAuthData, void());
  MOCK_METHOD0(GetAuthState, MicrosoftAuthService::AuthState());
  MOCK_METHOD1(SetAccessToken, void(new_tab_page::mojom::AccessTokenPtr));
  MOCK_METHOD0(SetAuthStateError, void());
};

std::unique_ptr<TestingProfile> MakeTestingProfile() {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      MicrosoftAuthServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<testing::NiceMock<MockMicrosoftAuthService>>();
      }));
  return profile_builder.Build();
}

}  // namespace

class NtpMicrosoftAuthUntrustedPageHandlerTest : public testing::Test {
 public:
  NtpMicrosoftAuthUntrustedPageHandlerTest() : profile_(MakeTestingProfile()) {
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kNtpSharepointModuleVisible, base::Value(true));
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpMicrosoftAuthenticationModule,
                              ntp_features::kNtpSharepointModule},
        /*disabled_features=*/{});
    mock_auth_service_ = static_cast<MockMicrosoftAuthService*>(
        MicrosoftAuthServiceFactory::GetForProfile(profile_.get()));
    EXPECT_CALL(*mock_auth_service_, AddObserver)
        .WillOnce(testing::SaveArg<0>(&auth_service_observer_));
  }

  ~NtpMicrosoftAuthUntrustedPageHandlerTest() override = default;

  void SetUp() override {
    handler_ = std::make_unique<MicrosoftAuthUntrustedPageHandler>(
        mojo::PendingReceiver<
            new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler>(),
        mock_document_.BindAndGetRemote(), profile_.get());
  }

  MicrosoftAuthServiceObserver& auth_service_observer() {
    return *auth_service_observer_;
  }
  MicrosoftAuthUntrustedPageHandler& handler() { return *handler_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  MockMicrosoftAuthService& mock_auth_service() { return *mock_auth_service_; }
  MockDocument& mock_document() { return mock_document_; }

 private:
  testing::NiceMock<MockDocument> mock_document_;
  base::HistogramTester histogram_tester_;
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MicrosoftAuthUntrustedPageHandler> handler_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockMicrosoftAuthService> mock_auth_service_;
  raw_ptr<MicrosoftAuthServiceObserver> auth_service_observer_;
};

TEST_F(NtpMicrosoftAuthUntrustedPageHandlerTest, ClearAuthData) {
  EXPECT_CALL(mock_auth_service(), ClearAuthData);

  handler().ClearAuthData();
}

TEST_F(NtpMicrosoftAuthUntrustedPageHandlerTest, MaybeAcquireTokenSilent) {
  EXPECT_CALL(mock_document(), AcquireTokenSilent).Times(0);
  ON_CALL(mock_auth_service(), GetAuthState)
      .WillByDefault(
          testing::Return(MicrosoftAuthService::AuthState::kSuccess));

  handler().MaybeAcquireTokenSilent();
  histogram_tester().ExpectBucketCount("NewTabPage.MicrosoftAuth.AuthStarted",
                                       new_tab_page::mojom::AuthType::kSilent,
                                       0);
}

TEST_F(NtpMicrosoftAuthUntrustedPageHandlerTest, MaybeAcquireTokenSilentNone) {
  EXPECT_CALL(mock_document(), AcquireTokenSilent);
  ON_CALL(mock_auth_service(), GetAuthState)
      .WillByDefault(testing::Return(MicrosoftAuthService::AuthState::kNone));

  handler().MaybeAcquireTokenSilent();
  histogram_tester().ExpectBucketCount("NewTabPage.MicrosoftAuth.AuthStarted",
                                       new_tab_page::mojom::AuthType::kSilent,
                                       1);
}

TEST_F(NtpMicrosoftAuthUntrustedPageHandlerTest, SetAccessToken) {
  new_tab_page::mojom::AccessTokenPtr access_token_arg;
  EXPECT_CALL(mock_auth_service(), SetAccessToken)
      .WillOnce(MoveArg<0>(&access_token_arg));

  new_tab_page::mojom::AccessTokenPtr access_token =
      new_tab_page::mojom::AccessToken::New();
  access_token->token = "1234";
  access_token->expiration = base::Time::Now();
  handler().SetAccessToken(std::move(access_token));

  ASSERT_TRUE(access_token_arg);
  EXPECT_EQ(access_token_arg->token, "1234");
  EXPECT_EQ(access_token_arg->expiration, base::Time::Now());
}

TEST_F(NtpMicrosoftAuthUntrustedPageHandlerTest, SetAuthStateError) {
  EXPECT_CALL(mock_auth_service(), SetAuthStateError);

  handler().SetAuthStateError("test_error", "Error message.");
  histogram_tester().ExpectBucketCount("NewTabPage.MicrosoftAuth.AuthError",
                                       base::PersistentHash("test_error"), 1);
}

TEST_F(NtpMicrosoftAuthUntrustedPageHandlerTest, OnAuthStateUpdated) {
  ON_CALL(mock_auth_service(), GetAuthState)
      .WillByDefault(testing::Return(MicrosoftAuthService::AuthState::kNone));
  EXPECT_CALL(mock_document(), AcquireTokenSilent);

  auth_service_observer().OnAuthStateUpdated();
  histogram_tester().ExpectBucketCount("NewTabPage.MicrosoftAuth.AuthStarted",
                                       new_tab_page::mojom::AuthType::kSilent,
                                       1);
}
