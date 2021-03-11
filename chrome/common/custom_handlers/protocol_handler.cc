// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/custom_handlers/protocol_handler.h"

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/util/values/values_util.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/origin_util.h"
#include "extensions/common/constants.h"
#include "net/base/escape.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "ui/base/l10n/l10n_util.h"

ProtocolHandler::ProtocolHandler(
    const std::string& protocol,
    const GURL& url,
    base::Time last_modified,
    blink::ProtocolHandlerSecurityLevel security_level)
    : protocol_(base::ToLowerASCII(protocol)),
      url_(url),
      last_modified_(last_modified),
      security_level_(security_level) {}

ProtocolHandler::ProtocolHandler(const ProtocolHandler&) = default;
ProtocolHandler::~ProtocolHandler() = default;

ProtocolHandler ProtocolHandler::CreateProtocolHandler(
    const std::string& protocol,
    const GURL& url,
    blink::ProtocolHandlerSecurityLevel security_level) {
  return ProtocolHandler(protocol, url, base::Time::Now(), security_level);
}

ProtocolHandler::ProtocolHandler(
    const std::string& protocol,
    const GURL& url,
    const std::string& app_id,
    base::Time last_modified,
    blink::ProtocolHandlerSecurityLevel security_level)
    : protocol_(base::ToLowerASCII(protocol)),
      url_(url),
      web_app_id_(app_id),
      last_modified_(last_modified),
      security_level_(security_level) {}

// static
ProtocolHandler ProtocolHandler::CreateWebAppProtocolHandler(
    const std::string& protocol,
    const GURL& url,
    const std::string& app_id) {
  return ProtocolHandler(protocol, url, app_id, base::Time::Now(),
                         blink::ProtocolHandlerSecurityLevel::kStrict);
}

ProtocolHandler::ProtocolHandler() = default;

bool ProtocolHandler::IsValidDict(const base::DictionaryValue* value) {
  // Note that "title" parameter is ignored.
  // The |last_modified| field is optional as it was introduced in M68.
  return value->HasKey("protocol") && value->HasKey("url");
}

bool ProtocolHandler::IsValid() const {
  // TODO(https://crbug.com/977083): Consider limiting to secure contexts.

  // This matches VerifyCustomHandlerURLSecurity() in blink's
  // NavigatorContentUtils.
  bool has_valid_scheme =
      url_.SchemeIsHTTPOrHTTPS() ||
      (security_level_ ==
           blink::ProtocolHandlerSecurityLevel::kExtensionFeatures &&
       url_.SchemeIs(extensions::kExtensionScheme));
  if (!has_valid_scheme)
    return false;

  bool has_custom_scheme_prefix = false;
  bool allow_ext_plus_prefix =
      (security_level_ >=
       blink::ProtocolHandlerSecurityLevel::kExtensionFeatures);
  return blink::IsValidCustomHandlerScheme(protocol_, allow_ext_plus_prefix,
                                           has_custom_scheme_prefix);
}

bool ProtocolHandler::IsSameOrigin(
    const ProtocolHandler& handler) const {
  return handler.url().GetOrigin() == url_.GetOrigin();
}

const ProtocolHandler& ProtocolHandler::EmptyProtocolHandler() {
  static const ProtocolHandler* const kEmpty = new ProtocolHandler();
  return *kEmpty;
}

ProtocolHandler ProtocolHandler::CreateProtocolHandler(
    const base::DictionaryValue* value) {
  if (!IsValidDict(value)) {
    return EmptyProtocolHandler();
  }
  std::string protocol, url;
  // |time| defaults to the beginning of time if it is not specified.
  base::Time time;
  blink::ProtocolHandlerSecurityLevel security_level =
      blink::ProtocolHandlerSecurityLevel::kStrict;
  value->GetString("protocol", &protocol);
  value->GetString("url", &url);
  base::Optional<base::Time> time_value =
      util::ValueToTime(value->FindKey("last_modified"));
  // Treat invalid times as the default value.
  if (time_value)
    time = *time_value;
  base::Optional<int> security_level_value =
      value->FindIntPath("security_level");
  if (security_level_value) {
    security_level =
        blink::ProtocolHandlerSecurityLevelFrom(*security_level_value);
  }

  if (value->HasKey("app_id")) {
    std::string app_id;
    value->GetString("app_id", &app_id);
    return ProtocolHandler(protocol, GURL(url), app_id, time, security_level);
  }

  return ProtocolHandler(protocol, GURL(url), time, security_level);
}

GURL ProtocolHandler::TranslateUrl(const GURL& url) const {
  std::string translatedUrlSpec(url_.spec());
  base::ReplaceFirstSubstringAfterOffset(
      &translatedUrlSpec, 0, "%s",
      net::EscapeQueryParamValue(url.spec(), false));
  return GURL(translatedUrlSpec);
}

std::unique_ptr<base::DictionaryValue> ProtocolHandler::Encode() const {
  auto d = std::make_unique<base::DictionaryValue>();
  d->SetString("protocol", protocol_);
  d->SetString("url", url_.spec());
  d->SetKey("last_modified", util::TimeToValue(last_modified_));
  d->SetIntPath("security_level", static_cast<int>(security_level_));

  if (web_app_id_.has_value())
    d->SetString("app_id", web_app_id_.value());

  return d;
}

std::u16string ProtocolHandler::GetProtocolDisplayName(
    const std::string& protocol) {
  if (protocol == "mailto")
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_MAILTO_NAME);
  if (protocol == "webcal")
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_WEBCAL_NAME);
  return base::UTF8ToUTF16(protocol);
}

std::u16string ProtocolHandler::GetProtocolDisplayName() const {
  return GetProtocolDisplayName(protocol_);
}

#if !defined(NDEBUG)
std::string ProtocolHandler::ToString() const {
  return "{ protocol=" + protocol_ +
         ", url=" + url_.spec() +
         " }";
}
#endif

bool ProtocolHandler::operator==(const ProtocolHandler& other) const {
  return protocol_ == other.protocol_ && url_ == other.url_;
}

bool ProtocolHandler::IsEquivalent(const ProtocolHandler& other) const {
  return protocol_ == other.protocol_ && url_ == other.url_;
}

bool ProtocolHandler::operator<(const ProtocolHandler& other) const {
  return url_ < other.url_;
}
