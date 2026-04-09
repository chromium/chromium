// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_header_helper.h"

#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_response_headers.h"

namespace signin {

const char kDiceProtocolVersion[] = "1";
const char kGoogleSignoutResponseHeader[] = "Google-Accounts-SignOut";

namespace {

using DiceResponseHeaderDictionary = base::flat_map<std::string, std::string>;

// Request parameters.
const char kRequestSigninAll[] = "all_accounts";

// Signin response parameters.
const char kSigninActionAttrName[] = "action";
const char kSigninAuthUserAttrName[] = "authuser";
const char kSigninAuthorizationCodeAttrName[] = "authorization_code";
const char kSigninNoAuthorizationCodeAttrName[] = "no_authorization_code";
const char kSigninEmailAttrName[] = "email";
const char kSigninIdAttrName[] = "id";
const char kSigninEligibleForTokenBindingAttrName[] =
    "eligible_for_token_binding";
const char kSigninMtlsTokenBindingAttrName[] = "mtls_token_binding";

// Signout response parameters.
const char kSignoutEmailAttrName[] = "email";
const char kSignoutSessionIndexAttrName[] = "sessionindex";
const char kSignoutObfuscatedIDAttrName[] = "obfuscatedid";

// LinkedAccounts metadata response parameters.
constexpr char kLinkedAccountsInitiatorIdAttrName[] = "initiator_id";
constexpr char kLinkedAccountsPrimaryIsConnectedAttrName[] =
    "primary_is_connected";

// Determines the Dice action that has been passed from Gaia in the header.
DiceAction GetDiceActionFromHeader(const std::string& value) {
  if (value == "SIGNIN") {
    return DiceAction::SIGNIN;
  } else if (value == "SIGNOUT") {
    return DiceAction::SIGNOUT;
  } else if (value == "ENABLE_SYNC") {
    return DiceAction::ENABLE_SYNC;
  } else {
    return DiceAction::NONE;
  }
}

// Helper to parse a single account consistency group (comma or semicolon
// separated).
// Expects one of the following formats:
// * delimiter="," group="key1=value1,key2=value2,key3=value3"
// * delimiter=";" group="key1=value1;key2=value2;key3=value3"
// Returns the dictionary containing the keys and values.
DiceResponseHeaderDictionary ParseGroup(std::string_view group,
                                        std::string_view delimiter) {
  DiceResponseHeaderDictionary dictionary;
  for (std::string_view field :
       base::SplitStringPiece(group, delimiter, base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    size_t delim = field.find_first_of('=');
    if (delim == std::string::npos) {
      continue;
    }
    dictionary.insert({std::string(field.substr(0, delim)),
                       base::UnescapeURLComponent(
                           field.substr(delim + 1),
                           base::UnescapeRule::PATH_SEPARATORS |
                               base::UnescapeRule::
                                   URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS)});
  }
  return dictionary;
}

DiceResponseParams::SigninInfo::SigninAccount BuildSigninAccount(
    const DiceResponseHeaderDictionary& dict) {
  DiceResponseParams::AccountInfo account_info;
  std::string authorization_code;
  bool no_authorization_code = false;
  std::string supported_algorithms_for_token_binding;
  bool mtls_token_binding = false;

  for (const auto& [key_name, value] : dict) {
    if (key_name == kSigninIdAttrName) {
      account_info.gaia_id = GaiaId(value);
    } else if (key_name == kSigninEmailAttrName) {
      account_info.email = value;
    } else if (key_name == kSigninAuthUserAttrName) {
      bool parse_success =
          base::StringToInt(value, &account_info.session_index);
      if (!parse_success) {
        account_info.session_index =
            DiceResponseParams::AccountInfo::kInvalidSessionIndex;
      }
    } else if (key_name == kSigninAuthorizationCodeAttrName) {
      authorization_code = value;
    } else if (key_name == kSigninNoAuthorizationCodeAttrName) {
      no_authorization_code = true;
    } else if (key_name == kSigninEligibleForTokenBindingAttrName) {
      supported_algorithms_for_token_binding = value;
    } else if (key_name == kSigninMtlsTokenBindingAttrName) {
      if (base::EqualsCaseInsensitiveASCII(value, "true") &&
          base::FeatureList::IsEnabled(switches::kEnableMtlsTokenBinding)) {
        mtls_token_binding = true;
      }
    } else if (key_name == kSigninActionAttrName) {
      // Handled separately initially.
    } else {
      DLOG(WARNING) << "Unexpected Gaia header attribute '" << key_name << "'.";
    }
  }

  return {std::move(account_info), std::move(authorization_code),
          no_authorization_code,
          std::move(supported_algorithms_for_token_binding),
          mtls_token_binding};
}

}  // namespace

DiceHeaderHelper::DiceHeaderHelper(AccountConsistencyMethod account_consistency)
    : account_consistency_(account_consistency) {}

// static
DiceResponseParams DiceHeaderHelper::BuildDiceSigninResponseParams(
    const std::string& header_value) {
  DCHECK(!header_value.empty());
  DiceResponseParams params;

  // The header can be in two formats:
  // 1. Legacy: action=SIGNIN,id=id1,email=email1
  // 2. Grouped: action=SIGNIN;id=id1;email=email1,id=id2;email=email2
  //
  // In the grouped format, attributes within an account are separated by ';',
  // and accounts are separated by ','.
  bool is_grouped_format = header_value.find(';') != std::string::npos;
  std::vector<DiceResponseHeaderDictionary> parsed_accounts;

  if (is_grouped_format) {
    for (std::string_view group :
         base::SplitStringPiece(header_value, ",", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY)) {
      parsed_accounts.push_back(ParseGroup(group, ";"));
    }
  } else {
    // Legacy format.
    parsed_accounts.push_back(ParseGroup(header_value, ","));
  }

  if (parsed_accounts.empty()) {
    return params;
  }

  // `parsed_accounts[0]` should contain an action, and one account info.
  // For other items in `parsed_accounts`, only the account info is used. The
  // action (if any) is assumed to be the same as in `parsed_accounts[0]`.
  auto action_it = parsed_accounts[0].find(kSigninActionAttrName);
  if (action_it == parsed_accounts[0].end()) {
    return params;
  }

  DiceAction action = GetDiceActionFromHeader(action_it->second);
  switch (action) {
    case DiceAction::SIGNIN:
      params.data.emplace<DiceResponseParams::SigninInfo>();
      break;
    case DiceAction::ENABLE_SYNC:
      params.data.emplace<DiceResponseParams::EnableSyncInfo>();
      break;
    default:
      DLOG(WARNING) << "Only SIGNIN and ENABLE_SYNC are supported through "
                    << "X-Chrome-ID-Consistency-Response :" << header_value;
      return params;
  }

  if (action == DiceAction::ENABLE_SYNC) {
    DiceResponseParams::SigninInfo::SigninAccount signin_account =
        BuildSigninAccount(parsed_accounts[0]);
    if (!signin_account.authorization_code.empty()) {
      DLOG(WARNING) << "Authorization code expected only with SIGNIN action";
    }
    if (signin_account.no_authorization_code) {
      DLOG(WARNING)
          << "No authorization code header expected only with SIGNIN action";
    }
    if (!signin_account.supported_algorithms_for_token_binding.empty()) {
      DLOG(WARNING) << "Eligible for token binding attribute expected only "
                       "with SIGNIN action";
    }
    params.enable_sync_info()->account_info =
        std::move(signin_account.account_info);
    return params;
  }

  // SIGNIN action.
  // Precaution against duplicate accounts in the header. The first one wins.
  base::flat_set<GaiaId> seen_ids;
  for (const auto& dict : parsed_accounts) {
    DiceResponseParams::SigninInfo::SigninAccount account =
        BuildSigninAccount(dict);
    if (!seen_ids.insert(account.account_info.gaia_id).second) {
      continue;
    }
    params.signin_info()->AddAccount(std::move(account));
  }

  return params;
}

// static
DiceResponseParams DiceHeaderHelper::BuildDiceSignoutResponseParams(
    const std::string& header_value) {
  // Google internal documentation of this header at:
  // http://go/gaia-response-headers
  DCHECK(!header_value.empty());
  DiceResponseParams params;
  DiceResponseParams::SignoutInfo& signout_info =
      params.data.emplace<DiceResponseParams::SignoutInfo>();
  std::vector<GaiaId> gaia_ids;
  std::vector<std::string> emails;
  std::vector<int> session_indices;
  ResponseHeaderDictionary header_dictionary =
      ParseAccountConsistencyResponseHeader(header_value);
  for (const auto& [key_name, value] : header_dictionary) {
    if (key_name == kSignoutObfuscatedIDAttrName) {
      std::string trimmed_value = value;
      // The Gaia ID is wrapped in quotes.
      base::TrimString(value, "\"", &trimmed_value);
      gaia_ids.emplace_back(std::move(trimmed_value));
    } else if (key_name == kSignoutEmailAttrName) {
      // The email is wrapped in quotes.
      emails.push_back(value);
      base::TrimString(value, "\"", &emails.back());
    } else if (key_name == kSignoutSessionIndexAttrName) {
      int session_index = -1;
      bool parse_success = base::StringToInt(value, &session_index);
      if (parse_success) {
        session_indices.push_back(session_index);
      }
    } else {
      DLOG(WARNING) << "Unexpected Gaia header attribute '" << key_name << "'.";
    }
  }

  if (gaia_ids.size() != emails.size() ||
      gaia_ids.size() != session_indices.size()) {
    return params;
  }

  for (size_t i = 0; i < gaia_ids.size(); ++i) {
    signout_info.account_infos.emplace_back(gaia_ids[i], emails[i],
                                            session_indices[i]);
  }

  return params;
}

// static
DiceResponseParams::SigninInfo::LinkedAccountsMetadata
DiceHeaderHelper::ParseLinkedAccountsMetadata(const std::string& header_value) {
  if (header_value.empty()) {
    return DiceResponseParams::SigninInfo::LinkedAccountsMetadata();
  }

  DiceResponseParams::SigninInfo::LinkedAccountsMetadata metadata;
  for (std::string_view field :
       base::SplitStringPiece(header_value, ";", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    size_t delim = field.find_first_of('=');
    if (delim == std::string::npos) {
      continue;
    }

    std::string_view key = field.substr(0, delim);
    std::string value = base::UnescapeURLComponent(
        field.substr(delim + 1),
        base::UnescapeRule::PATH_SEPARATORS |
            base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

    if (key == kLinkedAccountsInitiatorIdAttrName) {
      metadata.initiator_id = GaiaId(value);
    } else if (key == kLinkedAccountsPrimaryIsConnectedAttrName) {
      metadata.primary_is_connected =
          (value == "1") ? Tribool::kTrue : Tribool::kFalse;
    }
  }

  return metadata;
}

// static
DiceResponseParams DiceHeaderHelper::CreateDiceResponseParams(
    const net::HttpResponseHeaders* response_headers) {
  if (!response_headers) {
    return DiceResponseParams();
  }

  DiceResponseParams params;
  std::optional<std::string> dice_header =
      response_headers->GetNormalizedHeader(kDiceResponseHeader);
  std::optional<std::string> signout_header =
      response_headers->GetNormalizedHeader(kGoogleSignoutResponseHeader);
  std::optional<std::string> meta_header =
      response_headers->GetNormalizedHeader(kDiceLinkedAccountsMetaHeader);

  if (dice_header) {
    params = BuildDiceSigninResponseParams(*dice_header);
    if (DiceResponseParams::SigninInfo* signin_info = params.signin_info();
        signin_info && meta_header) {
      signin_info->set_linked_accounts_metadata(
          ParseLinkedAccountsMetadata(*meta_header));
      if (!signin_info->linked_accounts_metadata().IsValid()) {
        DLOG(WARNING)
            << "Malformed X-Chrome-ID-Consistency-Linked-Accounts-Meta header: "
            << *meta_header;
      }
    }
  } else if (signout_header) {
    params = BuildDiceSignoutResponseParams(*signout_header);
  }

  if (!params.signin_info() && meta_header) {
    DLOG(WARNING) << "X-Chrome-ID-Consistency-Linked-Accounts-Meta is only "
                     "supported for Sign-in Dice action";
  }

  if (!params.IsValid()) {
    if (dice_header) {
      DLOG(WARNING) << "Invalid DICE header: " << *dice_header;
    }
    if (signout_header) {
      DLOG(WARNING) << "Invalid Signout header: " << *signout_header;
    }
    if (meta_header) {
      DLOG(WARNING) << "Associated Meta header: " << *meta_header;
    }
  }

  return params;
}

// static
bool DiceHeaderHelper::AppendOrRemoveDiceRequestHeader(
    RequestAdapter* request,
    const GURL& redirect_url,
    const GaiaId& gaia_id,
    bool sync_enabled,
    AccountConsistencyMethod account_consistency,
    const std::string& device_id) {
  const GURL& url = redirect_url.is_empty() ? request->GetUrl() : redirect_url;
  DiceHeaderHelper dice_helper(account_consistency);
  std::string dice_header_value;
  if (dice_helper.IsUrlEligibleForRequestHeader(url)) {
    dice_header_value = dice_helper.BuildRequestHeader(
        sync_enabled ? gaia_id : GaiaId(), device_id);
  }
  return dice_helper.AppendOrRemoveRequestHeader(
      request, redirect_url, kDiceRequestHeader, dice_header_value);
}

bool DiceHeaderHelper::IsUrlEligibleForRequestHeader(const GURL& url) {
  if (account_consistency_ != AccountConsistencyMethod::kDice) {
    return false;
  }

  return gaia::HasGaiaSchemeHostPort(url);
}

std::string DiceHeaderHelper::BuildRequestHeader(const GaiaId& sync_gaia_id,
                                                 const std::string& device_id) {
  std::vector<std::string> parts;
  parts.push_back(base::StringPrintf("version=%s", kDiceProtocolVersion));
  parts.push_back("client_id=" +
                  GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  if (!device_id.empty()) {
    parts.push_back("device_id=" + device_id);
  }
  if (!sync_gaia_id.empty()) {
    parts.push_back("sync_account_id=" + sync_gaia_id.ToString());
  }

  // Restrict Signin to Sync account only when fixing auth errors.
  std::string signin_mode = kRequestSigninAll;
  parts.push_back("signin_mode=" + signin_mode);

  // Show the signout confirmation when Dice is enabled.
  parts.push_back("signout_mode=show_confirmation");

  return base::JoinString(parts, ",");
}

}  // namespace signin
