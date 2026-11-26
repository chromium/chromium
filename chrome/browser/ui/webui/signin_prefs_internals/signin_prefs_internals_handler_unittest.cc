// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin_prefs_internals/signin_prefs_internals_handler.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

using ParentKeys = std::vector<std::string>;
using PrefType = SigninPrefsInternalsHandler::PrefType;

// Messages handled by `SigninPrefsInternalsHandler`.
constexpr char kGetPrefsMessage[] = "getSigninPrefs";
constexpr char kGetSupportedPrefsMessage[] = "getSupportedSigninPrefs";
constexpr char kSetPrefMessage[] = "setSigninPref";
constexpr char kDeletePrefMessage[] = "deleteSigninPref";

// The `cr.sendWithPromise()` callback ID used by the tests.
constexpr char kCallbackId[] = "test_callback_id";

// A registered top-level pref of type INTEGER.
constexpr char kIntegerKey[] = "ChromeSigninInterceptionUserChoice";
// A registered top-level pref of type BOOLEAN.
constexpr char kBooleanKey[] = "ExtensionsExplicitBrowserSigninEnabled";
// A registered top-level timestamp pref. Note that the stored key really does
// start with a `k`, see `kChromeLastSignoutTime`.
constexpr char kTimestampKey[] = "kChromeLastSignoutTime";
// A registered top-level DICT container, holding the `kHistoryDictKey` DICT
// container, which in turn holds the INTEGER pref `kNestedIntegerKey`.
constexpr char kDictKey[] = "CrossDevicePromoPrefs";
constexpr char kHistoryDictKey[] = "history";
constexpr char kNestedIntegerKey[] = "shown_count";
// A registered top-level INTEGER pref whose key contains a literal '.', see
// `kHistoryPageHistorySyncPromoShownCount`.
constexpr char kFlatKeyWithDot[] = "history.sync_promo_shown_count";

constexpr char kGaiaId[] = "test_gaia_id";
constexpr char kEmail[] = "test@example.com";
constexpr char kOtherGaiaId[] = "other_gaia_id";
constexpr char kSigninAccountsMetadataDict[] = "signin.accounts_metadata_dict";

// Errors the handler rejects promises with, for inputs the page cannot send.
constexpr char kInvalidArgumentsError[] = "Invalid arguments";
constexpr char kEmptyGaiaIdError[] = "Empty Gaia ID";
constexpr char kUnknownGaiaIdError[] = "Unknown Gaia ID";
constexpr char kTooDeepError[] = "Parent keys are nested too deep";
constexpr char kUnregisteredError[] = "Unregistered pref";
constexpr char kDictionaryError[] = "Dictionary prefs cannot be set";
constexpr char kInvalidValueError[] = "Value does not match the pref type";
// Error the handler rejects mutations with while editing is disabled.
constexpr char kEditingDisabledError[] = "Editing is disabled";

ParentKeys NestedParents() {
  return {kDictKey, kHistoryDictKey};
}

base::ListValue ToListValue(const ParentKeys& parent_keys) {
  base::ListValue list;
  for (const std::string& parent_key : parent_keys) {
    list.Append(parent_key);
  }
  return list;
}

// Arguments of `setSigninPref`, after the callback ID.
base::ListValue SetPrefArgs(const std::string& gaia_id,
                            const ParentKeys& parent_keys,
                            const std::string& key,
                            base::Value value) {
  return base::ListValue()
      .Append(gaia_id)
      .Append(ToListValue(parent_keys))
      .Append(key)
      .Append(std::move(value));
}

// Arguments of `deleteSigninPref`, after the callback ID.
base::ListValue DeletePrefArgs(const std::string& gaia_id,
                               const ParentKeys& parent_keys,
                               const std::string& key) {
  return base::ListValue()
      .Append(gaia_id)
      .Append(ToListValue(parent_keys))
      .Append(key);
}

// How the handler answered a `cr.sendWithPromise()` message.
struct Response {
  // Whether the promise was resolved (true) or rejected (false).
  bool resolved = false;
  // The value the promise was resolved or rejected with.
  base::Value value;
};

// Sends `message` to `web_ui` with `kCallbackId` followed by `args`, and
// returns how the handler answered it. Fails the test if the handler did not
// answer exactly once.
Response SendWebUIMessage(content::TestWebUI& web_ui,
                          const std::string& message,
                          base::ListValue args) {
  base::ListValue message_args;
  message_args.Append(kCallbackId);
  for (base::Value& arg : args) {
    message_args.Append(std::move(arg));
  }

  web_ui.ClearTrackedCalls();
  web_ui.HandleReceivedMessage(message, message_args);

  Response response;
  if (web_ui.call_data().size() != 1u) {
    ADD_FAILURE() << message << " was answered " << web_ui.call_data().size()
                  << " times";
    return response;
  }
  const content::TestWebUI::CallData& call = *web_ui.call_data()[0];
  EXPECT_EQ(call.function_name(), "cr.webUIResponse");
  if (!call.arg1() || !call.arg2() || !call.arg3() || !call.arg2()->is_bool()) {
    ADD_FAILURE() << "Malformed response to " << message;
    return response;
  }
  EXPECT_EQ(*call.arg1(), base::Value(kCallbackId));
  response.resolved = call.arg2()->GetBool();
  response.value = call.arg3()->Clone();
  return response;
}

const base::DictValue* FindDefinition(const base::ListValue& prefs,
                                      const ParentKeys& parent_keys,
                                      const std::string& key) {
  const base::ListValue expected_parent_keys = ToListValue(parent_keys);
  for (const base::Value& pref : prefs) {
    const base::DictValue& pref_dict = pref.GetDict();
    const base::ListValue* pref_parent_keys = pref_dict.FindList("parentKeys");
    const std::string* pref_key = pref_dict.FindString("key");
    if (pref_parent_keys && *pref_parent_keys == expected_parent_keys &&
        pref_key && *pref_key == key) {
      return &pref_dict;
    }
  }
  return nullptr;
}

class SigninPrefsInternalsHandlerTest : public testing::Test {
 public:
  SigninPrefsInternalsHandlerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
    profile_ = testing_profile_manager_.CreateTestingProfile(
        "TestProfile",
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
                {TestingProfile::TestingFactory{
                    ChromeSigninClientFactory::GetInstance(),
                    base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                        &test_url_loader_factory_)}}));
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    // Needed by `IdentityTestEnvironment::SetCookieAccounts()`.
    identity_test_env()->SetTestURLLoaderFactory(&test_url_loader_factory_);
    // Makes `kGaiaId` known to `IdentityManager`, so that its prefs can be
    // edited.
    test_account_ = identity_test_env()->MakeAccountAvailable(
        kEmail, {.gaia_id = GaiaId(kGaiaId)});
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->AddMessageHandler(
        std::make_unique<SigninPrefsInternalsHandler>(profile_.get()));
  }

  void TearDown() override {
    // Destroys the handler before its profile.
    web_ui_.reset();
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    testing_profile_manager_.DeleteAllTestingProfiles();
    TestingBrowserProcess::GetGlobal()->local_state()->ClearPref(
        chrome_urls::kInternalOnlyUisEnabled);
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }
  // Sends `message` with `args` and returns the value the handler resolved
  // the promise with. Fails the test if the promise was rejected.
  base::Value SendMessageExpectResolved(const std::string& message,
                                        base::ListValue args) {
    Response response = SendWebUIMessage(*web_ui_, message, std::move(args));
    EXPECT_TRUE(response.resolved)
        << message << " was rejected with " << response.value;
    return std::move(response.value);
  }

  // Sends `message` with `args`, which the handler must reject. Returns the
  // error the promise was rejected with.
  std::string SendMessageExpectRejected(const std::string& message,
                                        base::ListValue args) {
    Response response = SendWebUIMessage(*web_ui_, message, std::move(args));
    EXPECT_FALSE(response.resolved)
        << message << " was resolved with " << response.value;
    if (!response.value.is_string()) {
      ADD_FAILURE() << message << " was rejected with " << response.value;
      return std::string();
    }
    return response.value.GetString();
  }

  // Sends `message` with `args` as is, i.e. without prepending a callback
  // ID, and expects the handler not to answer it.
  void SendMessageExpectNoResponse(const std::string& message,
                                   const base::ListValue& args) {
    web_ui_->ClearTrackedCalls();
    web_ui_->HandleReceivedMessage(message, args);
    EXPECT_TRUE(web_ui_->call_data().empty())
        << message << " with " << args << " was answered";
  }

  // Sends `setSigninPref` and returns whether the handler resolved it with no
  // value, as it does on success.
  bool SetPref(const std::string& gaia_id,
               const ParentKeys& parent_keys,
               const std::string& key,
               base::Value value) {
    const base::Value result = SendMessageExpectResolved(
        kSetPrefMessage,
        SetPrefArgs(gaia_id, parent_keys, key, std::move(value)));
    EXPECT_TRUE(result.is_none())
        << kSetPrefMessage << " resolved with " << result;
    return result.is_none();
  }

  // Sets a pref for `kGaiaId` in the test profile.
  bool SetPref(const ParentKeys& parent_keys,
               const std::string& key,
               base::Value value) {
    return SetPref(kGaiaId, parent_keys, key, std::move(value));
  }

  // Sends `setSigninPref` for the test profile, which the handler must
  // reject. Returns the error the promise was rejected with.
  std::string SetPrefExpectRejected(const std::string& gaia_id,
                                    const ParentKeys& parent_keys,
                                    const std::string& key,
                                    base::Value value) {
    return SendMessageExpectRejected(
        kSetPrefMessage,
        SetPrefArgs(gaia_id, parent_keys, key, std::move(value)));
  }

  // Same as above, for a pref of `kGaiaId`.
  std::string SetPrefExpectRejected(const ParentKeys& parent_keys,
                                    const std::string& key,
                                    base::Value value) {
    return SetPrefExpectRejected(kGaiaId, parent_keys, key, std::move(value));
  }

  bool DeletePref(const std::string& gaia_id,
                  const ParentKeys& parent_keys,
                  const std::string& key) {
    const base::Value result = SendMessageExpectResolved(
        kDeletePrefMessage, DeletePrefArgs(gaia_id, parent_keys, key));
    EXPECT_TRUE(result.is_bool());
    return result.is_bool() && result.GetBool();
  }

  // Deletes a pref for `kGaiaId` in the test profile.
  bool DeletePref(const ParentKeys& parent_keys, const std::string& key) {
    return DeletePref(kGaiaId, parent_keys, key);
  }

  // `deleteSigninPref` counterpart of `SetPrefExpectRejected()`.
  std::string DeletePrefExpectRejected(const std::string& gaia_id,
                                       const ParentKeys& parent_keys,
                                       const std::string& key) {
    return SendMessageExpectRejected(kDeletePrefMessage,
                                     DeletePrefArgs(gaia_id, parent_keys, key));
  }

  std::string DeletePrefExpectRejected(const ParentKeys& parent_keys,
                                       const std::string& key) {
    return DeletePrefExpectRejected(kGaiaId, parent_keys, key);
  }

  base::ListValue GetSupportedPrefs() {
    base::Value result =
        SendMessageExpectResolved(kGetSupportedPrefsMessage, base::ListValue());
    EXPECT_TRUE(result.is_list());
    return result.is_list() ? std::move(result).TakeList() : base::ListValue();
  }

  // Returns the `{canEdit, accountEmails, prefs}` dictionary resolved by
  // `getSigninPrefs`.
  base::DictValue GetPrefs() {
    base::Value result =
        SendMessageExpectResolved(kGetPrefsMessage, base::ListValue());
    EXPECT_TRUE(result.is_dict());
    return result.is_dict() ? std::move(result).TakeDict() : base::DictValue();
  }

  // Reads the pref straight from the PrefService, bypassing the handler.
  const base::DictValue& GetAccountPrefs() {
    return profile_->GetPrefs()->GetDict(kSigninAccountsMetadataDict);
  }

  // Returns the prefs of `kGaiaId`, or nullptr.
  const base::DictValue* GetTestAccountPrefs() {
    return GetAccountPrefs().FindDict(kGaiaId);
  }

  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  // The account of `kGaiaId`, with a refresh token.
  AccountInfo test_account_;
  std::unique_ptr<content::TestWebUI> web_ui_;
};

TEST_F(SigninPrefsInternalsHandlerTest, GetSupportedPrefsReportsTypes) {
  const base::ListValue prefs = GetSupportedPrefs();

  const base::DictValue* pref = FindDefinition(prefs, {}, kBooleanKey);
  ASSERT_TRUE(pref);
  EXPECT_EQ(pref->FindInt("type"), static_cast<int>(PrefType::kBoolean));

  pref = FindDefinition(prefs, {}, kIntegerKey);
  ASSERT_TRUE(pref);
  EXPECT_EQ(pref->FindInt("type"), static_cast<int>(PrefType::kInteger));

  pref = FindDefinition(prefs, {}, kTimestampKey);
  ASSERT_TRUE(pref);
  EXPECT_EQ(pref->FindInt("type"), static_cast<int>(PrefType::kTimestamp));

  pref = FindDefinition(prefs, {}, kDictKey);
  ASSERT_TRUE(pref);
  EXPECT_EQ(pref->FindInt("type"), static_cast<int>(PrefType::kDictionary));

  pref = FindDefinition(prefs, {kDictKey}, kHistoryDictKey);
  ASSERT_TRUE(pref);
  EXPECT_EQ(pref->FindInt("type"), static_cast<int>(PrefType::kDictionary));

  pref = FindDefinition(prefs, NestedParents(), kNestedIntegerKey);
  ASSERT_TRUE(pref);
  EXPECT_EQ(pref->FindInt("type"), static_cast<int>(PrefType::kInteger));

  // A '.' inside a key is not split into parent keys.
  pref = FindDefinition(prefs, {}, kFlatKeyWithDot);
  ASSERT_TRUE(pref);
  EXPECT_EQ(pref->FindInt("type"), static_cast<int>(PrefType::kInteger));

  // `kString` (a STRING pref that is not a timestamp) is not covered: no such
  // pref is registered today.
}

TEST_F(SigninPrefsInternalsHandlerTest, GetSupportedPrefsPathsAreUnique) {
  const base::ListValue prefs = GetSupportedPrefs();
  ASSERT_FALSE(prefs.empty());

  std::set<std::pair<ParentKeys, std::string>> paths;
  for (const base::Value& pref : prefs) {
    ASSERT_TRUE(pref.is_dict());
    const base::ListValue* parent_keys_list =
        pref.GetDict().FindList("parentKeys");
    const std::string* key = pref.GetDict().FindString("key");
    ASSERT_TRUE(parent_keys_list);
    ASSERT_TRUE(key);
    EXPECT_FALSE(key->empty());
    ParentKeys parent_keys;
    for (const base::Value& parent_key : *parent_keys_list) {
      ASSERT_TRUE(parent_key.is_string());
      parent_keys.push_back(parent_key.GetString());
    }
    EXPECT_TRUE(paths.emplace(std::move(parent_keys), *key).second)
        << "Duplicate pref: " << *key;
  }
}

TEST_F(SigninPrefsInternalsHandlerTest, GetPrefsReturnsEmptyPrefsInitially) {
  const base::DictValue info = GetPrefs();

  EXPECT_EQ(info.FindBool("canEdit"), true);
  // `kGaiaId` is listed although it has no prefs yet.
  ASSERT_TRUE(info.FindDict("accountEmails"));
  EXPECT_EQ(*info.FindDict("accountEmails"),
            base::DictValue().Set(kGaiaId, kEmail));
  ASSERT_TRUE(info.FindDict("prefs"));
  EXPECT_TRUE(info.FindDict("prefs")->empty());

  // Without accounts, `accountEmails` is empty too.
  identity_test_env()->RemoveRefreshTokenForAccount(
      test_account_.GetAccountId());
  const base::DictValue empty_info = GetPrefs();
  ASSERT_TRUE(empty_info.FindDict("accountEmails"));
  EXPECT_TRUE(empty_info.FindDict("accountEmails")->empty());
  ASSERT_TRUE(empty_info.FindDict("prefs"));
  EXPECT_TRUE(empty_info.FindDict("prefs")->empty());
}

TEST_F(SigninPrefsInternalsHandlerTest, GetPrefsReportsAccountEmails) {
  const AccountInfo account =
      identity_test_env()->MakeAccountAvailable("a@b.com");

  const base::DictValue info = GetPrefs();

  const base::DictValue* account_emails = info.FindDict("accountEmails");
  ASSERT_TRUE(account_emails);
  EXPECT_EQ(account_emails->size(), 2u);
  const std::string* email =
      account_emails->FindString(account.GetGaiaId().ToString());
  ASSERT_TRUE(email);
  EXPECT_EQ(*email, "a@b.com");
  email = account_emails->FindString(kGaiaId);
  ASSERT_TRUE(email);
  EXPECT_EQ(*email, kEmail);
}

// Accounts known to `IdentityManager` only through the Gaia cookie jar (signed
// in or signed out) or as the primary account are listed, and their prefs can
// be set, even before they have any.
TEST_F(SigninPrefsInternalsHandlerTest,
       GetPrefsAndSetPrefSupportCookieJarAndPrimaryAccounts) {
  const GaiaId signed_in_cookie_gaia_id("signed_in_cookie_gaia_id");
  const GaiaId signed_out_cookie_gaia_id("signed_out_cookie_gaia_id");
  // A non-canonical email checks that the cookie jar email is displayed as
  // listed by Gaia (`ListedAccount::raw_email`), not canonicalized.
  identity_test_env()->SetCookieAccounts(
      {{.email = "Signed.In@example.com", .gaia_id = signed_in_cookie_gaia_id},
       {.email = "signed_out@example.com",
        .gaia_id = signed_out_cookie_gaia_id,
        .signed_out = true}});
  const CoreAccountInfo primary_account =
      identity_test_env()->SetPrimaryAccount("primary@example.com",
                                             signin::ConsentLevel::kSignin);
  ASSERT_FALSE(
      identity_test_env()->identity_manager()->HasAccountWithRefreshToken(
          primary_account.account_id));
  // Signing in records `kChromeLastSigninTime` for `primary_account`; clear
  // stored prefs so `primary_account` is tested with no existing prefs entry.
  ScopedDictPrefUpdate(profile_->GetPrefs(), kSigninAccountsMetadataDict)
      ->clear();

  const base::DictValue info = GetPrefs();

  ASSERT_TRUE(info.FindDict("accountEmails"));
  EXPECT_EQ(
      *info.FindDict("accountEmails"),
      base::DictValue()
          .Set(kGaiaId, kEmail)
          .Set(signed_in_cookie_gaia_id.ToString(), "Signed.In@example.com")
          .Set(signed_out_cookie_gaia_id.ToString(), "signed_out@example.com")
          .Set(primary_account.gaia.ToString(), "primary@example.com"));
  ASSERT_TRUE(info.FindDict("prefs"));
  EXPECT_TRUE(info.FindDict("prefs")->empty());

  for (const GaiaId& gaia_id :
       {signed_in_cookie_gaia_id, signed_out_cookie_gaia_id,
        primary_account.gaia}) {
    SCOPED_TRACE(gaia_id);
    EXPECT_TRUE(SetPref(gaia_id.ToString(), {}, kIntegerKey, base::Value(1)));
    const base::DictValue* account_prefs =
        GetAccountPrefs().FindDict(gaia_id.ToString());
    ASSERT_TRUE(account_prefs);
    EXPECT_EQ(account_prefs->FindInt(kIntegerKey), 1);
  }
}

// An account with a refresh token is listed with its `CoreAccountInfo::email`,
// even if the cookie jar lists it with another spelling.
TEST_F(SigninPrefsInternalsHandlerTest,
       GetPrefsPrefersRefreshTokenEmailOverCookieJarEmail) {
  identity_test_env()->SetCookieAccounts(
      {{.email = "Test@Example.com", .gaia_id = GaiaId(kGaiaId)}});

  const base::DictValue info = GetPrefs();

  ASSERT_TRUE(info.FindDict("accountEmails"));
  EXPECT_EQ(*info.FindDict("accountEmails"),
            base::DictValue().Set(kGaiaId, kEmail));
}

// The accounts of a stale cookie jar are still listed and editable, e.g. while
// offline or before `/ListAccounts` completes.
TEST_F(SigninPrefsInternalsHandlerTest,
       GetPrefsAndSetPrefSupportStaleCookieJarAccounts) {
  const GaiaId cookie_gaia_id("cookie_gaia_id");
  identity_test_env()->SetCookieAccounts(
      {{.email = "cookie@example.com", .gaia_id = cookie_gaia_id}});
  identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(false);
  ASSERT_FALSE(identity_test_env()
                   ->identity_manager()
                   ->GetAccountsInCookieJar()
                   .AreAccountsFresh());

  const base::DictValue info = GetPrefs();

  ASSERT_TRUE(info.FindDict("accountEmails"));
  EXPECT_EQ(*info.FindDict("accountEmails"),
            base::DictValue()
                .Set(kGaiaId, kEmail)
                .Set(cookie_gaia_id.ToString(), "cookie@example.com"));
  EXPECT_TRUE(
      SetPref(cookie_gaia_id.ToString(), {}, kIntegerKey, base::Value(1)));
  const base::DictValue* account_prefs =
      GetAccountPrefs().FindDict(cookie_gaia_id.ToString());
  ASSERT_TRUE(account_prefs);
  EXPECT_EQ(account_prefs->FindInt(kIntegerKey), 1);
}

TEST_F(SigninPrefsInternalsHandlerTest, SetPrefAndGetPrefs) {
  EXPECT_TRUE(SetPref({}, kIntegerKey, base::Value(2)));

  const base::DictValue info = GetPrefs();

  const base::DictValue* prefs = info.FindDict("prefs");
  ASSERT_TRUE(prefs);
  const base::DictValue* account_prefs = prefs->FindDict(kGaiaId);
  ASSERT_TRUE(account_prefs);
  EXPECT_EQ(account_prefs->FindInt(kIntegerKey), 2);
}

TEST_F(SigninPrefsInternalsHandlerTest, GetPrefsSkipsNonDictAccountEntries) {
  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(2)));
  {
    // Corrupted entry: account prefs must be dictionaries.
    ScopedDictPrefUpdate update(profile_->GetPrefs(),
                                kSigninAccountsMetadataDict);
    update->Set(kOtherGaiaId, 1);
  }

  const base::DictValue info = GetPrefs();

  const base::DictValue* prefs = info.FindDict("prefs");
  ASSERT_TRUE(prefs);
  EXPECT_EQ(prefs->size(), 1u);
  EXPECT_TRUE(prefs->contains(kGaiaId));
  EXPECT_FALSE(prefs->contains(kOtherGaiaId));
}

// The page sends back the Gaia IDs it gets from `getSigninPrefs`, so the empty
// one, which `setSigninPref` and `deleteSigninPref` reject, is not listed.
TEST_F(SigninPrefsInternalsHandlerTest, GetPrefsSkipsEmptyGaiaIdKey) {
  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(2)));
  {
    // Corrupted entry, which the handler never writes.
    ScopedDictPrefUpdate update(profile_->GetPrefs(),
                                kSigninAccountsMetadataDict);
    update->Set("", base::DictValue());
  }

  const base::DictValue info = GetPrefs();

  const base::DictValue* prefs = info.FindDict("prefs");
  ASSERT_TRUE(prefs);
  EXPECT_EQ(prefs->size(), 1u);
  EXPECT_TRUE(prefs->contains(kGaiaId));
  EXPECT_FALSE(prefs->contains(""));
}

TEST_F(SigninPrefsInternalsHandlerTest, SetBooleanPref) {
  EXPECT_TRUE(SetPref({}, kBooleanKey, base::Value(true)));

  ASSERT_TRUE(GetTestAccountPrefs());
  EXPECT_EQ(GetTestAccountPrefs()->FindBool(kBooleanKey), true);
}

TEST_F(SigninPrefsInternalsHandlerTest, SetNestedPref) {
  EXPECT_TRUE(SetPref(NestedParents(), kNestedIntegerKey, base::Value(7)));

  ASSERT_TRUE(GetTestAccountPrefs());
  const base::DictValue* history = GetTestAccountPrefs()->FindDictByDottedPath(
      "CrossDevicePromoPrefs.history");
  ASSERT_TRUE(history);
  EXPECT_EQ(history->FindInt(kNestedIntegerKey), 7);
}

TEST_F(SigninPrefsInternalsHandlerTest, SetPrefDottedPathAsKeyIsRejected) {
  // Dots are never separators: a flattened path is not a registered key.
  EXPECT_EQ(
      SetPrefExpectRejected({}, "CrossDevicePromoPrefs.history.shown_count",
                            base::Value(7)),
      kUnregisteredError);
  EXPECT_TRUE(GetAccountPrefs().empty());
}

TEST_F(SigninPrefsInternalsHandlerTest, SetPrefWrongParentKeysIsRejected) {
  EXPECT_EQ(
      SetPrefExpectRejected({kDictKey}, kNestedIntegerKey, base::Value(7)),
      kUnregisteredError);
  EXPECT_EQ(SetPrefExpectRejected({}, kNestedIntegerKey, base::Value(7)),
            kUnregisteredError);
  EXPECT_EQ(SetPrefExpectRejected({kDictKey}, kIntegerKey, base::Value(7)),
            kUnregisteredError);
  EXPECT_TRUE(GetAccountPrefs().empty());
}

TEST_F(SigninPrefsInternalsHandlerTest, SetPrefEmptyKeysAreRejected) {
  EXPECT_EQ(SetPrefExpectRejected({}, /*key=*/"", base::Value(7)),
            kUnregisteredError);
  EXPECT_EQ(SetPrefExpectRejected({""}, kIntegerKey, base::Value(7)),
            kUnregisteredError);
  EXPECT_TRUE(GetAccountPrefs().empty());
}

// Observes the depth guard that runs before the registry lookup.
TEST_F(SigninPrefsInternalsHandlerTest, SetPrefTooDeepParentKeysIsRejected) {
  EXPECT_EQ(SetPrefExpectRejected({kDictKey, kHistoryDictKey, "a"},
                                  kNestedIntegerKey, base::Value(7)),
            kTooDeepError);
  EXPECT_TRUE(GetAccountPrefs().empty());
}

TEST_F(SigninPrefsInternalsHandlerTest, SetAndDeleteFlatPrefWithDotInKey) {
  EXPECT_TRUE(SetPref({}, kFlatKeyWithDot, base::Value(4)));

  SigninPrefs signin_prefs(*profile_->GetPrefs());
  EXPECT_EQ(
      signin_prefs.GetHistoryPageHistorySyncPromoShownCount(GaiaId(kGaiaId)),
      4);

  EXPECT_TRUE(DeletePref({}, kFlatKeyWithDot));
  EXPECT_EQ(
      signin_prefs.GetHistoryPageHistorySyncPromoShownCount(GaiaId(kGaiaId)),
      0);
}

TEST_F(SigninPrefsInternalsHandlerTest, SetTimestampPref) {
  const base::Time time = base::Time::Now();
  EXPECT_TRUE(SetPref({}, kTimestampKey, base::TimeToValue(time)));

  ASSERT_TRUE(GetTestAccountPrefs());
  const base::Value* value = GetTestAccountPrefs()->Find(kTimestampKey);
  ASSERT_TRUE(value);
  EXPECT_EQ(base::ValueToTime(*value), time);
}

TEST_F(SigninPrefsInternalsHandlerTest, SetPrefUnparsableTimestampIsRejected) {
  EXPECT_EQ(
      SetPrefExpectRejected({}, kTimestampKey, base::Value("not_a_timestamp")),
      kInvalidValueError);
  // Timestamps are serialized as strings, never as integers.
  EXPECT_EQ(SetPrefExpectRejected({}, kTimestampKey, base::Value(1)),
            kInvalidValueError);
  EXPECT_TRUE(GetAccountPrefs().empty());
}

TEST_F(SigninPrefsInternalsHandlerTest, SetPrefMismatchedTypeIsRejected) {
  // `kIntegerKey` is registered as an INTEGER, a string must be rejected.
  EXPECT_EQ(
      SetPrefExpectRejected({}, kIntegerKey, base::Value("not_an_integer")),
      kInvalidValueError);
  EXPECT_EQ(SetPrefExpectRejected({}, kIntegerKey, base::Value(true)),
            kInvalidValueError);
  EXPECT_EQ(SetPrefExpectRejected({}, kIntegerKey, base::Value(1.5)),
            kInvalidValueError);
  EXPECT_TRUE(GetAccountPrefs().empty());
}

TEST_F(SigninPrefsInternalsHandlerTest,
       SetPrefUnsupportedValueTypesAreRejected) {
  EXPECT_EQ(SetPrefExpectRejected({}, kIntegerKey, base::Value()),
            kInvalidValueError);
  EXPECT_EQ(
      SetPrefExpectRejected({}, kIntegerKey, base::Value(base::DictValue())),
      kInvalidValueError);
  EXPECT_EQ(
      SetPrefExpectRejected({}, kIntegerKey, base::Value(base::ListValue())),
      kInvalidValueError);
  EXPECT_EQ(SetPrefExpectRejected({}, kIntegerKey,
                                  base::Value(base::Value::BlobStorage())),
            kInvalidValueError);
  EXPECT_TRUE(GetAccountPrefs().empty());
}

TEST_F(SigninPrefsInternalsHandlerTest, SetPrefDictContainerIsRejected) {
  EXPECT_EQ(SetPrefExpectRejected({}, kDictKey, base::Value(base::DictValue())),
            kDictionaryError);
  EXPECT_EQ(SetPrefExpectRejected({}, kDictKey, base::Value(1)),
            kDictionaryError);
  EXPECT_EQ(SetPrefExpectRejected({kDictKey}, kHistoryDictKey,
                                  base::Value(base::DictValue())),
            kDictionaryError);
  EXPECT_TRUE(GetAccountPrefs().empty());
}

TEST_F(SigninPrefsInternalsHandlerTest, SetPrefUnregisteredPrefIsRejected) {
  EXPECT_EQ(SetPrefExpectRejected({}, "NotARegisteredPref", base::Value(1)),
            kUnregisteredError);
  EXPECT_TRUE(GetAccountPrefs().empty());
}

TEST_F(SigninPrefsInternalsHandlerTest, SetPrefEmptyGaiaIdIsRejected) {
  EXPECT_EQ(
      SetPrefExpectRejected(/*gaia_id=*/"", {}, kIntegerKey, base::Value(1)),
      kEmptyGaiaIdError);
  EXPECT_TRUE(GetAccountPrefs().empty());
}

// Gaia IDs neither known to `IdentityManager` nor already present in prefs are
// rejected, while an orphan entry (e.g. left by a signed-out account) can still
// be read, updated and deleted.
TEST_F(SigninPrefsInternalsHandlerTest,
       UnknownGaiaIdIsRejectedWhileStoredOrphanDictRemainsEditable) {
  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(3)));

  EXPECT_EQ(
      SetPrefExpectRejected("unknown_gaia", {}, kIntegerKey, base::Value(1)),
      kUnknownGaiaIdError);
  EXPECT_EQ(DeletePrefExpectRejected("unknown_gaia", {}, kIntegerKey),
            kUnknownGaiaIdError);
  EXPECT_EQ(
      GetAccountPrefs(),
      base::DictValue().Set(kGaiaId, base::DictValue().Set(kIntegerKey, 3)));

  {
    // Orphan entry of `kOtherGaiaId`, which is unknown to `IdentityManager`.
    ScopedDictPrefUpdate update(profile_->GetPrefs(),
                                kSigninAccountsMetadataDict);
    update->Set(kOtherGaiaId, base::DictValue().Set(kBooleanKey, true));
  }

  const base::DictValue info = GetPrefs();
  ASSERT_TRUE(info.FindDict("accountEmails"));
  EXPECT_FALSE(info.FindDict("accountEmails")->contains(kOtherGaiaId));
  ASSERT_TRUE(info.FindDict("prefs"));
  const base::DictValue* orphan_prefs =
      info.FindDict("prefs")->FindDict(kOtherGaiaId);
  ASSERT_TRUE(orphan_prefs);
  EXPECT_EQ(*orphan_prefs, base::DictValue().Set(kBooleanKey, true));

  EXPECT_TRUE(SetPref(kOtherGaiaId, {}, kIntegerKey, base::Value(1)));
  const base::DictValue* account_prefs =
      GetAccountPrefs().FindDict(kOtherGaiaId);
  ASSERT_TRUE(account_prefs);
  EXPECT_EQ(account_prefs->FindInt(kIntegerKey), 1);

  EXPECT_TRUE(DeletePref(kOtherGaiaId, {}, kBooleanKey));
  account_prefs = GetAccountPrefs().FindDict(kOtherGaiaId);
  ASSERT_TRUE(account_prefs);
  EXPECT_FALSE(account_prefs->contains(kBooleanKey));
  // Prefs of other accounts are untouched.
  ASSERT_TRUE(GetTestAccountPrefs());
  EXPECT_EQ(GetTestAccountPrefs()->FindInt(kIntegerKey), 3);
}

// Values are only type-checked: e.g. the enum-backed `kIntegerKey` accepts any
// integer.
TEST_F(SigninPrefsInternalsHandlerTest, SetPrefDoesNotRangeCheckIntegers) {
  EXPECT_TRUE(SetPref({}, kIntegerKey, base::Value(42)));
  ASSERT_TRUE(GetTestAccountPrefs());
  EXPECT_EQ(GetTestAccountPrefs()->FindInt(kIntegerKey), 42);
}

TEST_F(SigninPrefsInternalsHandlerTest, DeletePref) {
  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(3)));
  ASSERT_TRUE(GetTestAccountPrefs());
  ASSERT_TRUE(GetTestAccountPrefs()->FindInt(kIntegerKey));

  EXPECT_TRUE(DeletePref({}, kIntegerKey));

  ASSERT_TRUE(GetTestAccountPrefs());
  EXPECT_FALSE(GetTestAccountPrefs()->FindInt(kIntegerKey));
}

TEST_F(SigninPrefsInternalsHandlerTest, DeleteNestedPref) {
  ASSERT_TRUE(SetPref(NestedParents(), kNestedIntegerKey, base::Value(3)));

  EXPECT_TRUE(DeletePref(NestedParents(), kNestedIntegerKey));

  ASSERT_TRUE(GetTestAccountPrefs());
  const base::DictValue* history = GetTestAccountPrefs()->FindDictByDottedPath(
      "CrossDevicePromoPrefs.history");
  ASSERT_TRUE(history);
  EXPECT_FALSE(history->contains(kNestedIntegerKey));
}

// Documents that deleting a registered DICT container removes its subtree.
TEST_F(SigninPrefsInternalsHandlerTest, DeletePrefOnDictContainerRemovesAll) {
  ASSERT_TRUE(SetPref(NestedParents(), kNestedIntegerKey, base::Value(3)));
  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(1)));

  EXPECT_TRUE(DeletePref({}, kDictKey));

  ASSERT_TRUE(GetTestAccountPrefs());
  EXPECT_FALSE(GetTestAccountPrefs()->contains(kDictKey));
  // Prefs outside the container are untouched.
  EXPECT_EQ(GetTestAccountPrefs()->FindInt(kIntegerKey), 1);
}

// Same for a DICT container nested in another one.
TEST_F(SigninPrefsInternalsHandlerTest,
       DeletePrefOnInnerDictContainerRemovesAll) {
  ASSERT_TRUE(SetPref(NestedParents(), kNestedIntegerKey, base::Value(3)));
  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(1)));

  EXPECT_TRUE(DeletePref({kDictKey}, kHistoryDictKey));

  ASSERT_TRUE(GetTestAccountPrefs());
  EXPECT_FALSE(GetTestAccountPrefs()->FindDictByDottedPath(
      "CrossDevicePromoPrefs.history"));
  // Prefs outside the container are untouched.
  EXPECT_EQ(GetTestAccountPrefs()->FindInt(kIntegerKey), 1);
}

TEST_F(SigninPrefsInternalsHandlerTest, DeletePrefFailsWhenAccountHasNoPrefs) {
  EXPECT_FALSE(DeletePref({}, kIntegerKey));
}

TEST_F(SigninPrefsInternalsHandlerTest, DeletePrefEmptyGaiaIdIsRejected) {
  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(3)));

  EXPECT_EQ(DeletePrefExpectRejected(/*gaia_id=*/"", {}, kIntegerKey),
            kEmptyGaiaIdError);
  ASSERT_TRUE(GetTestAccountPrefs());
  EXPECT_EQ(GetTestAccountPrefs()->FindInt(kIntegerKey), 3);
}

TEST_F(SigninPrefsInternalsHandlerTest, DeletePrefFailsForUnsetPref) {
  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(3)));

  // The account dictionary exists, but this pref was never set.
  EXPECT_FALSE(DeletePref(NestedParents(), kNestedIntegerKey));
}

TEST_F(SigninPrefsInternalsHandlerTest, DeletePrefUnregisteredPrefIsRejected) {
  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(3)));

  EXPECT_EQ(DeletePrefExpectRejected({}, "NotARegisteredPref"),
            kUnregisteredError);
  EXPECT_EQ(DeletePrefExpectRejected({kDictKey}, kIntegerKey),
            kUnregisteredError);
  EXPECT_EQ(DeletePrefExpectRejected({}, /*key=*/""), kUnregisteredError);
  EXPECT_EQ(DeletePrefExpectRejected({""}, kIntegerKey), kUnregisteredError);
  EXPECT_EQ(DeletePrefExpectRejected({kDictKey, kHistoryDictKey, "a"},
                                     kNestedIntegerKey),
            kTooDeepError);
  EXPECT_EQ(GetTestAccountPrefs()->FindInt(kIntegerKey), 3);
}

// A message without a valid callback ID cannot be answered, so it is ignored.
TEST_F(SigninPrefsInternalsHandlerTest, MessagesWithoutCallbackIdAreIgnored) {
  for (const char* message : {kGetPrefsMessage, kGetSupportedPrefsMessage,
                              kSetPrefMessage, kDeletePrefMessage}) {
    SCOPED_TRACE(message);
    SendMessageExpectNoResponse(message, base::ListValue());
    SendMessageExpectNoResponse(message, base::ListValue().Append(1));
    SendMessageExpectNoResponse(message, base::ListValue().Append(""));
  }

  // Otherwise valid mutations are ignored without modifying prefs.
  base::ListValue set_args =
      SetPrefArgs(kGaiaId, {}, kIntegerKey, base::Value(1));
  set_args.Insert(set_args.begin(), base::Value(1));
  SendMessageExpectNoResponse(kSetPrefMessage, set_args);
  EXPECT_TRUE(GetAccountPrefs().empty());

  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(3)));
  base::ListValue delete_args = DeletePrefArgs(kGaiaId, {}, kIntegerKey);
  delete_args.Insert(delete_args.begin(), base::Value(""));
  SendMessageExpectNoResponse(kDeletePrefMessage, delete_args);
  ASSERT_TRUE(GetTestAccountPrefs());
  EXPECT_EQ(GetTestAccountPrefs()->FindInt(kIntegerKey), 3);
}

TEST_F(SigninPrefsInternalsHandlerTest, WrongArgumentCountIsRejected) {
  EXPECT_EQ(
      SendMessageExpectRejected(kGetPrefsMessage, base::ListValue().Append(1)),
      kInvalidArgumentsError);
  EXPECT_EQ(SendMessageExpectRejected(kGetSupportedPrefsMessage,
                                      base::ListValue().Append(1)),
            kInvalidArgumentsError);

  // Missing value.
  EXPECT_EQ(SendMessageExpectRejected(kSetPrefMessage,
                                      DeletePrefArgs(kGaiaId, {}, kIntegerKey)),
            kInvalidArgumentsError);
  // Extra argument.
  EXPECT_EQ(
      SendMessageExpectRejected(
          kSetPrefMessage,
          SetPrefArgs(kGaiaId, {}, kIntegerKey, base::Value(1)).Append(1)),
      kInvalidArgumentsError);
  EXPECT_EQ(SendMessageExpectRejected(kSetPrefMessage, base::ListValue()),
            kInvalidArgumentsError);
  EXPECT_TRUE(GetAccountPrefs().empty());

  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(3)));
  // Missing key.
  EXPECT_EQ(SendMessageExpectRejected(
                kDeletePrefMessage,
                base::ListValue().Append(kGaiaId).Append(base::ListValue())),
            kInvalidArgumentsError);
  // Extra argument.
  EXPECT_EQ(SendMessageExpectRejected(
                kDeletePrefMessage,
                DeletePrefArgs(kGaiaId, {}, kIntegerKey).Append(1)),
            kInvalidArgumentsError);
  EXPECT_EQ(SendMessageExpectRejected(kDeletePrefMessage, base::ListValue()),
            kInvalidArgumentsError);
  EXPECT_EQ(GetTestAccountPrefs()->FindInt(kIntegerKey), 3);
}

TEST_F(SigninPrefsInternalsHandlerTest, WrongArgumentTypesAreRejected) {
  // Gaia ID is not a string.
  EXPECT_EQ(
      SendMessageExpectRejected(kSetPrefMessage, base::ListValue()
                                                     .Append(1)
                                                     .Append(base::ListValue())
                                                     .Append(kIntegerKey)
                                                     .Append(1)),
      kInvalidArgumentsError);
  // Parent keys are not a list.
  EXPECT_EQ(SendMessageExpectRejected(kSetPrefMessage, base::ListValue()
                                                           .Append(kGaiaId)
                                                           .Append(kDictKey)
                                                           .Append(kIntegerKey)
                                                           .Append(1)),
            kInvalidArgumentsError);
  // A parent key is not a string.
  EXPECT_EQ(SendMessageExpectRejected(kSetPrefMessage,
                                      base::ListValue()
                                          .Append(kGaiaId)
                                          .Append(base::ListValue().Append(1))
                                          .Append(kIntegerKey)
                                          .Append(1)),
            kInvalidArgumentsError);
  // Key is not a string.
  EXPECT_EQ(
      SendMessageExpectRejected(kSetPrefMessage, base::ListValue()
                                                     .Append(kGaiaId)
                                                     .Append(base::ListValue())
                                                     .Append(1)
                                                     .Append(1)),
      kInvalidArgumentsError);
  EXPECT_TRUE(GetAccountPrefs().empty());

  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(3)));
  EXPECT_EQ(SendMessageExpectRejected(kDeletePrefMessage,
                                      base::ListValue()
                                          .Append(1)
                                          .Append(base::ListValue())
                                          .Append(kIntegerKey)),
            kInvalidArgumentsError);
  EXPECT_EQ(SendMessageExpectRejected(
                kDeletePrefMessage,
                base::ListValue().Append(kGaiaId).Append(kDictKey).Append(
                    kIntegerKey)),
            kInvalidArgumentsError);
  EXPECT_EQ(SendMessageExpectRejected(kDeletePrefMessage,
                                      base::ListValue()
                                          .Append(kGaiaId)
                                          .Append(base::ListValue().Append(1))
                                          .Append(kIntegerKey)),
            kInvalidArgumentsError);
  EXPECT_EQ(SendMessageExpectRejected(kDeletePrefMessage,
                                      base::ListValue()
                                          .Append(kGaiaId)
                                          .Append(base::ListValue())
                                          .Append(1)),
            kInvalidArgumentsError);
  EXPECT_EQ(GetTestAccountPrefs()->FindInt(kIntegerKey), 3);
}

TEST_F(SigninPrefsInternalsHandlerTest,
       GetPrefsAndMutationsRejectedForNonRegularProfile) {
  // Seed a pref on the original profile to verify an OTR handler does not leak
  // or mutate its original profile's prefs.
  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(3)));

  Profile* otr_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);
  TestingProfile* guest_profile = testing_profile_manager_.CreateGuestProfile();
  ASSERT_TRUE(guest_profile);

  // Also seed a pref directly on the Guest profile.
  SigninPrefs guest_prefs(*guest_profile->GetPrefs());
  guest_prefs.SetChromeSigninInterceptionUserChoice(
      GaiaId(kGaiaId), ChromeSigninUserChoice::kSignin);

  std::vector<Profile*> non_regular_profiles = {nullptr, otr_profile,
                                                guest_profile};
#if BUILDFLAG(IS_CHROMEOS)
  // An Ash internal profile has `IsRegularProfile() == true` while
  // `profiles::IsRegularUserProfile()` is `false`.
  TestingProfile* signin_profile =
      testing_profile_manager_.CreateTestingProfile(
          ash::kSigninBrowserContextBaseName);
  ASSERT_TRUE(signin_profile);
  ASSERT_TRUE(signin_profile->IsRegularProfile());
  SigninPrefs signin_profile_prefs(*signin_profile->GetPrefs());
  signin_profile_prefs.SetChromeSigninInterceptionUserChoice(
      GaiaId(kGaiaId), ChromeSigninUserChoice::kSignin);
  non_regular_profiles.push_back(signin_profile);
#endif  // BUILDFLAG(IS_CHROMEOS)

  for (Profile* non_regular : non_regular_profiles) {
    if (non_regular) {
      ASSERT_FALSE(profiles::IsRegularUserProfile(non_regular));
    }
    content::TestWebUI non_regular_web_ui;
    non_regular_web_ui.AddMessageHandler(
        std::make_unique<SigninPrefsInternalsHandler>(non_regular));

    const Response get_response = SendWebUIMessage(
        non_regular_web_ui, kGetPrefsMessage, base::ListValue());
    ASSERT_TRUE(get_response.resolved);
    ASSERT_TRUE(get_response.value.is_dict());
    const base::DictValue& info = get_response.value.GetDict();
    EXPECT_EQ(info.FindBool("canEdit"), false);
    ASSERT_TRUE(info.FindDict("accountEmails"));
    EXPECT_TRUE(info.FindDict("accountEmails")->empty());
    ASSERT_TRUE(info.FindDict("prefs"));
    EXPECT_TRUE(info.FindDict("prefs")->empty());

    const Response set_response =
        SendWebUIMessage(non_regular_web_ui, kSetPrefMessage,
                         SetPrefArgs(kGaiaId, {}, kIntegerKey, base::Value(9)));
    EXPECT_FALSE(set_response.resolved);
    EXPECT_EQ(set_response.value, base::Value(kEditingDisabledError));

    const Response delete_response =
        SendWebUIMessage(non_regular_web_ui, kDeletePrefMessage,
                         DeletePrefArgs(kGaiaId, {}, kIntegerKey));
    EXPECT_FALSE(delete_response.resolved);
    EXPECT_EQ(delete_response.value, base::Value(kEditingDisabledError));
  }

  // None of the profiles' prefs were modified.
  ASSERT_TRUE(GetTestAccountPrefs());
  EXPECT_EQ(GetTestAccountPrefs()->FindInt(kIntegerKey), 3);
  EXPECT_EQ(guest_prefs.GetChromeSigninInterceptionUserChoice(GaiaId(kGaiaId)),
            ChromeSigninUserChoice::kSignin);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(signin_profile_prefs.GetChromeSigninInterceptionUserChoice(
                GaiaId(kGaiaId)),
            ChromeSigninUserChoice::kSignin);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(SigninPrefsInternalsHandlerTest, EditingGatedByInternalOnlyUisPref) {
  const AccountInfo account =
      identity_test_env()->MakeAccountAvailable("a@b.com");
  ASSERT_TRUE(SetPref({}, kIntegerKey, base::Value(3)));

  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      chrome_urls::kInternalOnlyUisEnabled, false);

  // Reading still works when internal debug UIs are disabled, with `canEdit`
  // reporting false.
  const base::DictValue read_only_info = GetPrefs();
  EXPECT_EQ(read_only_info.FindBool("canEdit"), false);
  const base::DictValue* account_emails =
      read_only_info.FindDict("accountEmails");
  ASSERT_TRUE(account_emails);
  EXPECT_TRUE(account_emails->contains(account.GetGaiaId().ToString()));
  const base::DictValue* prefs = read_only_info.FindDict("prefs");
  ASSERT_TRUE(prefs);
  const base::DictValue* account_prefs = prefs->FindDict(kGaiaId);
  ASSERT_TRUE(account_prefs);
  EXPECT_EQ(account_prefs->FindInt(kIntegerKey), 3);

  // Mutations are rejected without modifying prefs.
  EXPECT_EQ(SetPrefExpectRejected({}, kIntegerKey, base::Value(9)),
            kEditingDisabledError);
  EXPECT_EQ(DeletePrefExpectRejected({}, kIntegerKey), kEditingDisabledError);
  ASSERT_TRUE(GetTestAccountPrefs());
  EXPECT_EQ(GetTestAccountPrefs()->FindInt(kIntegerKey), 3);

  // Invalid inputs are still rejected even when editing is disabled.
  EXPECT_EQ(SetPrefExpectRejected({}, "NotARegisteredPref", base::Value(1)),
            kUnregisteredError);
  EXPECT_EQ(DeletePrefExpectRejected({}, "NotARegisteredPref"),
            kUnregisteredError);

  // Re-enabling internal debug UIs restores editing.
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      chrome_urls::kInternalOnlyUisEnabled, true);
  const base::DictValue editable_info = GetPrefs();
  EXPECT_EQ(editable_info.FindBool("canEdit"), true);
  EXPECT_TRUE(SetPref({}, kIntegerKey, base::Value(9)));
  EXPECT_EQ(GetTestAccountPrefs()->FindInt(kIntegerKey), 9);
  EXPECT_TRUE(DeletePref({}, kIntegerKey));
  EXPECT_FALSE(GetTestAccountPrefs()->FindInt(kIntegerKey));
}

}  // namespace
