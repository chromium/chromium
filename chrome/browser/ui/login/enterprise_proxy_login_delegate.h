// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_ENTERPRISE_PROXY_LOGIN_DELEGATE_H_
#define CHROME_BROWSER_UI_LOGIN_ENTERPRISE_PROXY_LOGIN_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// Platform-specific implementation of EnterpriseProxyErrorService::Delegate
// for handling login and re-authentication prompts triggered by enterprise
// proxy auth challenges.
class EnterpriseProxyLoginDelegate
    : public enterprise_net::EnterpriseProxyErrorService::Delegate {
 public:
  EnterpriseProxyLoginDelegate(base::WeakPtr<content::WebContents> web_contents,
                               bool is_primary_main_frame);
  EnterpriseProxyLoginDelegate(const EnterpriseProxyLoginDelegate&) = delete;
  EnterpriseProxyLoginDelegate& operator=(const EnterpriseProxyLoginDelegate&) =
      delete;
  ~EnterpriseProxyLoginDelegate() override;

  // EnterpriseProxyErrorService::Delegate:
  void OnSignInRequired(const GURL& destination_url) override;

 private:
  // EnterpriseProxyLoginDelegate is created during proxy auth interception and
  // held across asynchronous credential/token checks. A WeakPtr is used
  // because the WebContents (and its owning tab) may be closed before the
  // async auth challenge handling completes.
  base::WeakPtr<content::WebContents> web_contents_;
  bool is_primary_main_frame_ = false;
};

#endif  // CHROME_BROWSER_UI_LOGIN_ENTERPRISE_PROXY_LOGIN_DELEGATE_H_
