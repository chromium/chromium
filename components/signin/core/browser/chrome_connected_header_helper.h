// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_CHROME_CONNECTED_HEADER_HELPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_CHROME_CONNECTED_HEADER_HELPER_H_

#include <string>

#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/account_consistency_method.h"

class GURL;

namespace signin {

enum class Tribool;

// Name of the cookie used by Chrome sign-in to inform GAIA that an
// authenticating user is already signed in to Chrome. Because it is not
// possible to intercept headers from iOS WKWebView, Chrome requires this cookie
// to communicate its signed-in state with GAIA.
extern const char kChromeConnectedCookieName[];

// SigninHeaderHelper implementation managing the "X-Chrome-Connected" header.
class ChromeConnectedHeaderHelper : public SigninHeaderHelper {
 public:
  explicit ChromeConnectedHeaderHelper(
      AccountConsistencyMethod account_consistency);
  ~ChromeConnectedHeaderHelper() override = default;

  // Returns the Chrome-Connected cookie, or an empty string if it should not be
  // added to the request to |url|.
  static std::string BuildRequestCookieIfPossible(
      const GURL& url,
      const std::string& gaia_id,
      AccountConsistencyMethod account_consistency,
      const content_settings::CookieSettings* cookie_settings,
      int profile_mode_mask);

  // Returns the parameters contained in the X-Chrome-Manage-Accounts response
  // header.
  static ManageAccountsParams BuildManageAccountsParams(
      const std::string& header_value);

  // Returns the value for the Chrome-Connected request header. May return the
  // empty string, in this case the header must not be added.
  std::string BuildRequestHeader(bool is_header_request,
                                 const GURL& url,
                                 const std::string& gaia_id,
                                 Tribool is_child_account,
                                 int profile_mode_mask,
                                 const std::string& source,
                                 bool force_account_consistency);

  // SigninHeaderHelper implementation:
  bool ShouldBuildRequestHeader(
      const GURL& url,
      const content_settings::CookieSettings* cookie_settings) override;

  // SigninHeaderHelper implementation:
  bool IsUrlEligibleForRequestHeader(const GURL& url) override;

 private:
  // Whether mirror account consistency should be used.
  AccountConsistencyMethod account_consistency_;

  // Returns whether the URL is eligible for the Gaia ID parameter.
  bool IsUrlEligibleToIncludeGaiaId(const GURL& url, bool is_header_request);

  // Returns whether the URL has a Google Drive origin.
  bool IsDriveOrigin(const GURL& url);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_CHROME_CONNECTED_HEADER_HELPER_H_
