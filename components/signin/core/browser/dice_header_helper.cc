// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_header_helper.h"

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"

namespace signin {

const char kDiceProtocolVersion[] = "1";

namespace {

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

// Signout response parameters.
const char kSignoutEmailAttrName[] = "email";
const char kSignoutSessionIndexAttrName[] = "sessionindex";
const char kSignoutObfuscatedIDAttrName[] = "obfuscatedid";

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

}  // namespace

DiceHeaderHelper::DiceHeaderHelper(AccountConsistencyMethod account_consistency)
    : account_consistency_(account_consistency) {}

// static
DiceResponseParams DiceHeaderHelper::BuildDiceSigninResponseParams(
    const std::string& header_value) {
  DCHECK(!header_value.empty());
  DiceResponseParams params;
  ResponseHeaderDictionary header_dictionary =
      ParseAccountConsistencyResponseHeader(header_value);
  auto action_it = header_dictionary.find(kSigninActionAttrName);
  if (action_it == header_dictionary.end()) {
    return params;
  }

  DiceResponseParams::AccountInfo account_info;
  std::string authorization_code;
  bool no_authorization_code = false;
  std::string supported_algorithms_for_token_binding;

  ResponseHeaderDictionary::const_iterator it = header_dictionary.begin();
  for (; it != header_dictionary.end(); ++it) {
    const std::string key_name(it->first);
    const std::string value(it->second);
    if (key_name == kSigninActionAttrName) {
      // Do nothing, handled separately below.
    } else if (key_name == kSigninIdAttrName) {
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
    } else {
      DLOG(WARNING) << "Unexpected Gaia header attribute '" << key_name << "'.";
    }
  }

  switch (GetDiceActionFromHeader(action_it->second)) {
    case DiceAction::NONE:
    case DiceAction::SIGNOUT:
      DLOG(WARNING) << "Only SIGNIN and ENABLE_SYNC are supported through "
                    << "X-Chrome-ID-Consistency-Response :" << header_value;
      return params;
    case DiceAction::SIGNIN:
      params.user_intention = DiceAction::SIGNIN;
      params.signin_info = std::make_unique<DiceResponseParams::SigninInfo>();
      params.signin_info->AddAccount(
          {std::move(account_info), std::move(authorization_code),
           no_authorization_code,
           std::move(supported_algorithms_for_token_binding)});
      break;
    case DiceAction::ENABLE_SYNC:
      params.user_intention = DiceAction::ENABLE_SYNC;
      params.enable_sync_info =
          std::make_unique<DiceResponseParams::EnableSyncInfo>();
      params.enable_sync_info->account_info = std::move(account_info);
      if (!authorization_code.empty()) {
        DLOG(WARNING) << "Authorization code expected only with SIGNIN action";
      }
      if (no_authorization_code) {
        DLOG(WARNING)
            << "No authorization code header expected only with SIGNIN action";
      }
      if (!supported_algorithms_for_token_binding.empty()) {
        DLOG(WARNING) << "Eligible for token binding attribute expected only "
                         "with SIGNIN action";
      }
      break;
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
  params.user_intention = DiceAction::SIGNOUT;
  std::vector<GaiaId> gaia_ids;
  std::vector<std::string> emails;
  std::vector<int> session_indices;
  ResponseHeaderDictionary header_dictionary =
      ParseAccountConsistencyResponseHeader(header_value);
  ResponseHeaderDictionary::const_iterator it = header_dictionary.begin();
  for (; it != header_dictionary.end(); ++it) {
    const std::string key_name(it->first);
    const std::string value(it->second);
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

  params.signout_info = std::make_unique<DiceResponseParams::SignoutInfo>();
  for (size_t i = 0; i < gaia_ids.size(); ++i) {
    params.signout_info->account_infos.emplace_back(gaia_ids[i], emails[i],
                                                    session_indices[i]);
  }

  return params;
}

bool DiceHeaderHelper::ShouldBuildRequestHeader(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings) {
  return IsUrlEligibleForRequestHeader(url);
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
