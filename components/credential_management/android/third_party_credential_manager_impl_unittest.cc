// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/android/third_party_credential_manager_impl.h"

#include "base/test/mock_callback.h"
#include "components/credential_management/android/third_party_credential_manager_bridge.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {
const std::u16string kTestUsername = u"username";
const std::u16string kTestPassword = u"password";
const std::string kTestOrigin = "https://origin.com";
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
              (bool is_auto_select_allowed, bool include_passwords, const std::string&, GetCallback),
              (override));
  MOCK_METHOD(void,
              Store,
              (const std::u16string&,
               const std::u16string&,
               const std::string&,
               StoreCallback),
              (override));
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
        base::PassKey<class ThirdPartyCredentialManagerImplTest>(), main_rfh(),
        std::move(mock_bridge));
  }

  ThirdPartyCredentialManagerImpl* credential_manager() {
    return credential_manager_.get();
  }
  MockThirdPartyCredentialManagerBridge* mock_bridge() { return mock_bridge_; }

 private:
  raw_ptr<MockThirdPartyCredentialManagerBridge> mock_bridge_;
  std::unique_ptr<ThirdPartyCredentialManagerImpl> credential_manager_;
};

TEST_F(ThirdPartyCredentialManagerImplTest, TestStore) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestOrigin));

  EXPECT_CALL(*mock_bridge(),
              Store(kTestUsername, kTestPassword, kTestOrigin, _));
  password_manager::CredentialInfo info = password_manager::CredentialInfo(
      ::password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      /*id=*/kTestUsername,
      /*name=*/kTestUsername,
      /*icon=*/GURL(),
      /*password=*/kTestPassword,
      /*federation=*/url::SchemeHostPort(GURL()));

  credential_manager()->Store(info, StoreCallback());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestGetWithOptionalMediation) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestOrigin));

  EXPECT_CALL(*mock_bridge(), Get(/*mediation=*/true, /*include_passwords=*/true, kTestOrigin, _));

  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::kOptional,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), GetCallback());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestGetWithRequiredMediation) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestOrigin));

  EXPECT_CALL(*mock_bridge(), Get(/*mediation=*/false, /*include_passwords=*/true, kTestOrigin, _));

  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::kRequired,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), GetCallback());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestGetWithSilentMediation) {
  base::MockCallback<GetCallback> mock_get_callback;
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestOrigin));

  EXPECT_CALL(mock_get_callback,
              Run(password_manager::CredentialManagerError::UNKNOWN,
                  testing::Eq(std::nullopt)));

  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::kSilent,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), mock_get_callback.Get());
}

TEST_F(ThirdPartyCredentialManagerImplTest, TestGetWithConditionalMediation) {
  base::MockCallback<GetCallback> mock_get_callback;
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestOrigin));

  EXPECT_CALL(mock_get_callback,
              Run(password_manager::CredentialManagerError::UNKNOWN,
                  testing::Eq(std::nullopt)));

  credential_manager()->Get(
      /*mediation=*/password_manager::CredentialMediationRequirement::
          kConditional,
      /*include_passwords=*/true,
      /*federations=*/std::vector<GURL>(), mock_get_callback.Get());
}
}  // namespace credential_management
