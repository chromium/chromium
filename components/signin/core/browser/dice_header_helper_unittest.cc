// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_header_helper.h"

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/core/browser/dice_response_params.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_request_headers.h"
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

  base::test::TaskEnvironment task_environment_;

  bool sync_enabled_ = false;
  AccountConsistencyMethod account_consistency_ =
      AccountConsistencyMethod::kDisabled;
  std::string device_id_ = kTestDeviceId;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
};

TEST_F(DiceHeaderHelperTest, TestDiceInvalidResponseParams) {
  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams("blah");
  EXPECT_FALSE(params.IsValid());
  EXPECT_EQ(DiceAction::NONE, params.user_intention);
  params = DiceHeaderHelper::BuildDiceSignoutResponseParams("blah");
  EXPECT_FALSE(params.IsValid());
  EXPECT_EQ(DiceAction::SIGNOUT, params.user_intention);
}

TEST_F(DiceHeaderHelperTest, BuildDiceSigninResponseParams) {
  const char kAuthorizationCode[] = "authorization_code";
  const char kEmail[] = "foo@example.com";
  const GaiaId kGaiaID("gaia_id");
  const char kSupportedTokenBindingAlgorithms[] = "ES256 RS256";
  const int kSessionIndex = 42;

  {
    // SIGNIN response.
    base::HistogramTester histogram_tester;
    DiceResponseParams params =
        DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
            "action=SIGNIN,id=%s,email=%s,authuser=%i,"
            "authorization_code=%s,"
            "eligible_for_token_binding=%s,",
            kGaiaID.ToString().c_str(), kEmail, kSessionIndex,
            kAuthorizationCode, kSupportedTokenBindingAlgorithms));
    EXPECT_EQ(DiceAction::SIGNIN, params.user_intention);
    ASSERT_TRUE(params.signin_info);
    const auto* account = params.signin_info->GetInitiator();
    ASSERT_TRUE(account);
    EXPECT_EQ(kGaiaID, account->account_info.gaia_id);
    EXPECT_EQ(kEmail, account->account_info.email);
    EXPECT_EQ(kSessionIndex, account->account_info.session_index);
    EXPECT_EQ(kAuthorizationCode, account->authorization_code);
    EXPECT_EQ(kSupportedTokenBindingAlgorithms,
              account->supported_algorithms_for_token_binding);
    EXPECT_FALSE(account->mtls_token_binding);
    histogram_tester.ExpectUniqueSample("Signin.DiceAuthorizationCode", true,
                                        1);
  }

  {
    // ENABLE_SYNC response.
    DiceResponseParams params = DiceHeaderHelper::BuildDiceSigninResponseParams(
        base::StringPrintf("action=ENABLE_SYNC,id=%s,email=%s,authuser=%i",
                           kGaiaID.ToString().c_str(), kEmail, kSessionIndex));
    EXPECT_EQ(DiceAction::ENABLE_SYNC, params.user_intention);
    ASSERT_TRUE(params.enable_sync_info);
    EXPECT_EQ(kGaiaID, params.enable_sync_info->account_info.gaia_id);
    EXPECT_EQ(kEmail, params.enable_sync_info->account_info.email);
    EXPECT_EQ(kSessionIndex,
              params.enable_sync_info->account_info.session_index);
  }

  {
    // Signin response with no_authorization_code and missing
    // authorization_code.
    base::HistogramTester histogram_tester;
    DiceResponseParams params = DiceHeaderHelper::BuildDiceSigninResponseParams(
        base::StringPrintf("action=SIGNIN,id=%s,email=%s,authuser=%i,"
                           "no_authorization_code=true",
                           kGaiaID.ToString().c_str(), kEmail, kSessionIndex));
    EXPECT_EQ(DiceAction::SIGNIN, params.user_intention);
    ASSERT_TRUE(params.signin_info);
    const auto* account = params.signin_info->GetInitiator();
    ASSERT_TRUE(account);
    EXPECT_EQ(kGaiaID, account->account_info.gaia_id);
    EXPECT_EQ(kEmail, account->account_info.email);
    EXPECT_EQ(kSessionIndex, account->account_info.session_index);
    EXPECT_TRUE(account->authorization_code.empty());
    EXPECT_TRUE(account->no_authorization_code);
    EXPECT_FALSE(account->mtls_token_binding);
    histogram_tester.ExpectUniqueSample("Signin.DiceAuthorizationCode", false,
                                        1);
  }

  {
    // Missing authorization code and no_authorization_code.
    base::HistogramTester histogram_tester;
    DiceResponseParams params = DiceHeaderHelper::BuildDiceSigninResponseParams(
        base::StringPrintf("action=SIGNIN,id=%s,email=%s,authuser=%i",
                           kGaiaID.ToString().c_str(), kEmail, kSessionIndex));
    EXPECT_EQ(DiceAction::SIGNIN, params.user_intention);
    EXPECT_FALSE(params.IsValid());
    histogram_tester.ExpectTotalCount("Signin.DiceAuthorizationCode", 0);
  }

  {
    // Missing email in SIGNIN.
    DiceResponseParams params =
        DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
            "action=SIGNIN,id=%s,authuser=%i,authorization_code=%s",
            kGaiaID.ToString().c_str(), kSessionIndex, kAuthorizationCode));
    EXPECT_EQ(DiceAction::SIGNIN, params.user_intention);
    EXPECT_FALSE(params.IsValid());
  }
}

TEST_F(DiceHeaderHelperTest,
       BuildDiceSigninResponseParamsWithMtlsTokenBinding) {
  const char kAuthorizationCode[] = "authorization_code";
  const char kEmail[] = "foo@example.com";
  const GaiaId kGaiaID("gaia_id");
  const char kSupportedTokenBindingAlgorithms[] = "ES256 RS256";
  const int kSessionIndex = 42;
  base::HistogramTester histogram_tester;
  DiceResponseParams params = DiceHeaderHelper::BuildDiceSigninResponseParams(
      base::StringPrintf("action=SIGNIN,id=%s,email=%s,authuser=%i,"
                         "authorization_code=%s,"
                         "eligible_for_token_binding=%s,"
                         "mtls_token_binding=true",
                         kGaiaID.ToString().c_str(), kEmail, kSessionIndex,
                         kAuthorizationCode, kSupportedTokenBindingAlgorithms));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention);
  ASSERT_TRUE(params.signin_info);
  const auto* account = params.signin_info->GetInitiator();
  ASSERT_TRUE(account);
  EXPECT_EQ(kGaiaID, account->account_info.gaia_id);
  EXPECT_EQ(kEmail, account->account_info.email);
  EXPECT_EQ(kSessionIndex, account->account_info.session_index);
  EXPECT_EQ(kAuthorizationCode, account->authorization_code);
  EXPECT_EQ(kSupportedTokenBindingAlgorithms,
            account->supported_algorithms_for_token_binding);
  EXPECT_TRUE(account->mtls_token_binding);
  histogram_tester.ExpectUniqueSample("Signin.DiceAuthorizationCode", true, 1);
}

TEST_F(DiceHeaderHelperTest, BuildDiceSignoutResponseParams) {
  const char kEmail[] = "foo@example.com";
  const GaiaId kGaiaID("gaia_id");
  const int kSessionIndex = 42;

  {
    // SIGNOUT response.
    DiceResponseParams params =
        DiceHeaderHelper::BuildDiceSignoutResponseParams(base::StringPrintf(
            "email=%s, sessionindex=%i, obfuscatedid=%s", kEmail, kSessionIndex,
            kGaiaID.ToString().c_str()));
    ASSERT_EQ(DiceAction::SIGNOUT, params.user_intention);
    ASSERT_TRUE(params.signout_info);
    EXPECT_EQ(1u, params.signout_info->account_infos.size());
    EXPECT_EQ(kGaiaID, params.signout_info->account_infos[0].gaia_id);
    EXPECT_EQ(kEmail, params.signout_info->account_infos[0].email);
    EXPECT_EQ(kSessionIndex,
              params.signout_info->account_infos[0].session_index);
  }

  {
    // SIGNOUT response with multiple accounts.
    // some fields are wrapped in quotes.
    DiceResponseParams params =
        DiceHeaderHelper::BuildDiceSignoutResponseParams(base::StringPrintf(
            "email=\"%s\", sessionindex=%i, obfuscatedid=\"%s\"", kEmail,
            kSessionIndex, kGaiaID.ToString().c_str()));
    ASSERT_EQ(DiceAction::SIGNOUT, params.user_intention);
    ASSERT_TRUE(params.signout_info);
    EXPECT_EQ(1u, params.signout_info->account_infos.size());
    EXPECT_EQ(kGaiaID, params.signout_info->account_infos[0].gaia_id);
    EXPECT_EQ(kEmail, params.signout_info->account_infos[0].email);
    EXPECT_EQ(kSessionIndex,
              params.signout_info->account_infos[0].session_index);
  }

  {
    // Multi-Signout response.
    const char kEmail2[] = "bar@example.com";
    const GaiaId kGaiaID2("gaia_id_2");
    const int kSessionIndex2 = 2;
    DiceResponseParams params =
        DiceHeaderHelper::BuildDiceSignoutResponseParams(base::StringPrintf(
            "email=\"%s\", sessionindex=%i, obfuscatedid=\"%s\", "
            "email=\"%s\", sessionindex=%i, obfuscatedid=\"%s\"",
            kEmail, kSessionIndex, kGaiaID.ToString().c_str(), kEmail2,
            kSessionIndex2, kGaiaID2.ToString().c_str()));
    ASSERT_EQ(DiceAction::SIGNOUT, params.user_intention);
    ASSERT_TRUE(params.signout_info);
    EXPECT_EQ(2u, params.signout_info->account_infos.size());
    EXPECT_EQ(kGaiaID, params.signout_info->account_infos[0].gaia_id);
    EXPECT_EQ(kEmail, params.signout_info->account_infos[0].email);
    EXPECT_EQ(kSessionIndex,
              params.signout_info->account_infos[0].session_index);
    EXPECT_EQ(kGaiaID2, params.signout_info->account_infos[1].gaia_id);
    EXPECT_EQ(kEmail2, params.signout_info->account_infos[1].email);
    EXPECT_EQ(kSessionIndex2,
              params.signout_info->account_infos[1].session_index);
  }

  {
    // Missing email in signout.
    DiceResponseParams params =
        DiceHeaderHelper::BuildDiceSignoutResponseParams(base::StringPrintf(
            "email=%s, sessionindex=%i, obfuscatedid=%s, "
            "sessionindex=2, obfuscatedid=bar",
            kEmail, kSessionIndex, kGaiaID.ToString().c_str()));
    EXPECT_EQ(DiceAction::SIGNOUT, params.user_intention);
    EXPECT_FALSE(params.IsValid());
  }
}

TEST_F(DiceHeaderHelperTest,
       BuildDiceSigninResponseParamsNotEligibleForTokenBinding) {
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
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention);
  ASSERT_TRUE(params.signin_info);
  ASSERT_EQ(1u, params.signin_info->accounts().size());
  EXPECT_TRUE(params.signin_info->GetInitiator()
                  ->supported_algorithms_for_token_binding.empty());
}

TEST_F(DiceHeaderHelperTest, BuildDiceSigninResponseParamsMixedOrder) {
  const char kAuthorizationCode[] = "authorization_code";
  const char kEmail[] = "foo@example.com";
  const GaiaId kGaiaID("gaia_id");
  const char kSupportedTokenBindingAlgorithms[] = "ES256 RS256";
  const int kSessionIndex = 42;

  DiceResponseParams params =
      DiceHeaderHelper::BuildDiceSigninResponseParams(base::StringPrintf(
          "id=%s,action=SIGNIN,authuser=%i,eligible_for_token_binding=%s,email="
          "%s,"
          "authorization_code=%s",
          kGaiaID.ToString(), kSessionIndex, kSupportedTokenBindingAlgorithms,
          kEmail, kAuthorizationCode));
  EXPECT_EQ(DiceAction::SIGNIN, params.user_intention);
  ASSERT_TRUE(params.signin_info);
  const auto* account = params.signin_info->GetInitiator();
  ASSERT_TRUE(account);
  EXPECT_EQ(kGaiaID, account->account_info.gaia_id);
  EXPECT_EQ(kEmail, account->account_info.email);
  EXPECT_EQ(kSessionIndex, account->account_info.session_index);
  EXPECT_EQ(kAuthorizationCode, account->authorization_code);
  EXPECT_EQ(kSupportedTokenBindingAlgorithms,
            account->supported_algorithms_for_token_binding);
}

TEST_F(DiceHeaderHelperTest, TestDiceRequest) {
  account_consistency_ = AccountConsistencyMethod::kDice;
  // No Dice for Docs URLs.
  CheckDiceHeaderRequest(GURL("https://docs.google.com"), GaiaId("0123456789"),
                         /*expected_dice_request=*/"");

  // Only Dice header for Gaia URLs.
  // Sync disabled.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  ASSERT_FALSE(client_id.empty());
  CheckDiceHeaderRequest(
      GURL("https://accounts.google.com"), GaiaId("0123456789"),
      base::StringPrintf("version=%s,client_id=%s,device_id=DeviceID,signin_"
                         "mode=all_accounts,signout_mode=show_confirmation",
                         kDiceProtocolVersion, client_id.c_str()));
  // Sync enabled: check that the Dice header has the Sync account ID.
  sync_enabled_ = true;
  CheckDiceHeaderRequest(
      GURL("https://accounts.google.com"), GaiaId("0123456789"),
      base::StringPrintf(
          "version=%s,client_id=%s,device_id=DeviceID,sync_account_id="
          "0123456789,signin_mode=all_accounts,signout_mode=show_confirmation",
          kDiceProtocolVersion, client_id.c_str()));
  sync_enabled_ = false;

  // No Dice for other URLs.
  CheckDiceHeaderRequest(GURL("https://www.google.com"), GaiaId("0123456789"),
                         /*expected_dice_request=*/"");
}

// When cookies are blocked, the Dice header is still sent.
TEST_F(DiceHeaderHelperTest, DiceCookiesBlocked) {
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

TEST_F(DiceHeaderHelperTest, TestNoDiceRequestWhenDisabled) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  const GURL url("https://accounts.google.com");
  CheckDiceHeaderRequest(url, GaiaId("0123456789"),
                         /*expected_dice_request=*/"");
}

TEST_F(DiceHeaderHelperTest, TestDiceEmptyDeviceID) {
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
