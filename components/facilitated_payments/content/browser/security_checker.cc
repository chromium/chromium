// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/security_checker.h"

#include "base/check.h"
#include "base/command_line.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "url/gurl.h"

namespace payments::facilitated {

SecurityChecker::SecurityChecker() = default;
SecurityChecker::~SecurityChecker() = default;

bool SecurityChecker::IsSecureForPaymentLinkHandling(
    content::RenderFrameHost& rfh) {
  const GURL& last_committed_url = rfh.GetLastCommittedURL();
  if (!network::IsUrlPotentiallyTrustworthy(last_committed_url)) {
    return false;
  }

  if (!last_committed_url.SchemeIsCryptographic()) {
    return false;
  }

  if (!IsPaymentPermissionsPolicyEnabled(rfh)) {
    return false;
  }

  if (!IsSslValid(*content::WebContents::FromRenderFrameHost(&rfh))) {
    return false;
  }

  return true;
}

bool SecurityChecker::IsSslValid(content::WebContents& web_contents) {
  security_state::SecurityLevel security_level = GetSecurityLevel(web_contents);
  // Indicate a HTTPS scheme with valid cert.
  if (security_level == security_state::SECURE) {
    return true;
  }

  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kIgnoreCertificateErrors);
}

security_state::SecurityLevel SecurityChecker::GetSecurityLevel(
    content::WebContents& web_contents) {
  std::unique_ptr<security_state::VisibleSecurityState> state =
      security_state::GetVisibleSecurityState(&web_contents);

  CHECK(state) << "security_state::VisibleSecurityState was a nullpt.";

  return security_state::GetSecurityLevel(
      *state, /*used_policy_installed_certificate=*/false);
}

bool SecurityChecker::IsPaymentPermissionsPolicyEnabled(
    content::RenderFrameHost& rfh) {
  // The payment PermissionsPolicy is enabled by default on the top-level
  // context, unless explicitly disabled by websites.
  return rfh.IsFeatureEnabled(blink::mojom::PermissionsPolicyFeature::kPayment);
}

}  // namespace payments::facilitated
