// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_header_helper.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/browser/chrome_connected_header_helper.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/core/browser/dice_header_helper.h"
#endif

namespace signin {
namespace {
constexpr char kTestDeviceId[] = "DeviceID";
constexpr char kTestSource[] = "TestSource";

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
    net::HttpRequestHeaders final_headers(original_headers_);
    final_headers.MergeFrom(modified_request_headers_);
    for (const std::string& name : to_be_removed_request_headers_)
      final_headers.RemoveHeader(name);
    return final_headers;
  }

 private:
  RequestAdapter adapter_;
  const net::HttpRequestHeaders& original_headers_;
  net::HttpRequestHeaders modified_request_headers_;
  std::vector<std::string> to_be_removed_request_headers_;
};
}  // namespace

class SigninHeaderHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());

    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */);
    cookie_settings_ = new content_settings::CookieSettings(settings_map_.get(),
                                                            &prefs_, false, "");
  }

  void TearDown() override { settings_map_->ShutdownOnUIThread(); }

  void CheckMirrorCookieRequest(const GURL& url,
                                const std::string& gaia_id,
                                const std::string& expected_request) {
    EXPECT_EQ(BuildMirrorRequestCookieIfPossible(
                  url, gaia_id, account_consistency_, cookie_settings_.get(),
                  PROFILE_MODE_DEFAULT),
              expected_request);
  }

  net::HttpRequestHeaders CreateRequest(const GURL& url,
                                        const std::string& account_id) {
    net::HttpRequestHeaders original_headers;
    RequestAdapterWrapper request_adapter(url, original_headers);
    AppendOrRemoveMirrorRequestHeader(
        request_adapter.adapter(), GURL(), account_id, account_consistency_,
        cookie_settings_.get(), PROFILE_MODE_DEFAULT, kTestSource,
        force_account_consistency_);
    AppendOrRemoveDiceRequestHeader(
        request_adapter.adapter(), GURL(), account_id, sync_enabled_,
        account_consistency_, cookie_settings_.get(), device_id_);
    return request_adapter.GetFinalHeaders();
  }

  void CheckAccountConsistencyHeaderRequest(
      const net::HttpRequestHeaders& headers,
      const char* header_name,
      const std::string& expected_request) {
    bool expected_result = !expected_request.empty();
    std::string request;
    EXPECT_EQ(headers.GetHeader(header_name, &request), expected_result)
        << header_name << ": " << request;
    if (expected_result) {
      EXPECT_EQ(expected_request, request);
    }
  }

  void CheckMirrorHeaderRequest(const GURL& url,
                                const std::string& account_id,
                                const std::string& expected_request) {
    net::HttpRequestHeaders headers = CreateRequest(url, account_id);
    CheckAccountConsistencyHeaderRequest(headers, kChromeConnectedHeader,
                                         expected_request);
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void CheckDiceHeaderRequest(const GURL& url,
                              const std::string& account_id,
                              const std::string& expected_mirror_request,
                              const std::string& expected_dice_request) {
    net::HttpRequestHeaders headers = CreateRequest(url, account_id);
    CheckAccountConsistencyHeaderRequest(headers, kChromeConnectedHeader,
                                         expected_mirror_request);
    CheckAccountConsistencyHeaderRequest(headers, kDiceRequestHeader,
                                         expected_dice_request);
  }
#endif

  base::test::SingleThreadTaskEnvironment task_environment_;

  bool sync_enabled_ = false;
  std::string device_id_ = kTestDeviceId;
  AccountConsistencyMethod account_consistency_ =
      AccountConsistencyMethod::kDisabled;
  bool force_account_consistency_ = false;

  sync_preferences::TestingPrefServiceSyncable prefs_;

  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
};

#if defined(OS_CHROMEOS)
// Tests that Mirror request is returned on Chrome OS for Public Sessions (no
// account id).
TEST_F(SigninHeaderHelperTest, TestMirrorRequestNoAccountIdChromeOS) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  CheckMirrorHeaderRequest(
      GURL("https://docs.google.com"), "",
      "source=TestSource,mode=0,enable_account_consistency=true,"
      "consistency_enabled_by_default=false");
  CheckMirrorCookieRequest(GURL("https://docs.google.com"), "",
                           "mode=0:enable_account_consistency=true:"
                           "consistency_enabled_by_default=false");
}
#else  // !defined(OS_CHROMEOS)
#if defined(OS_ANDROID) || defined(OS_IOS)
// Tests that eligible_for_consistency request is returned on mobile (Android,
// iOS) when reaching to Gaia origin and there's no primary account. Only
// applicable when the Mobile Identity Consistency is enabled.
TEST_F(SigninHeaderHelperTest, TestEligibleForConsistencyRequestGaiaOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMobileIdentityConsistency);

  account_consistency_ = AccountConsistencyMethod::kMirror;
  CheckMirrorHeaderRequest(GURL("https://accounts.google.com"), "",
                           "source=TestSource,eligible_for_consistency=true");
  CheckMirrorCookieRequest(GURL("https://accounts.google.com"), "",
                           "eligible_for_consistency=true");
}

// Tests that eligible_for_consistency request is NOT returned on mobile
// (Android, iOS) when reaching to NON-Gaia origin and there's no primary
// account. Only applicable when the Mobile Identity Consistency is enabled.
TEST_F(SigninHeaderHelperTest,
       TestNoEligibleForConsistencyRequestNonGaiaOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMobileIdentityConsistency);

  account_consistency_ = AccountConsistencyMethod::kMirror;
  CheckMirrorHeaderRequest(GURL("https://docs.google.com"), "", "");
  CheckMirrorCookieRequest(GURL("https://docs.google.com"), "", "");
}

// Tests that the full Mirror request is returned when the
// force_account_consistency param is true.
TEST_F(SigninHeaderHelperTest, TestForceAccountConsistencyMobile) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMobileIdentityConsistency);

  account_consistency_ = AccountConsistencyMethod::kMirror;
  force_account_consistency_ = true;
  CheckMirrorHeaderRequest(
      GURL("https://docs.google.com"), "",
      "source=TestSource,mode=0,enable_account_consistency=true,"
      "consistency_enabled_by_default=false");
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

// Tests that no Mirror request is returned when the user is not signed in (no
// account id), for non Chrome OS platforms.
TEST_F(SigninHeaderHelperTest, TestNoMirrorRequestNoAccountId) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  CheckMirrorHeaderRequest(GURL("https://docs.google.com"), "", "");
  CheckMirrorCookieRequest(GURL("https://docs.google.com"), "", "");
}
#endif

// Tests that no Mirror request is returned when the cookies aren't allowed to
// be set.
TEST_F(SigninHeaderHelperTest, TestNoMirrorRequestCookieSettingBlocked) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CheckMirrorHeaderRequest(GURL("https://docs.google.com"), "0123456789", "");
  CheckMirrorCookieRequest(GURL("https://docs.google.com"), "0123456789", "");
}

// Tests that no Mirror request is returned when the target is a non-Google URL.
TEST_F(SigninHeaderHelperTest, TestNoMirrorRequestExternalURL) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  CheckMirrorHeaderRequest(GURL("https://foo.com"), "0123456789", "");
  CheckMirrorCookieRequest(GURL("https://foo.com"), "0123456789", "");
}

// Tests that the Mirror request is returned without the GAIA Id when the target
// is a google TLD domain.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestGoogleTLD) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  CheckMirrorHeaderRequest(
      GURL("https://google.fr"), "0123456789",
      "source=TestSource,mode=0,enable_account_consistency=true,"
      "consistency_enabled_by_default=false");
  CheckMirrorCookieRequest(GURL("https://google.de"), "0123456789",
                           "mode=0:enable_account_consistency=true:"
                           "consistency_enabled_by_default=false");
}

// Tests that the Mirror request is returned when the target is the domain
// google.com, and that the GAIA Id is only attached for the cookie.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestGoogleCom) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  CheckMirrorHeaderRequest(
      GURL("https://www.google.com"), "0123456789",
      "source=TestSource,mode=0,enable_account_consistency=true,"
      "consistency_enabled_by_default=false");
  CheckMirrorCookieRequest(
      GURL("https://www.google.com"), "0123456789",
      "id=0123456789:mode=0:enable_account_consistency=true:"
      "consistency_enabled_by_default=false");
}

// Tests that no header sent when mirror account consistency is nor requested.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestGoogleComNoProfileConsistency) {
  net::HttpRequestHeaders original_headers;
  RequestAdapterWrapper request_adapter(GURL("https://www.google.com"),
                                        original_headers);
  AppendOrRemoveMirrorRequestHeader(
      request_adapter.adapter(), GURL(), "0123456789", account_consistency_,
      cookie_settings_.get(), PROFILE_MODE_DEFAULT, kTestSource,
      false /* force_account_consistency */);
  CheckAccountConsistencyHeaderRequest(request_adapter.GetFinalHeaders(),
                                       kChromeConnectedHeader, "");
}

// Tests that header sent when mirror account consistency is requested.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestGoogleComProfileConsistency) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  net::HttpRequestHeaders original_headers;
  RequestAdapterWrapper request_adapter(GURL("https://www.google.com"),
                                        original_headers);
  AppendOrRemoveMirrorRequestHeader(
      request_adapter.adapter(), GURL(), "0123456789", account_consistency_,
      cookie_settings_.get(), PROFILE_MODE_DEFAULT, kTestSource,
      false /* force_account_consistency */);
  CheckAccountConsistencyHeaderRequest(
      request_adapter.GetFinalHeaders(), kChromeConnectedHeader,
      "source=TestSource,mode=0,enable_account_consistency=true,"
      "consistency_enabled_by_default=false");
}

// Mirror is always enabled on Android and iOS, so these tests are only relevant
// on Desktop.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)

// Tests that the Mirror request is returned when the target is a Gaia URL, even
// if account consistency is disabled.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestGaiaURL) {
  CheckMirrorHeaderRequest(
      GURL("https://accounts.google.com"), "0123456789",
      "source=TestSource,mode=0,enable_account_consistency=false,"
      "consistency_enabled_by_default=false");
  CheckMirrorCookieRequest(
      GURL("https://accounts.google.com"), "0123456789",
      "id=0123456789:mode=0:enable_account_consistency=false:"
      "consistency_enabled_by_default=false");
}

// Tests Dice requests.
TEST_F(SigninHeaderHelperTest, TestDiceRequest) {
  account_consistency_ = AccountConsistencyMethod::kDice;
  // ChromeConnected but no Dice for Docs URLs.
  CheckDiceHeaderRequest(
      GURL("https://docs.google.com"), "0123456789",
      "source=TestSource,id=0123456789,mode=0,enable_account_consistency=false,"
      "consistency_enabled_by_default=false",
      "");

  // ChromeConnected and Dice for Gaia URLs.
  // Sync disabled.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  ASSERT_FALSE(client_id.empty());
  CheckDiceHeaderRequest(
      GURL("https://accounts.google.com"), "0123456789",
      "source=TestSource,mode=0,enable_account_consistency=false,"
      "consistency_enabled_by_default=false",
      base::StringPrintf(
          "version=%s,client_id=%s,device_id=DeviceID,signin_mode=all_accounts,"
          "signout_mode=show_confirmation",
          kDiceProtocolVersion, client_id.c_str()));
  // Sync enabled: check that the Dice header has the Sync account ID and that
  // the mirror header is not modified.
  sync_enabled_ = true;
  CheckDiceHeaderRequest(
      GURL("https://accounts.google.com"), "0123456789",
      "source=TestSource,mode=0,enable_account_consistency=false,"
      "consistency_enabled_by_default=false",
      base::StringPrintf("version=%s,client_id=%s,device_id=DeviceID,"
                         "sync_account_id=0123456789,signin_mode=all_accounts,"
                         "signout_mode=show_confirmation",
                         kDiceProtocolVersion, client_id.c_str()));
  sync_enabled_ = false;

  // No ChromeConnected and no Dice for other URLs.
  CheckDiceHeaderRequest(GURL("https://www.google.com"), "0123456789", "", "");
}

// When cookies are blocked, only the Dice header is sent.
TEST_F(SigninHeaderHelperTest, DiceCookiesBlocked) {
  account_consistency_ = AccountConsistencyMethod::kDice;
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  ASSERT_FALSE(client_id.empty());
  CheckDiceHeaderRequest(
      GURL("https://accounts.google.com"), "0123456789", "",
      base::StringPrintf(
          "version=%s,client_id=%s,device_id=DeviceID,signin_mode=all_accounts,"
          "signout_mode=show_confirmation",
          kDiceProtocolVersion, client_id.c_str()));
}

// Tests that no Dice request is returned when Dice is not enabled.
TEST_F(SigninHeaderHelperTest, TestNoDiceRequestWhenDisabled) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  CheckDiceHeaderRequest(
      GURL("https://accounts.google.com"), "0123456789",
      "source=TestSource,mode=0,enable_account_consistency=true,"
      "consistency_enabled_by_default=false",
      "");
}

TEST_F(SigninHeaderHelperTest, TestDiceEmptyDeviceID) {
  account_consistency_ = AccountConsistencyMethod::kDice;
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  ASSERT_FALSE(client_id.empty());

  device_id_.clear();

  CheckDiceHeaderRequest(
      GURL("https://accounts.google.com"), "0123456789",
      "source=TestSource,mode=0,enable_account_consistency=false,"
      "consistency_enabled_by_default=false",
      base::StringPrintf("version=%s,client_id=%s,signin_mode=all_accounts,"
                         "signout_mode=show_confirmation",
                         kDiceProtocolVersion, client_id.c_str()));
}

// Tests that the signout confirmation is requested.
TEST_F(SigninHeaderHelperTest, TestSignoutConfirmation) {
  account_consistency_ = AccountConsistencyMethod::kDice;
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  ASSERT_FALSE(client_id.empty());

  CheckDiceHeaderRequest(
      GURL("https://accounts.google.com"), "0123456789",
      "source=TestSource,mode=0,enable_account_consistency=false,"
      "consistency_enabled_by_default=false",
      base::StringPrintf(
          "version=%s,client_id=%s,device_id=DeviceID,signin_mode=all_accounts,"
          "signout_mode=show_confirmation",
          kDiceProtocolVersion, client_id.c_str()));
}

// Tests that the Mirror request is returned with the GAIA Id on Drive origin,
// even if account consistency is disabled.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestDrive) {
  CheckMirrorHeaderRequest(
      GURL("https://docs.google.com/document"), "0123456789",
      "source=TestSource,id=0123456789,mode=0,enable_account_consistency=false,"
      "consistency_enabled_by_default=false");
  CheckMirrorCookieRequest(
      GURL("https://drive.google.com/drive"), "0123456789",
      "id=0123456789:mode=0:enable_account_consistency=false:"
      "consistency_enabled_by_default=false");

  // Enable Account Consistency will override the disable.
  account_consistency_ = AccountConsistencyMethod::kMirror;
  CheckMirrorHeaderRequest(
      GURL("https://docs.google.com/document"), "0123456789",
      "source=TestSource,id=0123456789,mode=0,enable_account_consistency=true,"
      "consistency_enabled_by_default=false");
  CheckMirrorCookieRequest(
      GURL("https://drive.google.com/drive"), "0123456789",
      "id=0123456789:mode=0:enable_account_consistency=true:"
      "consistency_enabled_by_default=false");
}

TEST_F(SigninHeaderHelperTest, TestDiceInvalidResponseParams) {
  DiceResponseParams params = BuildDiceSigninResponseParams("blah");
  EXPECT_EQ(DiceAction::NONE, params.user_intention);
  params = BuildDiceSignoutResponseParams("blah");
  EXPECT_EQ(DiceAction::NONE, params.user_intention);
}

TEST_F(SigninHeaderHelperTest, TestBuildDiceResponseParams) {
  const char kAuthorizationCode[] = "authorization_code";
  const char kEmail[] = "foo@example.com";
  const char kGaiaID[] = "gaia_id";
  const int kSessionIndex = 42;

  {
    // Signin response.
    base::HistogramTester histogram_tester;
    DiceResponseParams params =
        BuildDiceSigninResponseParams(base::StringPrintf(
            "action=SIGNIN,id=%s,email=%s,authuser=%i,authorization_code=%s",
            kGaiaID, kEmail, kSessionIndex, kAuthorizationCode));
    EXPECT_EQ(DiceAction::SIGNIN, params.user_intention);
    ASSERT_TRUE(params.signin_info);
    EXPECT_EQ(kGaiaID, params.signin_info->account_info.gaia_id);
    EXPECT_EQ(kEmail, params.signin_info->account_info.email);
    EXPECT_EQ(kSessionIndex, params.signin_info->account_info.session_index);
    EXPECT_EQ(kAuthorizationCode, params.signin_info->authorization_code);
    histogram_tester.ExpectUniqueSample("Signin.DiceAuthorizationCode", true,
                                        1);
  }

  {
    // EnableSync response.
    DiceResponseParams params = BuildDiceSigninResponseParams(
        base::StringPrintf("action=ENABLE_SYNC,id=%s,email=%s,authuser=%i",
                           kGaiaID, kEmail, kSessionIndex));
    EXPECT_EQ(DiceAction::ENABLE_SYNC, params.user_intention);
    ASSERT_TRUE(params.enable_sync_info);
    EXPECT_EQ(kGaiaID, params.enable_sync_info->account_info.gaia_id);
    EXPECT_EQ(kEmail, params.enable_sync_info->account_info.email);
    EXPECT_EQ(kSessionIndex,
              params.enable_sync_info->account_info.session_index);
  }

  {
    // Signout response.
    // Note: Gaia responses typically have a whitespace after the commas, and
    // some fields are wrapped in quotes.
    DiceResponseParams params = BuildDiceSignoutResponseParams(
        base::StringPrintf("email=\"%s\", sessionindex=%i, obfuscatedid=\"%s\"",
                           kEmail, kSessionIndex, kGaiaID));
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
    const char kGaiaID2[] = "gaia_id_2";
    const int kSessionIndex2 = 2;
    DiceResponseParams params =
        BuildDiceSignoutResponseParams(base::StringPrintf(
            "email=\"%s\", sessionindex=%i, obfuscatedid=\"%s\", "
            "email=\"%s\", sessionindex=%i, obfuscatedid=\"%s\"",
            kEmail, kSessionIndex, kGaiaID, kEmail2, kSessionIndex2, kGaiaID2));
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
    // Signin response with no_authorization_code and missing
    // authorization_code.
    base::HistogramTester histogram_tester;
    DiceResponseParams params = BuildDiceSigninResponseParams(
        base::StringPrintf("action=SIGNIN,id=%s,email=%s,authuser=%i,"
                           "no_authorization_code=true",
                           kGaiaID, kEmail, kSessionIndex));
    EXPECT_EQ(DiceAction::SIGNIN, params.user_intention);
    ASSERT_TRUE(params.signin_info);
    EXPECT_EQ(kGaiaID, params.signin_info->account_info.gaia_id);
    EXPECT_EQ(kEmail, params.signin_info->account_info.email);
    EXPECT_EQ(kSessionIndex, params.signin_info->account_info.session_index);
    EXPECT_TRUE(params.signin_info->authorization_code.empty());
    EXPECT_TRUE(params.signin_info->no_authorization_code);
    histogram_tester.ExpectUniqueSample("Signin.DiceAuthorizationCode", false,
                                        1);
  }

  {
    // Missing authorization code and no_authorization_code.
    base::HistogramTester histogram_tester;
    DiceResponseParams params = BuildDiceSigninResponseParams(
        base::StringPrintf("action=SIGNIN,id=%s,email=%s,authuser=%i", kGaiaID,
                           kEmail, kSessionIndex));
    EXPECT_EQ(DiceAction::NONE, params.user_intention);
    histogram_tester.ExpectTotalCount("Signin.DiceAuthorizationCode", 0);
  }

  {
    // Missing email in SIGNIN.
    DiceResponseParams params =
        BuildDiceSigninResponseParams(base::StringPrintf(
            "action=SIGNIN,id=%s,authuser=%i,authorization_code=%s", kGaiaID,
            kSessionIndex, kAuthorizationCode));
    EXPECT_EQ(DiceAction::NONE, params.user_intention);
  }

  {
    // Missing email in signout.
    DiceResponseParams params = BuildDiceSignoutResponseParams(
        base::StringPrintf("email=%s, sessionindex=%i, obfuscatedid=%s, "
                           "sessionindex=2, obfuscatedid=bar",
                           kEmail, kSessionIndex, kGaiaID));
    EXPECT_EQ(DiceAction::NONE, params.user_intention);
  }
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Tests that the Mirror header request is returned normally when the redirect
// URL is eligible.
TEST_F(SigninHeaderHelperTest, TestMirrorHeaderEligibleRedirectURL) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  const GURL url("https://docs.google.com/document");
  const GURL redirect_url("https://www.google.com");
  const std::string account_id = "0123456789";
  net::HttpRequestHeaders original_headers;
  RequestAdapterWrapper request_adapter(url, original_headers);
  AppendOrRemoveMirrorRequestHeader(
      request_adapter.adapter(), redirect_url, account_id, account_consistency_,
      cookie_settings_.get(), PROFILE_MODE_DEFAULT, kTestSource,
      false /* force_account_consistency */);
  EXPECT_TRUE(
      request_adapter.GetFinalHeaders().HasHeader(kChromeConnectedHeader));
}

// Tests that the Mirror header request is stripped when the redirect URL is not
// eligible.
TEST_F(SigninHeaderHelperTest, TestMirrorHeaderNonEligibleRedirectURL) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  const GURL url("https://docs.google.com/document");
  const GURL redirect_url("http://www.foo.com");
  const std::string account_id = "0123456789";
  net::HttpRequestHeaders original_headers;
  original_headers.SetHeader(kChromeConnectedHeader, "foo,bar");
  RequestAdapterWrapper request_adapter(url, original_headers);
  AppendOrRemoveMirrorRequestHeader(
      request_adapter.adapter(), redirect_url, account_id, account_consistency_,
      cookie_settings_.get(), PROFILE_MODE_DEFAULT, kTestSource,
      false /* force_account_consistency */);
  EXPECT_FALSE(
      request_adapter.GetFinalHeaders().HasHeader(kChromeConnectedHeader));
}

// Tests that the Mirror header, whatever its value is, is untouched when both
// the current and the redirect URL are non-eligible.
TEST_F(SigninHeaderHelperTest, TestIgnoreMirrorHeaderNonEligibleURLs) {
  account_consistency_ = AccountConsistencyMethod::kMirror;
  const GURL url("https://www.bar.com");
  const GURL redirect_url("http://www.foo.com");
  const std::string account_id = "0123456789";
  const std::string fake_header = "foo,bar";
  net::HttpRequestHeaders original_headers;
  original_headers.SetHeader(kChromeConnectedHeader, fake_header);
  RequestAdapterWrapper request_adapter(url, original_headers);
  AppendOrRemoveMirrorRequestHeader(
      request_adapter.adapter(), redirect_url, account_id, account_consistency_,
      cookie_settings_.get(), PROFILE_MODE_DEFAULT, kTestSource,
      false /* force_account_consistency */);
  std::string header;
  EXPECT_TRUE(request_adapter.GetFinalHeaders().GetHeader(
      kChromeConnectedHeader, &header));
  EXPECT_EQ(fake_header, header);
}

TEST_F(SigninHeaderHelperTest, TestInvalidManageAccountsParams) {
  ManageAccountsParams params = BuildManageAccountsParams("blah");
  EXPECT_EQ(GAIA_SERVICE_TYPE_NONE, params.service_type);
}

// Tests that managed accounts' header is parsed correctly from string input.
TEST_F(SigninHeaderHelperTest, TestBuildManageAccountsParams) {
  const char kContinueURL[] = "https://www.example.com/continue";
  const char kEmail[] = "foo@example.com";

  std::string header = base::StringPrintf(
      "action=ADDSESSION,email=%s,is_saml=true,"
      "is_same_tab=true,continue_url=%s",
      kEmail, kContinueURL);
#if defined(OS_ANDROID) || defined(OS_IOS)
  header += ",show_consistency_promo=true";
#endif

  ManageAccountsParams params = BuildManageAccountsParams(header);
  EXPECT_EQ(GAIA_SERVICE_TYPE_ADDSESSION, params.service_type);
  EXPECT_EQ(kEmail, params.email);
  EXPECT_EQ(true, params.is_saml);
  EXPECT_EQ(true, params.is_same_tab);
  EXPECT_EQ(GURL(kContinueURL), params.continue_url);
#if defined(OS_ANDROID) || defined(OS_IOS)
  EXPECT_EQ(true, params.show_consistency_promo);
#endif
}

}  // namespace signin
