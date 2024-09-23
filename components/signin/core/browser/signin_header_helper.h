// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/gurl.h"

namespace content_settings {
class CookieSettings;
}

namespace net {
class HttpRequestHeaders;
}

namespace signin {

enum class Tribool;

// Profile mode flags.
enum ProfileMode {
  PROFILE_MODE_DEFAULT = 0,
  // Incognito mode disabled by enterprise policy or by parental controls.
  PROFILE_MODE_INCOGNITO_DISABLED = 1 << 0,
  // Adding account disabled in the Android-for-EDU mode and for child accounts.
  PROFILE_MODE_ADD_ACCOUNT_DISABLED = 1 << 1
};

extern const char kChromeConnectedHeader[];
extern const char kChromeManageAccountsHeader[];
extern const char kDiceRequestHeader[];
extern const char kDiceResponseHeader[];

// The X-Auto-Login header detects when a user is prompted to enter their
// credentials on the Gaia sign-in page. It is sent with an empty email if the
// user is on the Gaia sign-in email page or a pre-filled email if the user has
// selected an account on the AccountChooser. X-Auto-Login is not sent following
// a reauth request.
extern const char kAutoLoginHeader[];

// The ServiceType specified by Gaia in the response header accompanying the 204
// response. This indicates the action Chrome is supposed to lead the user to
// perform.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin
// NOTE: This enum is persisted to histograms. Do not change or reorder
// values.
enum GAIAServiceType : int {
  GAIA_SERVICE_TYPE_NONE = 0,    // No Gaia response header.
  GAIA_SERVICE_TYPE_SIGNOUT,     // Logout all existing sessions.
  GAIA_SERVICE_TYPE_INCOGNITO,   // Open an incognito tab.
  GAIA_SERVICE_TYPE_ADDSESSION,  // Add or re-authenticate an account.
  GAIA_SERVICE_TYPE_SIGNUP,      // Create a new account.
  GAIA_SERVICE_TYPE_DEFAULT,     // All other cases.
  kMaxValue = GAIA_SERVICE_TYPE_DEFAULT
};

enum class DiceAction {
  NONE,
  SIGNIN,      // Sign in an account.
  SIGNOUT,     // Sign out of all sessions.
  ENABLE_SYNC  // Enable Sync on a signed-in account.
};

// Struct describing the parameters received in the manage account header.
struct ManageAccountsParams {
  // The requested service type such as "ADDSESSION".
  GAIAServiceType service_type = GAIA_SERVICE_TYPE_NONE;
  // The prefilled email.
  std::string email;
  // Whether |email| is a saml account.
  bool is_saml = false;
  // The continue URL after the requested service is completed successfully.
  // Defaults to the current URL if empty.
  std::string continue_url;
  // Whether the continue URL should be loaded in the same tab.
  bool is_same_tab = false;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Whether to show consistency promo.
  bool show_consistency_promo = false;
#endif

  ManageAccountsParams();
  ManageAccountsParams(const ManageAccountsParams& other);
  ManageAccountsParams& operator=(const ManageAccountsParams& other);
};

// Struct describing the parameters received in the Dice response header.
struct DiceResponseParams {
  struct AccountInfo {
    AccountInfo();
    AccountInfo(const std::string& gaia_id,
                const std::string& email,
                int session_index);
    ~AccountInfo();
    AccountInfo(const AccountInfo&);

    // Gaia ID of the account.
    std::string gaia_id;
    // Email of the account.
    std::string email;
    // Session index for the account.
    int session_index;
  };

  // Parameters for the SIGNIN action.
  struct SigninInfo {
    SigninInfo();
    SigninInfo(const SigninInfo&);
    ~SigninInfo();

    // AccountInfo of the account signed in.
    AccountInfo account_info;
    // Authorization code to fetch a refresh token.
    std::string authorization_code;
    // Whether Dice response contains the 'no_authorization_code' header value.
    // If true then LSO was unavailable for provision of auth code.
    bool no_authorization_code = false;
    // If the account is eligible for token binding, this string is non-empty
    // and contains a list of supported binding algorithms separated by space.
    std::string supported_algorithms_for_token_binding;
  };

  // Parameters for the SIGNOUT action.
  struct SignoutInfo {
    SignoutInfo();
    SignoutInfo(const SignoutInfo&);
    ~SignoutInfo();

    // Account infos for the accounts signed out.
    std::vector<AccountInfo> account_infos;
  };

  // Parameters for the ENABLE_SYNC action.
  struct EnableSyncInfo {
    EnableSyncInfo();
    EnableSyncInfo(const EnableSyncInfo&);
    ~EnableSyncInfo();

    // AccountInfo of the account enabling Sync.
    AccountInfo account_info;
  };

  DiceResponseParams();

  DiceResponseParams(const DiceResponseParams&) = delete;
  DiceResponseParams& operator=(const DiceResponseParams&) = delete;

  DiceResponseParams(DiceResponseParams&&);
  DiceResponseParams& operator=(DiceResponseParams&&);

  ~DiceResponseParams();

  DiceAction user_intention = DiceAction::NONE;

  // Populated when |user_intention| is SIGNIN.
  std::unique_ptr<SigninInfo> signin_info;

  // Populated when |user_intention| is SIGNOUT.
  std::unique_ptr<SignoutInfo> signout_info;

  // Populated when |user_intention| is ENABLE_SYNC.
  std::unique_ptr<EnableSyncInfo> enable_sync_info;
};

class RequestAdapter {
 public:
  RequestAdapter(const GURL& url,
                 const net::HttpRequestHeaders& original_headers,
                 net::HttpRequestHeaders* modified_headers,
                 std::vector<std::string>* headers_to_remove);

  RequestAdapter(const RequestAdapter&) = delete;
  RequestAdapter& operator=(const RequestAdapter&) = delete;

  virtual ~RequestAdapter();

  const GURL& GetUrl();
  bool HasHeader(const std::string& name);
  void RemoveRequestHeaderByName(const std::string& name);
  void SetExtraHeaderByName(const std::string& name, const std::string& value);

 private:
  const GURL url_;
  const raw_ref<const net::HttpRequestHeaders, DanglingUntriaged>
      original_headers_;
  const raw_ptr<net::HttpRequestHeaders> modified_headers_;
  const raw_ptr<std::vector<std::string>> headers_to_remove_;
};

// Base class for managing the signin headers (Dice and Chrome-Connected).
class SigninHeaderHelper {
 public:
  SigninHeaderHelper(const SigninHeaderHelper&) = delete;
  SigninHeaderHelper& operator=(const SigninHeaderHelper&) = delete;

  // Appends or remove the header to a network request if necessary.
  // Returns whether the request has the request header.
  bool AppendOrRemoveRequestHeader(RequestAdapter* request,
                                   const GURL& redirect_url,
                                   const char* header_name,
                                   const std::string& header_value);

  // Returns wether an account consistency header should be built for this
  // request.
  virtual bool ShouldBuildRequestHeader(
      const GURL& url,
      const content_settings::CookieSettings* cookie_settings) = 0;

  // Dictionary of fields in a account consistency response header.
  using ResponseHeaderDictionary = std::multimap<std::string, std::string>;

  // Parses the account consistency response header. Its expected format is
  // "key1=value1,key2=value2,...".
  static ResponseHeaderDictionary ParseAccountConsistencyResponseHeader(
      const std::string& header_value);

 protected:
  SigninHeaderHelper();
  virtual ~SigninHeaderHelper();

  // Returns whether the url is eligible for the request header.
  virtual bool IsUrlEligibleForRequestHeader(const GURL& url) = 0;
};

// Returns whether the url is eligible for account consistency on Google
// domains.
bool IsUrlEligibleForMirrorCookie(const GURL& url);

// Returns the CHROME_CONNECTED cookie, or an empty string if it should not be
// added to the request to |url|.
std::string BuildMirrorRequestCookieIfPossible(
    const GURL& url,
    const std::string& gaia_id,
    AccountConsistencyMethod account_consistency,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask);

// Adds the mirror header to all Gaia requests from a connected profile, with
// the exception of requests from gaia webview.
// Removes the header in case it should not be transfered to a redirected url.
// If |force_account_consistency| is true, the mirror header will still be added
// in cases where |gaia_id| is empty.
void AppendOrRemoveMirrorRequestHeader(
    RequestAdapter* request,
    const GURL& redirect_url,
    const std::string& gaia_id,
    Tribool is_child_account,
    AccountConsistencyMethod account_consistency,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask,
    const std::string& source,
    bool force_account_consistency);

// Adds the Dice to all Gaia requests from a connected profile, with the
// exception of requests from gaia webview.
// Removes the header in case it should not be transfered to a redirected url.
// Returns whether the request has the Dice request header.
bool AppendOrRemoveDiceRequestHeader(
    RequestAdapter* request,
    const GURL& redirect_url,
    const std::string& gaia_id,
    bool sync_enabled,
    AccountConsistencyMethod account_consistency,
    const content_settings::CookieSettings* cookie_settings,
    const std::string& device_id);

// Returns the parameters contained in the X-Chrome-Manage-Accounts response
// header.
ManageAccountsParams BuildManageAccountsParams(const std::string& header_value);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Returns the parameters contained in the X-Chrome-ID-Consistency-Response
// response header.
// Returns DiceAction::NONE in case of error (such as missing or malformed
// parameters).
DiceResponseParams BuildDiceSigninResponseParams(
    const std::string& header_value);

// Returns the parameters contained in the Google-Accounts-SignOut response
// header.
// Returns DiceAction::NONE in case of error (such as missing or malformed
// parameters).
DiceResponseParams BuildDiceSignoutResponseParams(
    const std::string& header_value);
#endif

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_
