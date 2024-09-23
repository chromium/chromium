// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/custom_handlers/protocol_handler.h"

#include <string_view>

#include "base/json/values_util.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace custom_handlers {

namespace features {

// https://html.spec.whatwg.org/multipage/system-state.html#security-and-privacy
BASE_FEATURE(kStripCredentialsForExternalProtocolHandler,
             "StripCredentialsForExternalProtocolHandler",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ProtocolHandler(protocol, url, app_id, base::Time::Now(),
                         blink::ProtocolHandlerSecurityLevel::kStrict);
}

ProtocolHandler::ProtocolHandler() = default;

bool ProtocolHandler::IsValidDict(const base::Value::Dict& value) {
  // Note that "title" parameter is ignored.
  // The |last_modified| field is optional as it was introduced in M68.
  return value.FindString("protocol") && value.FindString("url");
}

bool ProtocolHandler::IsValid() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We don't want to include URL's syntax checks because there are use cases of
  // the protocol handlers logic that require more flexibility than the one
  // specified for the registerProtocolHandler API (eg, Predefined Handlers).
  if (!blink::IsAllowedCustomHandlerURL(url_, security_level_))
    return false;

  return blink::IsValidCustomHandlerScheme(protocol_, security_level_);
}

bool ProtocolHandler::IsSameOrigin(const ProtocolHandler& handler) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return handler.url().DeprecatedGetOriginAsURL() ==
         url_.DeprecatedGetOriginAsURL();
}

const ProtocolHandler& ProtocolHandler::EmptyProtocolHandler() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static const ProtocolHandler* const kEmpty = new ProtocolHandler();
  return *kEmpty;
}

ProtocolHandler ProtocolHandler::CreateProtocolHandler(
    const base::Value::Dict& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsValidDict(value)) {
    return EmptyProtocolHandler();
  }
  std::string protocol, url;
  // |time| defaults to the beginning of time if it is not specified.
  base::Time time;
  blink::ProtocolHandlerSecurityLevel security_level =
      blink::ProtocolHandlerSecurityLevel::kStrict;
  if (const std::string* protocol_in = value.FindString("protocol"))
    protocol = *protocol_in;
  if (const std::string* url_in = value.FindString("url"))
    url = *url_in;
  std::optional<base::Time> time_value =
      base::ValueToTime(value.Find("last_modified"));
  // Treat invalid times as the default value.
  if (time_value)
    time = *time_value;
  std::optional<int> security_level_value = value.FindInt("security_level");
  if (security_level_value) {
    security_level =
        blink::ProtocolHandlerSecurityLevelFrom(*security_level_value);
  }

  if (const base::Value* app_id_val = value.Find("app_id")) {
    std::string app_id;
    if (app_id_val->is_string())
      app_id = app_id_val->GetString();
    return ProtocolHandler(protocol, GURL(url), app_id, time, security_level);
  }

  return ProtocolHandler(protocol, GURL(url), time, security_level);
}

GURL ProtocolHandler::TranslateUrl(const GURL& url) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string clean_url;
  std::string_view url_spec(url.spec());

  // Remove credentials from the url if present, in order to mitigate the risk
  // of credential leakage
  if ((url.has_username() || url.has_password()) &&
      base::FeatureList::IsEnabled(
          features::kStripCredentialsForExternalProtocolHandler)) {
    GURL::Replacements replacements;
    replacements.ClearUsername();
    replacements.ClearPassword();
    clean_url = url.ReplaceComponents(replacements).spec();
    url_spec = clean_url;
  }

  std::string translatedUrlSpec(url_.spec());
  base::ReplaceFirstSubstringAfterOffset(
      &translatedUrlSpec, 0, "%s",
      base::EscapeQueryParamValue(url_spec, false));
  return GURL(translatedUrlSpec);
}

base::Value::Dict ProtocolHandler::Encode() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Value::Dict d;
  d.Set("protocol", protocol_);
  d.Set("url", url_.spec());
  d.Set("last_modified", base::TimeToValue(last_modified_));
  d.Set("security_level", static_cast<int>(security_level_));

  if (web_app_id_.has_value())
    d.Set("app_id", web_app_id_.value());

  return d;
}

std::u16string ProtocolHandler::GetProtocolDisplayName(
    const std::string& protocol) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (protocol == "mailto")
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_MAILTO_NAME);
  if (protocol == "webcal")
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_WEBCAL_NAME);
  return base::UTF8ToUTF16(protocol);
}

std::u16string ProtocolHandler::GetProtocolDisplayName() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetProtocolDisplayName(protocol_);
}

#if !defined(NDEBUG)
std::string ProtocolHandler::ToString() const {
  return "{ protocol=" + protocol_ + ", url=" + url_.spec() + " }";
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

}  // namespace custom_handlers
