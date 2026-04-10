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

namespace net {
class HttpResponseHeaders;
}

namespace signin {

// Version of the Dice protocol.
extern const char kDiceProtocolVersion[];
extern const char kGoogleSignoutResponseHeader[];

// SigninHeaderHelper implementation managing the Dice header.
class DiceHeaderHelper : public SigninHeaderHelper {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(DiceLinkedAccountsMetaHeaderStatus)
  enum class DiceLinkedAccountsMetaHeaderStatus {
    kHeaderMissing = 0,
    kValid = 1,
    kMissingInitiatorId = 2,
    kMissingPrimaryIsConnected = 3,
    kMissingBothParams = 4,
    kInitiatorMismatch = 5,
    kMaxValue = kInitiatorMismatch,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:DiceLinkedAccountsMetaHeaderStatus)

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

  // Returns the metadata contained in the
  // X-Chrome-ID-Consistency-Linked-Accounts-Meta response header.
  static DiceResponseParams::SigninInfo::LinkedAccountsMetadata
  ParseLinkedAccountsMetadata(const std::string& header_value);

  // Creates DiceResponseParams from the response headers by combining standard
  // Dice and Linked Accounts metadata.
  static DiceResponseParams CreateDiceResponseParams(
      const net::HttpResponseHeaders* response_headers);

  // Adds the Dice to all Gaia requests from a connected profile, with the
  // exception of requests from gaia webview.
  // Removes the header in case it should not be transferred to a redirected
  // url. Returns whether the request has the Dice request header.
  static bool AppendOrRemoveDiceRequestHeader(
      RequestAdapter* request,
      const GURL& redirect_url,
      const GaiaId& gaia_id,
      bool sync_enabled,
      AccountConsistencyMethod account_consistency,
      const std::string& device_id);

  // Returns the header value for Dice requests. Returns the empty string when
  // the header must not be added.
  // |sync_gaia_id| is not empty if Sync is currently enabled for this
  // account.
  // |show_signout_confirmation| is true if Gaia must display the signout
  // confirmation dialog.
  std::string BuildRequestHeader(const GaiaId& sync_gaia_id,
                                 const std::string& device_id);

 private:
  // SigninHeaderHelper implementation:
  bool IsUrlEligibleForRequestHeader(const GURL& url) override;

  AccountConsistencyMethod account_consistency_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_DICE_HEADER_HELPER_H_
