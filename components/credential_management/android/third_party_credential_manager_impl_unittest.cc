// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/android/third_party_credential_manager_impl.h"

#include "base/test/mock_callback.h"
#include "components/credential_management/android/third_party_credential_manager_bridge.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/browser/visibility.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Ref;

namespace {
const std::u16string kTestUsername = u"username";
const std::u16string kTestPassword = u"password";
const char kTestOrigin[] = "https://origin.com";
}  // namespace

namespace credential_management {

using StoreCallback = base::OnceCallback<void()>;
using GetCallback = base::OnceCallback<void(
    password_manager::CredentialManagerError,
    const std::optional<password_manager::CredentialInfo>&)>;

class MockThirdPartyCredentialManagerBridge : public CredentialManagerBridge {
 public:
  MockThirdPartyCredentialManagerBridge() = default;
  ~MockThirdPartyCredentialManagerBridge() override = default;

  MOCK_METHOD(void,
              Get,
              (content::WebContents&,
               bool is_auto_select_allowed,
               bool include_passwords,
               const std::vector<GURL>& federations,
               const std::string&,
               GetCallback),
              (override));
  MOCK_METHOD(void,
              Store,
              (content::WebContents&,
               const std::u16string&,
               const std::u16string&,
               const std::string&,
               StoreCallback),
              (override));
  MOCK_METHOD(void, Cancel, (), (override));
};

class ThirdPartyCredentialManagerImplTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    auto mock_bridge =
        std::make_unique<MockThirdPartyCredentialManagerBridge>();
    mock_bridge_ = mock_bridge.get();

    credential_manager_ = std::make_unique<ThirdPartyCredentialManagerImpl>(
        base::PassKey<class ThirdPartyCredentialManagerImplTest>(),
        web_contents(), std::move(mock_bridge));
  }

  ThirdPartyCredentialManagerImpl* credential_manager() {
    return credential_manager_.get();
  }
  MockThirdPartyCredentialManagerBridge* mock_bridge() { return mock_bridge_; }

  void NavigateToTestOrigin() {
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL(kTestOrigin));
  }

 private:
  raw_ptr<MockThirdPartyCredentialManagerBridge> mock_bridge_;
  std::unique_ptr<ThirdPartyCredentialManagerImpl> credential_manager_;
};

TEST_F(ThirdPartyCredentialManagerImplTest, TestStore) {
  NavigateToTestOrigin();

  EXPECT_CALL(*mock_bridge(), Store(Ref(*web_contents()), kTestUsername,
                                    kTestPassword, kTestOrigin, _));
  password_manager::CredentialInfo info = password_manager::CredentialInfo(
      ::password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      /*id=*/kTestUsername,
      /*name=*/kTestUsername,
      /*icon=*/GURL(),
      /*password=*/kTestPassword,
      /*federation=*/url::SchemeHostPort(GURL()));

  credential_manager()->Store(info, StoreCallback());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestDoesntStoreEmptyCredentials) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestOrigin));

  EXPECT_CALL(*mock_bridge(), Store(_, _, _, kTestOrigin, _)).Times(0);
  password_manager::CredentialInfo info = password_manager::CredentialInfo(
      password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY,
      /*id=*/u"",
      /*name=*/u"",
      /*icon=*/GURL(),
      /*password=*/u"",
      /*federation=*/url::SchemeHostPort(GURL()));
  base::MockCallback<StoreCallback> mock_store_callback;
  EXPECT_CALL(mock_store_callback, Run());

  // The callback should be called immediately with an empty credential that
  // won't be stored.
  credential_manager()->Store(info, mock_store_callback.Get());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestDoesntStoreEmptyPasswords) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestOrigin));

  EXPECT_CALL(*mock_bridge(), Store(_, _, _, kTestOrigin, _)).Times(0);
  password_manager::CredentialInfo info = password_manager::CredentialInfo(
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      /*id=*/kTestUsername,
      /*name=*/kTestUsername,
      /*icon=*/GURL(),
      /*password=*/u"",
      /*federation=*/url::SchemeHostPort(GURL()));
  base::MockCallback<StoreCallback> mock_store_callback;
  EXPECT_CALL(mock_store_callback, Run());

  // The callback should be called immediately with an empty password that won't
  // be stored.
  credential_manager()->Store(info, mock_store_callback.Get());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestStoresEmptyUsernames) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestOrigin));

  EXPECT_CALL(*mock_bridge(), Store(_, _, _, kTestOrigin, _));
  password_manager::CredentialInfo info = password_manager::CredentialInfo(
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      /*id=*/u"",
      /*name=*/u"",
      /*icon=*/GURL(),
      /*password=*/kTestPassword,
      /*federation=*/url::SchemeHostPort(GURL()));

  credential_manager()->Store(info, StoreCallback());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestGetWithOptionalMediation) {
  NavigateToTestOrigin();

  EXPECT_CALL(*mock_bridge(),
              Get(Ref(*web_contents()), /*is_auto_select_allowed=*/true,
                  /*include_passwords=*/true,
                  /*federations=*/std::vector<GURL>(), kTestOrigin, _));

  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::kOptional,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), GetCallback());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestGetWithRequiredMediation) {
  NavigateToTestOrigin();

  EXPECT_CALL(*mock_bridge(),
              Get(Ref(*web_contents()), /*is_auto_select_allowed=*/false,
                  /*include_passwords=*/true,
                  /*federations=*/std::vector<GURL>(), kTestOrigin, _));

  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::kRequired,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), GetCallback());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestGetWithSilentMediation) {
  base::MockCallback<GetCallback> mock_get_callback;
  NavigateToTestOrigin();

  EXPECT_CALL(mock_get_callback,
              Run(password_manager::CredentialManagerError::SUCCESS,
                  Eq(password_manager::CredentialInfo())));

  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::kSilent,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), mock_get_callback.Get());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestGetWithConditionalMediation) {
  base::MockCallback<GetCallback> mock_get_callback;
  NavigateToTestOrigin();

  EXPECT_CALL(mock_get_callback,
              Run(password_manager::CredentialManagerError::SUCCESS,
                  Eq(password_manager::CredentialInfo())));

  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::
          kConditional,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), mock_get_callback.Get());
}

class ThirdPartyCredentialManagerImplIncognitoModeTest
    : public ThirdPartyCredentialManagerImplTest {
 public:
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    auto browser_context = std::make_unique<content::TestBrowserContext>();
    browser_context->set_is_off_the_record(true);
    return std::move(browser_context);
  }
};

TEST_F(ThirdPartyCredentialManagerImplIncognitoModeTest,
       TestGetWithIncognitoMode) {
  base::MockCallback<GetCallback> mock_get_callback;
  NavigateToTestOrigin();

  // In incognito mode, Get should return immediately with an empty credential.
  EXPECT_CALL(mock_get_callback,
              Run(password_manager::CredentialManagerError::SUCCESS,
                  Eq(password_manager::CredentialInfo())));

  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::kRequired,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), mock_get_callback.Get());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestGetWhenInvisible) {
  NavigateToTestOrigin();

  // Simulate the WebContents being hidden.
  web_contents()->WasHidden();
  ASSERT_EQ(content::Visibility::HIDDEN, web_contents()->GetVisibility());

  // We expect that Get will NOT call the bridge.
  EXPECT_CALL(*mock_bridge(), Get).Times(0);

  base::MockCallback<GetCallback> mock_get_callback;
  // We expect the callback to be called with SUCCESS and empty CredentialInfo.
  EXPECT_CALL(mock_get_callback,
              Run(password_manager::CredentialManagerError::SUCCESS,
                  Eq(password_manager::CredentialInfo())));

  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::kRequired,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), mock_get_callback.Get());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestStoreWhenInvisible) {
  NavigateToTestOrigin();

  // Simulate the WebContents being hidden.
  web_contents()->WasHidden();
  ASSERT_EQ(content::Visibility::HIDDEN, web_contents()->GetVisibility());

  // We expect that Store will NOT call the bridge.
  EXPECT_CALL(*mock_bridge(), Store).Times(0);

  base::MockCallback<StoreCallback> mock_store_callback;
  // We expect the callback to be called (acknowledge).
  EXPECT_CALL(mock_store_callback, Run());

  password_manager::CredentialInfo info = password_manager::CredentialInfo(
      ::password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      /*id=*/kTestUsername,
      /*name=*/kTestUsername,
      /*icon=*/GURL(),
      /*password=*/kTestPassword,
      /*federation=*/url::SchemeHostPort(GURL()));

  credential_manager()->Store(info, mock_store_callback.Get());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestGetThrottling) {
  NavigateToTestOrigin();

  // First call should go to the bridge.
  EXPECT_CALL(*mock_bridge(), Get).Times(1);

  base::MockCallback<GetCallback> mock_get_callback1;
  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::kRequired,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), mock_get_callback1.Get());

  // Second call should fail immediately with PENDING_REQUEST and NOT call the
  // bridge.
  base::MockCallback<GetCallback> mock_get_callback2;
  EXPECT_CALL(mock_get_callback2,
              Run(password_manager::CredentialManagerError::PENDING_REQUEST,
                  Eq(std::nullopt)));

  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::kRequired,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), mock_get_callback2.Get());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestStoreThrottling) {
  NavigateToTestOrigin();

  // First call should go to the bridge.
  EXPECT_CALL(*mock_bridge(), Store).Times(1);

  base::MockCallback<StoreCallback> mock_store_callback1;
  password_manager::CredentialInfo info = password_manager::CredentialInfo(
      ::password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      /*id=*/kTestUsername,
      /*name=*/kTestUsername,
      /*icon=*/GURL(),
      /*password=*/kTestPassword,
      /*federation=*/url::SchemeHostPort(GURL()));

  credential_manager()->Store(info, mock_store_callback1.Get());

  // Second call should fail (run callback) immediately and NOT call the bridge.
  base::MockCallback<StoreCallback> mock_store_callback2;
  EXPECT_CALL(mock_store_callback2, Run());

  credential_manager()->Store(info, mock_store_callback2.Get());
}

TEST_F(ThirdPartyCredentialManagerImplTest,
       TestResetAfterDisconnectingResetsThrottling) {
  NavigateToTestOrigin();

  // First call should go to the bridge.
  EXPECT_CALL(*mock_bridge(), Get).Times(1);

  base::MockCallback<GetCallback> mock_get_callback1;
  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::kRequired,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), mock_get_callback1.Get());

  // Disconnect should call Cancel (once for explicit disconnect, once for
  // destructor).
  EXPECT_CALL(*mock_bridge(), Cancel()).Times(2);
  credential_manager()->ResetAfterDisconnecting();

  // Second call should now ALSO go to the bridge because we reset.
  EXPECT_CALL(*mock_bridge(), Get).Times(1);

  base::MockCallback<GetCallback> mock_get_callback2;
  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::kRequired,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), mock_get_callback2.Get());
}

TEST_F(ThirdPartyCredentialManagerImplIncognitoModeTest,
       TestStoreWithIncognitoMode) {
  base::MockCallback<StoreCallback> mock_store_callback;
  NavigateToTestOrigin();

  // In incognito mode, Store should return immediately and NOT call the bridge.
  EXPECT_CALL(*mock_bridge(), Store).Times(0);
  EXPECT_CALL(mock_store_callback, Run());

  password_manager::CredentialInfo info = password_manager::CredentialInfo(
      ::password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      /*id=*/kTestUsername,
      /*name=*/kTestUsername,
      /*icon=*/GURL(),
      /*password=*/kTestPassword,
      /*federation=*/url::SchemeHostPort(GURL()));

  credential_manager()->Store(info, mock_store_callback.Get());
}

}  // namespace credential_management
