// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/local_self_signed_blocking_page.h"

#include "base/values.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"

LocalSelfSignedBlockingPage::LocalSelfSignedBlockingPage(
    content::WebContents* web_contents,
    net::Error cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    const GURL& support_url,
    bool overrideable,
    bool can_show_enhanced_protection_message,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : SSLBlockingPage(web_contents,
                      cert_error,
                      ssl_info,
                      request_url,
                      options_mask,
                      time_triggered,
                      support_url,
                      overrideable,
                      can_show_enhanced_protection_message,
                      std::move(controller_client)) {}

LocalSelfSignedBlockingPage::~LocalSelfSignedBlockingPage() = default;

void LocalSelfSignedBlockingPage::PopulateInterstitialStrings(
    base::DictValue& load_time_data) {
  // TODO(crbug.com/394119724): For now this is just the SSL interstitial with a
  // different title and heading. This will eventually be replaced with a
  // different UI.
  SSLBlockingPage::PopulateInterstitialStrings(load_time_data);
  load_time_data.Set("tabTitle", "Local Self-Signed Site");
  load_time_data.Set("heading", "Local Self-Signed Site");
}
