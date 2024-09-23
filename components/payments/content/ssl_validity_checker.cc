// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/ssl_validity_checker.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/url_util.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace payments {

// static std::string
std::string SslValidityChecker::GetInvalidSslCertificateErrorMessage(
    content::WebContents* web_contents) {
  if (!web_contents)
    return errors::kInvalidSslCertificate;

  security_state::SecurityLevel security_level = GetSecurityLevel(web_contents);
  std::string level;
  switch (security_level) {
    // Indicate valid SSL with an empty string.
    case security_state::SECURE:
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      return "";

    case security_state::NONE:
      level = "NONE";
      break;
    case security_state::WARNING:
      level = "WARNING";
      break;
    case security_state::DANGEROUS:
      level = "DANGEROUS";
      break;

    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED_IN_MIGRATION();
      return errors::kInvalidSslCertificate;
  }

  std::string message;
  bool replaced =
      base::ReplaceChars(errors::kDetailedInvalidSslCertificateMessageFormat,
                         "$", level, &message);
  DCHECK(replaced);

  // No early return, so the other code is exercised in tests, too.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kIgnoreCertificateErrors)
             ? ""
             : message;
}

// static
bool SslValidityChecker::IsValidPageInPaymentHandlerWindow(
    content::WebContents* web_contents) {
  if (!web_contents)
    return false;

  GURL main_frame_url = web_contents->GetVisibleURL();
  if (!UrlUtil::IsValidUrlInPaymentHandlerWindow(main_frame_url))
    return false;

  if (main_frame_url.SchemeIsCryptographic()) {
    security_state::SecurityLevel security_level =
        GetSecurityLevel(web_contents);
    return security_level == security_state::SECURE ||
           security_level ==
               security_state::SECURE_WITH_POLICY_INSTALLED_CERT ||
           // No early return, so the other code is exercised in tests, too.
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kIgnoreCertificateErrors);
  }

  return true;
}

// static
security_state::SecurityLevel SslValidityChecker::GetSecurityLevel(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  std::unique_ptr<security_state::VisibleSecurityState> state =
      security_state::GetVisibleSecurityState(web_contents);
  DCHECK(state);

  return security_state::GetSecurityLevel(
      *state, /*used_policy_installed_certificate=*/false);
}

}  // namespace payments
