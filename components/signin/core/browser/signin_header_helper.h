// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_

#include <map>
#include <string>
#include <vector>

#include "components/prefs/pref_member.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/gurl.h"

namespace content_settings {
class CookieSettings;
}

namespace net {
class URLRequest;
}

namespace signin {

// Profile mode flags.
enum ProfileMode {
  PROFILE_MODE_DEFAULT = 0,
  // Incognito mode disabled by enterprise policy or by parental controls.
  PROFILE_MODE_INCOGNITO_DISABLED = 1 << 0,
  // Adding account disabled in the Android-for-EDU mode and for child accounts.
  PROFILE_MODE_ADD_ACCOUNT_DISABLED = 1 << 1
};

extern const char kChromeConnectedHeader[];
extern const char kDiceRequestHeader[];
extern const char kDiceResponseHeader[];

// The ServiceType specified by Gaia in the response header accompanying the 204
// response. This indicates the action Chrome is supposed to lead the user to
// perform.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin
enum GAIAServiceType : int {
  GAIA_SERVICE_TYPE_NONE = 0,    // No Gaia response header.
  GAIA_SERVICE_TYPE_SIGNOUT,     // Logout all existing sessions.
  GAIA_SERVICE_TYPE_INCOGNITO,   // Open an incognito tab.
  GAIA_SERVICE_TYPE_ADDSESSION,  // Add a secondary account.
  GAIA_SERVICE_TYPE_SIGNUP,      // Create a new account.
  GAIA_SERVICE_TYPE_DEFAULT,     // All other cases.
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
  GAIAServiceType service_type;
  // The prefilled email.
  std::string email;
  // Whether |email| is a saml account.
  bool is_saml;
  // The continue URL after the requested service is completed successfully.
  // Defaults to the current URL if empty.
  std::string continue_url;
  // Whether the continue URL should be loaded in the same tab.
  bool is_same_tab;

  ManageAccountsParams();
  ManageAccountsParams(const ManageAccountsParams& other);
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
  ~DiceResponseParams();
  DiceResponseParams(DiceResponseParams&&);
  DiceResponseParams& operator=(DiceResponseParams&&);

  DiceAction user_intention = DiceAction::NONE;

  // Populated when |user_intention| is SIGNIN.
  std::unique_ptr<SigninInfo> signin_info;

  // Populated when |user_intention| is SIGNOUT.
  std::unique_ptr<SignoutInfo> signout_info;

  // Populated when |user_intention| is ENABLE_SYNC.
  std::unique_ptr<EnableSyncInfo> enable_sync_info;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiceResponseParams);
};

class RequestAdapter {
 public:
  explicit RequestAdapter(net::URLRequest* request);
  virtual ~RequestAdapter();

  virtual const GURL& GetUrl();
  virtual bool HasHeader(const std::string& name);
  virtual void RemoveRequestHeaderByName(const std::string& name);
  virtual void SetExtraHeaderByName(const std::string& name,
                                    const std::string& value);

 protected:
  net::URLRequest* const request_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestAdapter);
};

// Base class for managing the signin headers (Dice and Chrome-Connected).
class SigninHeaderHelper {
 public:
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

 protected:
  SigninHeaderHelper();
  virtual ~SigninHeaderHelper();

  // Dictionary of fields in a account consistency response header.
  using ResponseHeaderDictionary = std::multimap<std::string, std::string>;

  // Parses the account consistency response header. Its expected format is
  // "key1=value1,key2=value2,...".
  static ResponseHeaderDictionary ParseAccountConsistencyResponseHeader(
      const std::string& header_value);

 private:
  // Returns whether the url is eligible for the request header.
  virtual bool IsUrlEligibleForRequestHeader(const GURL& url) = 0;

  DISALLOW_COPY_AND_ASSIGN(SigninHeaderHelper);
};


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
void AppendOrRemoveMirrorRequestHeader(
    RequestAdapter* request,
    const GURL& redirect_url,
    const std::string& gaia_id,
    AccountConsistencyMethod account_consistency,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask);

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
