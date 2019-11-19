// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_CHROME_CONNECTED_HEADER_HELPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_CHROME_CONNECTED_HEADER_HELPER_H_

#include <string>

#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/account_consistency_method.h"

class GURL;

namespace signin {

// SigninHeaderHelper implementation managing the "X-Chrome-Connected" header.
class ChromeConnectedHeaderHelper : public SigninHeaderHelper {
 public:
  explicit ChromeConnectedHeaderHelper(
      AccountConsistencyMethod account_consistency);
  ~ChromeConnectedHeaderHelper() override {}

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
                                 int profile_mode_mask);

  // SigninHeaderHelper implementation:
  bool ShouldBuildRequestHeader(
      const GURL& url,
      const content_settings::CookieSettings* cookie_settings) override;

 private:
  // Whether mirror account consistency should be used.
  AccountConsistencyMethod account_consistency_;

  // Returns whether the URL is eligible for the Gaia ID parameter.
  bool IsUrlEligibleToIncludeGaiaId(const GURL& url, bool is_header_request);

  // Returns whether the URL has a Google Drive origin.
  bool IsDriveOrigin(const GURL& url);

  // SigninHeaderHelper implementation:
  bool IsUrlEligibleForRequestHeader(const GURL& url) override;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_CHROME_CONNECTED_HEADER_HELPER_H_
