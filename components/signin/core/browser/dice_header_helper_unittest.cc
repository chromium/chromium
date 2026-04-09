// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_header_helper.h"

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/core/browser/dice_response_params.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace signin {

namespace {

constexpr char kTestDeviceId[] = "DeviceID";

class RequestAdapterWrapper {
 public:
  RequestAdapterWrapper(const GURL& url, const net::HttpRequestHeaders& headers)
      : adapter_(url,
                 headers,
                 &modified_request_headers_,
                 &to_be_removed_request_headers_),
        original_headers_(headers) {}

  RequestAdapter* adapter() { return &adapter_; }

  net::HttpRequestHeaders GetFinalHeaders() {
    net::HttpRequestHeaders final_headers(*original_headers_);
    final_headers.MergeFrom(modified_request_headers_);
    for (const std::string& name : to_be_removed_request_headers_) {
      final_headers.RemoveHeader(name);
    }
    return final_headers;
  }

 private:
  RequestAdapter adapter_;
  const raw_ref<const net::HttpRequestHeaders> original_headers_;
  net::HttpRequestHeaders modified_request_headers_;
  std::vector<std::string> to_be_removed_request_headers_;
};

}  // namespace

class DiceHeaderHelperTest : public testing::Test {
 protected:
  // Common constants for tests.
  const GaiaId kGaiaID = GaiaId("gaia_id");
  const std::string kEmail = "foo@example.com";
  const std::string kAuthorizationCode = "authorization_code";
  const std::string kSupportedTokenBindingAlgorithms = "ES256 RS256";
  const int kSessionIndex = 42;

  DiceHeaderHelperTest() {
    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    privacy_sandbox::RegisterProfilePrefs(prefs_.registry());

    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, false /* should_record_metrics */);
  }

  ~DiceHeaderHelperTest() override { settings_map_->ShutdownOnUIThread(); }

  void CheckDiceHeaderRequest(const GURL& url,
                              const GaiaId& gaia_id,
                              const std::string& expected_dice_request) {
    net::HttpRequestHeaders original_headers;
    RequestAdapterWrapper request_adapter(url, original_headers);
    DiceHeaderHelper::AppendOrRemoveDiceRequestHeader(
        request_adapter.adapter(), GURL(), gaia_id, sync_enabled_,
        account_consistency_, device_id_);
    net::HttpRequestHeaders headers = request_adapter.GetFinalHeaders();

    bool expected_result = !expected_dice_request.empty();
    EXPECT_THAT(headers.GetHeader(kDiceRequestHeader),
                testing::Conditional(expected_result,
                                     testing::Optional(expected_dice_request),
                                     std::nullopt));
  }

  void VerifySigninAccount(
      const DiceResponseParams::SigninInfo::SigninAccount& account,
      const GaiaId& expected_gaia_id,
      const std::string& expected_email,
      int expected_session_index,
      const std::string& expected_auth_code,
      bool expected_no_auth_code = false,
      const std::string& expected_supported_tb_algorithms = "",
      bool expected_mtls_token_binding = false) {
    EXPECT_EQ(expected_gaia_id, account.account_info.gaia_id);
    EXPECT_EQ(expected_email, account.account_info.email);
    EXPECT_EQ(expected_session_index, account.account_info.session_index);
    EXPECT_EQ(expected_auth_code, account.authorization_code);
    EXPECT_EQ(expected_no_auth_code, account.no_authorization_code);
    EXPECT_EQ(expected_supported_tb_algorithms,
              account.supported_algorithms_for_token_binding);
    EXPECT_EQ(expected_mtls_token_binding, account.mtls_token_binding);
  }

  base::test::TaskEnvironment task_environment_;
  bool sync_enabled_ = false;
  AccountConsistencyMethod account_consistency_ =
      AccountConsistencyMethod::kDisabled;
  std::string device_id_ = kTestDeviceId;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
};

TEST_F(DiceHeaderHelperTest, BuildDiceSigninResponseParams_Invalid) {
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams("blah");
  EXPECT_FALSE(params.IsValid());
  EXPECT_EQ(DiceAction::NONE, params.user_intention());
}

TEST_F(DiceHeaderHelperTest, BuildDiceSignoutResponseParams_Invalid) {
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSignoutResponseParams("blah");
  EXPECT_FALSE(params.IsValid());
  EXPECT_EQ(DiceAction::SIGNOUT, params.user_intention());
}

TEST_F(DiceHeaderHelperTest, CreateDiceResponseParams_Signin) {
  base::HistogramTester histogram_tester;
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n");
  headers->AddHeader(
      kDiceResponseHeader,
      base::StringPrintf("action=SIGNIN,id=%s,email=%s,authuser=%d,"
                         "authorization_code=%s,"
                         "eligible_for_token_binding=%s",
                         kGaiaID.ToString().c_str(), kEmail.c_str(),
                         kSessionIndex, kAuthorizationCode.c_str(),
                         kSupportedTokenBindingAlgorithms.c_str()));
  DiceResponseParams params =
      DiceHeaderHelper::CreateDiceResponseParams(headers.get());
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  const auto* account = signin_info->GetInitiator();
  ASSERT_TRUE(account);

  {
    SCOPED_TRACE("Verifying Signin Account");
    VerifySigninAccount(*account, kGaiaID, kEmail, kSessionIndex,
                        kAuthorizationCode, /*expected_no_auth_code=*/false,
                        kSupportedTokenBindingAlgorithms);
  }
  histogram_tester.ExpectUniqueSample("Signin.DiceAuthorizationCode", true, 1);
}

TEST_F(DiceHeaderHelperTest, BuildDiceSigninResponseParams_EnableSync) {
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
          "action=ENABLE_SYNC,id=%s,email=%s,authuser=%i",
          kGaiaID.ToString().c_str(), kEmail.c_str(), kSessionIndex));
  EXPECT_EQ(DiceAction::ENABLE_SYNC, params.user_intention());
  const auto* enable_sync_info = params.enable_sync_info();
  ASSERT_TRUE(enable_sync_info);
  EXPECT_EQ(kGaiaID, enable_sync_info->account_info.gaia_id);
  EXPECT_EQ(kEmail, enable_sync_info->account_info.email);
  EXPECT_EQ(kSessionIndex, enable_sync_info->account_info.session_index);
}

TEST_F(DiceHeaderHelperTest,
       BuildDiceSigninResponseParams_NoAuthorizationCode) {
  base::HistogramTester histogram_tester;
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
          "action=SIGNIN,id=%s,email=%s,authuser=%i,"
          "no_authorization_code=true",
          kGaiaID.ToString().c_str(), kEmail.c_str(), kSessionIndex));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  const auto* account = signin_info->GetInitiator();
  ASSERT_TRUE(account);

  {
    SCOPED_TRACE("Verifying No Auth Code Account");
    VerifySigninAccount(*account, kGaiaID, kEmail, kSessionIndex,
                        /*expected_auth_code=*/"",
                        /*expected_no_auth_code=*/true);
  }
  histogram_tester.ExpectUniqueSample("Signin.DiceAuthorizationCode", false, 1);
}

TEST_F(
    DiceHeaderHelperTest,
    BuildDiceSigninResponseParams_MissingAuthorizationCodeAndNoAuthorizationCode) {
  base::HistogramTester histogram_tester;
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
          "action=SIGNIN,id=%s,email=%s,authuser=%i",
          kGaiaID.ToString().c_str(), kEmail.c_str(), kSessionIndex));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  EXPECT_FALSE(params.IsValid());
  histogram_tester.ExpectTotalCount("Signin.DiceAuthorizationCode", 0);
}

TEST_F(DiceHeaderHelperTest, BuildDiceSigninResponseParams_MissingEmail) {
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
          "action=SIGNIN,id=%s,authuser=%i,authorization_code=%s",
          kGaiaID.ToString().c_str(), kSessionIndex,
          kAuthorizationCode.c_str()));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  EXPECT_FALSE(params.IsValid());
}

TEST_F(DiceHeaderHelperTest, BuildDiceSigninResponseParams_MtlsTokenBinding) {
  base::test::ScopedFeatureList scoped_feature_list(
      switches::kEnableMtlsTokenBinding);
  base::HistogramTester histogram_tester;
  DiceResponseParams params = DiceHeaderHelper::BuildDiceSigninResponseParams(
      base::StringPrintf("action=SIGNIN,id=%s,email=%s,authuser=%i,"
                         "authorization_code=%s,"
                         "eligible_for_token_binding=%s,"
                         "mtls_token_binding=true",
                         kGaiaID.ToString().c_str(), kEmail.c_str(),
                         kSessionIndex, kAuthorizationCode.c_str(),
                         kSupportedTokenBindingAlgorithms.c_str()));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  const auto* account = signin_info->GetInitiator();
  ASSERT_TRUE(account);

  {
    SCOPED_TRACE("Verifying MTLS Token Binding Account");
    VerifySigninAccount(*account, kGaiaID, kEmail, kSessionIndex,
                        kAuthorizationCode, /*expected_no_auth_code=*/false,
                        kSupportedTokenBindingAlgorithms,
                        /*expected_mtls_token_binding=*/true);
  }
  histogram_tester.ExpectUniqueSample("Signin.DiceAuthorizationCode", true, 1);
}

TEST_F(DiceHeaderHelperTest,
       BuildDiceSigninResponseParamsWithMtlsTokenBindingFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(switches::kEnableMtlsTokenBinding);

  DiceResponseParams params = DiceHeaderHelper::BuildDiceSigninResponseParams(
      base::StringPrintf("action=SIGNIN,id=%s,email=%s,authuser=%i,"
                         "authorization_code=%s,"
                         "eligible_for_token_binding=%s,"
                         "mtls_token_binding=true",
                         kGaiaID.ToString().c_str(), kEmail, kSessionIndex,
                         kAuthorizationCode, kSupportedTokenBindingAlgorithms));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  const auto* account = signin_info->GetInitiator();
  ASSERT_TRUE(account);
  EXPECT_FALSE(account->mtls_token_binding);
}

TEST_F(DiceHeaderHelperTest, BuildDiceSignoutResponseParams) {
  DiceResponseParams params = DiceHeaderHelper::BuildDiceSignoutResponseParams(
      base::StringPrintf("email=%s, sessionindex=%i, obfuscatedid=%s", kEmail,
                         kSessionIndex, kGaiaID.ToString().c_str()));
  ASSERT_EQ(DiceAction::SIGNOUT, params.user_intention());
  const auto* signout_info = params.signout_info();
  ASSERT_TRUE(signout_info);
  EXPECT_EQ(1u, signout_info->account_infos.size());
  EXPECT_EQ(kGaiaID, signout_info->account_infos[0].gaia_id);
  EXPECT_EQ(kEmail, signout_info->account_infos[0].email);
  EXPECT_EQ(kSessionIndex, signout_info->account_infos[0].session_index);
}

TEST_F(DiceHeaderHelperTest, BuildDiceSignoutResponseParams_MultipleAccounts) {
  // some fields are wrapped in quotes.
  DiceResponseParams params = DiceHeaderHelper::BuildDiceSignoutResponseParams(
      base::StringPrintf("email=\"%s\", sessionindex=%i, obfuscatedid=\"%s\"",
                         kEmail, kSessionIndex, kGaiaID.ToString().c_str()));
  ASSERT_EQ(DiceAction::SIGNOUT, params.user_intention());
  const auto* signout_info = params.signout_info();
  ASSERT_TRUE(signout_info);
  EXPECT_EQ(1u, signout_info->account_infos.size());
  EXPECT_EQ(kGaiaID, signout_info->account_infos[0].gaia_id);
  EXPECT_EQ(kEmail, signout_info->account_infos[0].email);
  EXPECT_EQ(kSessionIndex, signout_info->account_infos[0].session_index);
}

TEST_F(DiceHeaderHelperTest, BuildDiceSignoutResponseParams_MultiSignout) {
  const char kEmail2[] = "bar@example.com";
  const GaiaId kGaiaID2("gaia_id_2");
  const int kSessionIndex2 = 2;
  DiceResponseParams params = DiceHeaderHelper::BuildDiceSignoutResponseParams(
      base::StringPrintf("email=\"%s\", sessionindex=%i, obfuscatedid=\"%s\", "
                         "email=\"%s\", sessionindex=%i, obfuscatedid=\"%s\"",
                         kEmail, kSessionIndex, kGaiaID.ToString().c_str(),
                         kEmail2, kSessionIndex2, kGaiaID2.ToString().c_str()));
  ASSERT_EQ(DiceAction::SIGNOUT, params.user_intention());
  const auto* signout_info = params.signout_info();
  ASSERT_TRUE(signout_info);
  EXPECT_EQ(2u, signout_info->account_infos.size());
  EXPECT_EQ(kGaiaID, signout_info->account_infos[0].gaia_id);
  EXPECT_EQ(kEmail, signout_info->account_infos[0].email);
  EXPECT_EQ(kSessionIndex, signout_info->account_infos[0].session_index);
  EXPECT_EQ(kGaiaID2, signout_info->account_infos[1].gaia_id);
  EXPECT_EQ(kEmail2, signout_info->account_infos[1].email);
  EXPECT_EQ(kSessionIndex2, signout_info->account_infos[1].session_index);
}

TEST_F(DiceHeaderHelperTest, BuildDiceSignoutResponseParams_MissingEmail) {
  DiceResponseParams params = DiceHeaderHelper::BuildDiceSignoutResponseParams(
      base::StringPrintf("email=%s, sessionindex=%i, obfuscatedid=%s, "
                         "sessionindex=2, obfuscatedid=bar",
                         kEmail, kSessionIndex, kGaiaID.ToString().c_str()));
  EXPECT_EQ(DiceAction::SIGNOUT, params.user_intention());
  EXPECT_FALSE(params.IsValid());
}

TEST_F(DiceHeaderHelperTest,
       BuildDiceSigninResponseParams_NotEligibleForTokenBinding) {
  const char kAuthorizationCode[] = "authorization_code";
  const char kEmail[] = "foo@example.com";
  const GaiaId kGaiaID("gaia_id");
  const int kSessionIndex = 42;

  // "eligible_for_token_binding" is missing.
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
          "action=SIGNIN,id=%s,email=%s,authuser=%i,authorization_code=%s",
          kGaiaID.ToString().c_str(), kEmail, kSessionIndex,
          kAuthorizationCode));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  ASSERT_EQ(1u, signin_info->accounts().size());
  EXPECT_TRUE(signin_info->GetInitiator()
                  ->supported_algorithms_for_token_binding.empty());
}

TEST_F(DiceHeaderHelperTest, BuildDiceSigninResponseParams_MixedOrderComma) {
  DiceResponseParams params = DiceHeaderHelper::BuildDiceSigninResponseParams(
      base::StringPrintf("id=%s,action=SIGNIN,authuser=%i,eligible_for_token_"
                         "binding=%s,email=%s,authorization_code=%s",
                         kGaiaID.ToString().c_str(), kSessionIndex,
                         kSupportedTokenBindingAlgorithms.c_str(),
                         kEmail.c_str(), kAuthorizationCode.c_str()));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  const auto* account = signin_info->GetInitiator();
  ASSERT_TRUE(account);

  {
    SCOPED_TRACE("Verifying Mixed Order Comma Account");
    VerifySigninAccount(*account, kGaiaID, kEmail, kSessionIndex,
                        kAuthorizationCode, /*expected_no_auth_code=*/false,
                        kSupportedTokenBindingAlgorithms);
  }
}

TEST_F(DiceHeaderHelperTest,
       BuildDiceSigninResponseParams_MixedOrderSemicolon) {
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
          "id=%s;action=SIGNIN;authuser=%i;eligible_for_token_binding=%s;"
          "email=%s;authorization_code=%s",
          kGaiaID.ToString().c_str(), kSessionIndex,
          kSupportedTokenBindingAlgorithms.c_str(), kEmail.c_str(),
          kAuthorizationCode.c_str()));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  const auto* account = signin_info->GetInitiator();
  ASSERT_TRUE(account);

  {
    SCOPED_TRACE("Verifying Mixed Order Semicolon Account");
    VerifySigninAccount(*account, kGaiaID, kEmail, kSessionIndex,
                        kAuthorizationCode, /*expected_no_auth_code=*/false,
                        kSupportedTokenBindingAlgorithms);
  }
}


TEST_F(DiceHeaderHelperTest,
       BuildDiceSigninResponseParamsSemicolon_EnableSync) {
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
          "action=ENABLE_SYNC;id=%s;email=%s;authuser=%i",
          kGaiaID.ToString().c_str(), kEmail.c_str(), kSessionIndex));
  EXPECT_EQ(DiceAction::ENABLE_SYNC, params.user_intention());
  const auto* enable_sync_info = params.enable_sync_info();
  ASSERT_TRUE(enable_sync_info);
  EXPECT_EQ(kGaiaID, enable_sync_info->account_info.gaia_id);
  EXPECT_EQ(kEmail, enable_sync_info->account_info.email);
  EXPECT_EQ(kSessionIndex, enable_sync_info->account_info.session_index);
}

TEST_F(DiceHeaderHelperTest,
       BuildDiceSigninResponseParamsSemicolon_NoAuthorizationCode) {
  base::HistogramTester histogram_tester;
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
          "action=SIGNIN;id=%s;email=%s;authuser=%i;"
          "no_authorization_code=true",
          kGaiaID.ToString().c_str(), kEmail.c_str(), kSessionIndex));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  const auto* account = signin_info->GetInitiator();
  ASSERT_TRUE(account);

  {
    SCOPED_TRACE("Verifying Semicolon No Auth Code Account");
    VerifySigninAccount(*account, kGaiaID, kEmail, kSessionIndex,
                        /*expected_auth_code=*/"",
                        /*expected_no_auth_code=*/true);
  }
  histogram_tester.ExpectUniqueSample("Signin.DiceAuthorizationCode", false, 1);
}

TEST_F(
    DiceHeaderHelperTest,
    BuildDiceSigninResponseParamsSemicolon_MissingAuthorizationCodeAndNoAuthorizationCode) {
  base::HistogramTester histogram_tester;
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
          "action=SIGNIN;id=%s;email=%s;authuser=%i",
          kGaiaID.ToString().c_str(), kEmail.c_str(), kSessionIndex));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  EXPECT_FALSE(params.IsValid());
  histogram_tester.ExpectTotalCount("Signin.DiceAuthorizationCode", 0);
}

TEST_F(DiceHeaderHelperTest,
       BuildDiceSigninResponseParamsSemicolon_MissingEmail) {
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
          "action=SIGNIN;id=%s;authuser=%i;authorization_code=%s",
          kGaiaID.ToString().c_str(), kSessionIndex,
          kAuthorizationCode.c_str()));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  EXPECT_FALSE(params.IsValid());
}

TEST_F(DiceHeaderHelperTest, BuildDiceSigninResponseParamsSemicolon_AllFields) {
  base::test::ScopedFeatureList scoped_feature_list(
      switches::kEnableMtlsTokenBinding);
  DiceResponseParams params = DiceHeaderHelper::BuildDiceSigninResponseParams(
      base::StringPrintf("action=SIGNIN;id=%s;email=%s;authuser=%i;"
                         "authorization_code=%s;"
                         "eligible_for_token_binding=%s;"
                         "mtls_token_binding=true",
                         kGaiaID.ToString().c_str(), kEmail.c_str(),
                         kSessionIndex, kAuthorizationCode.c_str(),
                         kSupportedTokenBindingAlgorithms.c_str()));

  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  const auto* account = signin_info->GetInitiator();
  ASSERT_TRUE(account);

  {
    SCOPED_TRACE("Verifying Semicolon All Fields Account");
    VerifySigninAccount(*account, kGaiaID, kEmail, kSessionIndex,
                        kAuthorizationCode, /*expected_no_auth_code=*/false,
                        kSupportedTokenBindingAlgorithms,
                        /*expected_mtls_token_binding=*/true);
  }
}

TEST_F(DiceHeaderHelperTest,
       BuildDiceSigninResponseParamsMultiAccount_MissingData) {
  // Multi-Account Format: Comma between accounts, semicolon within accounts.
  // Missing data.
  DiceResponseParams params = DiceHeaderHelper::BuildDiceSigninResponseParams(
      base::StringPrintf("action=SIGNIN;id=other_id;email=other_email,"
                         "action=SIGNIN;id=%s;email=%s;authorization_code=%s",
                         kGaiaID.ToString().c_str(), kEmail.c_str(),
                         kAuthorizationCode.c_str()));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  ASSERT_EQ(2u, signin_info->accounts().size());
  EXPECT_EQ(GaiaId("other_id"),
            signin_info->accounts()[0].account_info.gaia_id);
  EXPECT_EQ(kGaiaID, signin_info->accounts()[1].account_info.gaia_id);
  EXPECT_FALSE(params.IsValid());
}

TEST_F(DiceHeaderHelperTest, CreateDiceResponseParams_NullHeaders) {
  DiceResponseParams params =
      DiceHeaderHelper::CreateDiceResponseParams(nullptr);
  EXPECT_FALSE(params.IsValid());
}

TEST_F(DiceHeaderHelperTest, CreateDiceResponseParams_SigninHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n");
  headers->AddHeader(
      kDiceResponseHeader,
      base::StringPrintf("action=SIGNIN;id=%s;email=%s;authuser=%d;"
                         "authorization_code=%s",
                         kGaiaID.ToString().c_str(), kEmail.c_str(),
                         kSessionIndex, kAuthorizationCode.c_str()));
  DiceResponseParams params =
      DiceHeaderHelper::CreateDiceResponseParams(headers.get());
  EXPECT_TRUE(params.IsValid());
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  ASSERT_EQ(1u, signin_info->accounts().size());
  VerifySigninAccount(signin_info->accounts()[0], kGaiaID, kEmail,
                      kSessionIndex, kAuthorizationCode);
}

TEST_F(DiceHeaderHelperTest,
       CreateDiceResponseParams_MultipleAccountsAndMetaHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n");
  headers->AddHeader(
      kDiceResponseHeader,
      "action=SIGNIN;id=id1;email=email1;authuser=1;authorization_code=code1,"
      "action=SIGNIN;id=id2;email=email2;authuser=2;authorization_code=code2");
  headers->AddHeader(kDiceLinkedAccountsMetaHeader,
                     "initiator_id=id2;primary_is_connected=1");

  DiceResponseParams params =
      DiceHeaderHelper::CreateDiceResponseParams(headers.get());

  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  const auto& accounts = signin_info->accounts();
  ASSERT_EQ(2u, accounts.size());

  {
    SCOPED_TRACE("Verifying Account 1");
    VerifySigninAccount(accounts[0], GaiaId("id1"), "email1", 1, "code1");
  }
  {
    SCOPED_TRACE("Verifying Account 2");
    VerifySigninAccount(accounts[1], GaiaId("id2"), "email2", 2, "code2");
  }

  const auto* initiator = signin_info->GetInitiator();
  ASSERT_TRUE(initiator);
  EXPECT_EQ(GaiaId("id2"), initiator->account_info.gaia_id);
  EXPECT_EQ(Tribool::kTrue,
            signin_info->linked_accounts_metadata().primary_is_connected);
}

TEST_F(DiceHeaderHelperTest, CreateDiceResponseParams_MultiAccountAllFields) {
  base::test::ScopedFeatureList scoped_feature_list(
      switches::kEnableMtlsTokenBinding);
  const GaiaId kGaiaID1("id1");
  const GaiaId kGaiaID2("id2");
  const std::string kSupportedAlgorithms2 = "ES256";

  std::string header_value = base::StringPrintf(
      "action=SIGNIN;id=%s;email=email1;authuser=1;authorization_code=code1,"
      "action=SIGNIN;id=%s;email=email2;authuser=2;authorization_code=code2;"
      "eligible_for_token_binding=%s;mtls_token_binding=true",
      kGaiaID1.ToString().c_str(), kGaiaID2.ToString().c_str(),
      kSupportedAlgorithms2.c_str());

  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n");
  headers->AddHeader(kDiceResponseHeader, header_value);
  headers->AddHeader(
      kDiceLinkedAccountsMetaHeader,
      base::StringPrintf("initiator_id=%s", kGaiaID2.ToString().c_str()));
  DiceResponseParams params =
      DiceHeaderHelper::CreateDiceResponseParams(headers.get());

  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  const auto* signin_info = params.signin_info();
  ASSERT_TRUE(signin_info);
  const auto& accounts = signin_info->accounts();
  ASSERT_EQ(2u, accounts.size());

  {
    SCOPED_TRACE("Verifying Account 1 (Basic)");
    VerifySigninAccount(accounts[0], kGaiaID1, "email1", 1, "code1");
  }
  {
    SCOPED_TRACE("Verifying Account 2 (All Fields)");
    VerifySigninAccount(accounts[1], kGaiaID2, "email2", 2, "code2",
                        /*expected_no_auth_code=*/false, kSupportedAlgorithms2,
                        /*expected_mtls_token_binding=*/true);
  }

  const auto* initiator = signin_info->GetInitiator();
  ASSERT_TRUE(initiator);
  EXPECT_EQ(kGaiaID2, initiator->account_info.gaia_id);
}

TEST_F(DiceHeaderHelperTest, CreateDiceResponseParams_MultiAccountDuplicates) {
  std::string header_value = base::StringPrintf(
      "action=SIGNIN;id=%s;email=%s;authuser=1;authorization_code=code1,"
      "action=SIGNIN;id=%s;email=%s;authuser=2;authorization_code=code2",
      kGaiaID.ToString().c_str(), kEmail.c_str(), kGaiaID.ToString().c_str(),
      kEmail.c_str());
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n");
  headers->AddHeader(kDiceResponseHeader, header_value);
  headers->AddHeader(
      kDiceLinkedAccountsMetaHeader,
      base::StringPrintf("initiator_id=%s", kGaiaID.ToString().c_str()));
  DiceResponseParams params =
      DiceHeaderHelper::CreateDiceResponseParams(headers.get());

  EXPECT_EQ(1U, params.signin_info()->accounts().size());
  {
    SCOPED_TRACE("Verifying Preserved Account");
    VerifySigninAccount(params.signin_info()->accounts()[0], kGaiaID, kEmail, 1,
                        "code1");
  }
  EXPECT_TRUE(params.IsValid());
}

TEST_F(DiceHeaderHelperTest,
       CreateDiceResponseParams_MultiAccountTrailingSemicolon) {
  std::string header_value = base::StringPrintf(
      "action=SIGNIN;id=id1;email=email1;authuser=1;authorization_code=code1,"
      "action=SIGNIN;id=id2;email=email2;authuser=2;authorization_code=code2;");
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n");
  headers->AddHeader(kDiceResponseHeader, header_value);
  headers->AddHeader(kDiceLinkedAccountsMetaHeader, "initiator_id=id2");
  DiceResponseParams params =
      DiceHeaderHelper::CreateDiceResponseParams(headers.get());

  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  ASSERT_TRUE(params.signin_info());
  const auto& accounts = params.signin_info()->accounts();
  ASSERT_EQ(2U, accounts.size());

  {
    SCOPED_TRACE("Verifying Account 1");
    VerifySigninAccount(accounts[0], GaiaId("id1"), "email1", 1, "code1");
  }
  {
    SCOPED_TRACE("Verifying Account 2");
    VerifySigninAccount(accounts[1], GaiaId("id2"), "email2", 2, "code2");
  }
  EXPECT_TRUE(params.IsValid());
}

TEST_F(DiceHeaderHelperTest,
       CreateDiceResponseParams_MultiAccountDifferentActionsFirstWins) {
  // This is a synthetic edgecase to test that the first action in the header
  // wins when actions differ between accounts. In real-world scenarios, all
  // actions in a multi-account header should be the same.
  std::string header_value = base::StringPrintf(
      "action=SIGNIN;id=id1;email=email1;authuser=1;authorization_code=code1,"
      "action=ENABLE_SYNC;id=id2;email=email2;authuser=2;authorization_code="
      "code2");
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n");
  headers->AddHeader(kDiceResponseHeader, header_value);
  headers->AddHeader(kDiceLinkedAccountsMetaHeader, "initiator_id=id2");
  DiceResponseParams params =
      DiceHeaderHelper::CreateDiceResponseParams(headers.get());

  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention());
  ASSERT_TRUE(params.signin_info());
  const auto& accounts = params.signin_info()->accounts();
  ASSERT_EQ(2U, accounts.size());

  {
    SCOPED_TRACE("Verifying Account 1 (Signin wins)");
    VerifySigninAccount(accounts[0], GaiaId("id1"), "email1", 1, "code1");
  }
  {
    SCOPED_TRACE("Verifying Account 2 (EnableSync parsed as Signin)");
    VerifySigninAccount(accounts[1], GaiaId("id2"), "email2", 2, "code2");
  }

  EXPECT_TRUE(params.IsValid());
}

TEST_F(DiceHeaderHelperTest, ParseLinkedAccountsMetadata_ValidHeader) {
  std::string header_value =
      "initiator_id=initiator_gaia_id;primary_is_connected=1";
  DiceResponseParams::SigninInfo::LinkedAccountsMetadata metadata =
      DiceHeaderHelper::ParseLinkedAccountsMetadata(header_value);
  EXPECT_EQ((DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
                .primary_is_connected = Tribool::kTrue,
                .initiator_id = GaiaId("initiator_gaia_id")}),
            metadata);
  EXPECT_TRUE(metadata.IsValid());
}

TEST_F(DiceHeaderHelperTest, ParseLinkedAccountsMetadata_PrimaryNotConnected) {
  std::string header_value =
      "initiator_id=initiator_gaia_id;primary_is_connected=0";
  DiceResponseParams::SigninInfo::LinkedAccountsMetadata metadata =
      DiceHeaderHelper::ParseLinkedAccountsMetadata(header_value);
  EXPECT_EQ((DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
                .primary_is_connected = Tribool::kFalse,
                .initiator_id = GaiaId("initiator_gaia_id")}),
            metadata);
  EXPECT_TRUE(metadata.IsValid());
}

TEST_F(DiceHeaderHelperTest,
       ParseLinkedAccountsMetadata_MissingPrimaryIsConnected) {
  // Missing primary_is_connected -> Partial info.
  std::string header_value = "initiator_id=initiator_gaia_id";
  DiceResponseParams::SigninInfo::LinkedAccountsMetadata metadata =
      DiceHeaderHelper::ParseLinkedAccountsMetadata(header_value);
  EXPECT_EQ((DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
                .primary_is_connected = Tribool::kUnknown,
                .initiator_id = GaiaId("initiator_gaia_id")}),
            metadata);
  EXPECT_FALSE(metadata.IsValid());
}

TEST_F(DiceHeaderHelperTest, ParseLinkedAccountsMetadata_MissingInitiatorId) {
  // Missing initiator_id -> Partial info.
  std::string header_value = "primary_is_connected=1";
  DiceResponseParams::SigninInfo::LinkedAccountsMetadata metadata =
      DiceHeaderHelper::ParseLinkedAccountsMetadata(header_value);
  EXPECT_EQ(
      (DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
          .primary_is_connected = Tribool::kTrue, .initiator_id = GaiaId()}),
      metadata);
  EXPECT_FALSE(metadata.IsValid());
}

TEST_F(DiceHeaderHelperTest, ParseLinkedAccountsMetadata_EmptyHeader) {
  // Empty header -> Default metadata.
  std::string header_value = "";
  DiceResponseParams::SigninInfo::LinkedAccountsMetadata metadata =
      DiceHeaderHelper::ParseLinkedAccountsMetadata(header_value);
  EXPECT_EQ(
      (DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
          .primary_is_connected = Tribool::kUnknown, .initiator_id = GaiaId()}),
      metadata);
  EXPECT_FALSE(metadata.IsValid());
}

TEST_F(DiceHeaderHelperTest, ParseLinkedAccountsMetadata_GarbageHeader) {
  // Garbage header -> Default metadata.
  std::string header_value = "garbage";
  DiceResponseParams::SigninInfo::LinkedAccountsMetadata metadata =
      DiceHeaderHelper::ParseLinkedAccountsMetadata(header_value);
  EXPECT_EQ(
      (DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
          .primary_is_connected = Tribool::kUnknown, .initiator_id = GaiaId()}),
      metadata);
  EXPECT_FALSE(metadata.IsValid());
}

TEST_F(DiceHeaderHelperTest,
       ParseLinkedAccountsMetadata_EscapedValuesInHeader) {
  std::string header_value = "initiator_id=gaia%3Aid;primary_is_connected=1";
  DiceResponseParams::SigninInfo::LinkedAccountsMetadata metadata =
      DiceHeaderHelper::ParseLinkedAccountsMetadata(header_value);
  EXPECT_EQ((DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
                .primary_is_connected = Tribool::kTrue,
                .initiator_id = GaiaId("gaia:id")}),
            metadata);
  EXPECT_TRUE(metadata.IsValid());
}

TEST_F(DiceHeaderHelperTest, AppendOrRemoveDiceRequestHeader_NoDiceForNonGaia) {
  account_consistency_ = AccountConsistencyMethod::kDice;
  // No Dice for Docs URLs.
  CheckDiceHeaderRequest(GURL("https://docs.google.com"), GaiaId("0123456789"),
                         /*expected_dice_request=*/"");

  // No Dice for other URLs.
  CheckDiceHeaderRequest(GURL("https://www.google.com"), GaiaId("0123456789"),
                         /*expected_dice_request=*/"");
}

TEST_F(DiceHeaderHelperTest, AppendOrRemoveDiceRequestHeader_GaiaSyncDisabled) {
  account_consistency_ = AccountConsistencyMethod::kDice;
  sync_enabled_ = false;

  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  ASSERT_FALSE(client_id.empty());
  CheckDiceHeaderRequest(
      GURL("https://accounts.google.com"), GaiaId("0123456789"),
      base::StringPrintf("version=%s,client_id=%s,device_id=DeviceID,signin_"
                         "mode=all_accounts,signout_mode=show_confirmation",
                         kDiceProtocolVersion, client_id.c_str()));
}

TEST_F(DiceHeaderHelperTest, AppendOrRemoveDiceRequestHeader_GaiaSyncEnabled) {
  account_consistency_ = AccountConsistencyMethod::kDice;
  sync_enabled_ = true;

  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  ASSERT_FALSE(client_id.empty());
  CheckDiceHeaderRequest(
      GURL("https://accounts.google.com"), GaiaId("0123456789"),
      base::StringPrintf(
          "version=%s,client_id=%s,device_id=DeviceID,sync_account_id="
          "0123456789,signin_mode=all_accounts,signout_mode=show_confirmation",
          kDiceProtocolVersion, client_id.c_str()));
}

// When cookies are blocked, the Dice header is still sent.
TEST_F(DiceHeaderHelperTest, AppendOrRemoveDiceRequestHeader_CookiesBlocked) {
  account_consistency_ = AccountConsistencyMethod::kDice;
  settings_map_->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                          CONTENT_SETTING_BLOCK);

  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  ASSERT_FALSE(client_id.empty());
  CheckDiceHeaderRequest(
      GURL("https://accounts.google.com"), GaiaId("0123456789"),
      base::StringPrintf("version=%s,client_id=%s,device_id=DeviceID,signin_"
                         "mode=all_accounts,signout_mode=show_confirmation",
                         kDiceProtocolVersion, client_id.c_str()));
}

TEST_F(DiceHeaderHelperTest, AppendOrRemoveDiceRequestHeader_DiceDisabled) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  const GURL url("https://accounts.google.com");
  CheckDiceHeaderRequest(url, GaiaId("0123456789"),
                         /*expected_dice_request=*/"");
}

TEST_F(DiceHeaderHelperTest, AppendOrRemoveDiceRequestHeader_EmptyDeviceID) {
  account_consistency_ = AccountConsistencyMethod::kDice;
  device_id_ = std::string();
  const GURL url("https://accounts.google.com");
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  CheckDiceHeaderRequest(url, GaiaId("0123456789"),
                         "version=" + std::string(kDiceProtocolVersion) +
                             ",client_id=" + client_id +
                             ",signin_mode=all_accounts,signout_mode=show_"
                             "confirmation");
}

}  // namespace signin
