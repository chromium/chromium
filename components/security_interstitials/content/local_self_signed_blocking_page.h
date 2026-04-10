// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_LOCAL_SELF_SIGNED_BLOCKING_PAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_LOCAL_SELF_SIGNED_BLOCKING_PAGE_H_

#include "components/security_interstitials/content/ssl_blocking_page.h"

class LocalSelfSignedBlockingPage : public SSLBlockingPage {
 public:
  LocalSelfSignedBlockingPage(
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
          controller_client);

  LocalSelfSignedBlockingPage(const LocalSelfSignedBlockingPage&) = delete;
  LocalSelfSignedBlockingPage& operator=(const LocalSelfSignedBlockingPage&) =
      delete;

  ~LocalSelfSignedBlockingPage() override;

 protected:
  // SSLBlockingPageBase implementation:
  void PopulateInterstitialStrings(base::DictValue& load_time_data) override;
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_LOCAL_SELF_SIGNED_BLOCKING_PAGE_H_
