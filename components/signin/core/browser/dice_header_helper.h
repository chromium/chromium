// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_DICE_HEADER_HELPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_DICE_HEADER_HELPER_H_

#include <string>

#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "google_apis/gaia/core_account_id.h"

class GURL;

namespace signin {

// Version of the Dice protocol.
extern const char kDiceProtocolVersion[];

// SigninHeaderHelper implementation managing the Dice header.
class DiceHeaderHelper : public SigninHeaderHelper {
 public:
  explicit DiceHeaderHelper(AccountConsistencyMethod account_consistency);

  DiceHeaderHelper(const DiceHeaderHelper&) = delete;
  DiceHeaderHelper& operator=(const DiceHeaderHelper&) = delete;

  ~DiceHeaderHelper() override = default;

  // Returns the parameters contained in the X-Chrome-ID-Consistency-Response
  // response header.
  static DiceResponseParams BuildDiceSigninResponseParams(
      const std::string& header_value);

  // Returns the parameters contained in the Google-Accounts-SignOut response
  // header.
  static DiceResponseParams BuildDiceSignoutResponseParams(
      const std::string& header_value);

  // Returns the header value for Dice requests. Returns the empty string when
  // the header must not be added.
  // |sync_gaia_id| is not empty if Sync is currently enabled for this
  // account.
  // |show_signout_confirmation| is true if Gaia must display the signout
  // confirmation dialog.
  std::string BuildRequestHeader(const std::string& sync_gaia_id,
                                 const std::string& device_id);

  // SigninHeaderHelper implementation:
  bool ShouldBuildRequestHeader(
      const GURL& url,
      const content_settings::CookieSettings* cookie_settings) override;

 private:
  // SigninHeaderHelper implementation:
  bool IsUrlEligibleForRequestHeader(const GURL& url) override;

  AccountConsistencyMethod account_consistency_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_DICE_HEADER_HELPER_H_
