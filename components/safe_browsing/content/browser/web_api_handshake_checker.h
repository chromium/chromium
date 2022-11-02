// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_API_HANDSHAKE_CHECKER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_API_HANDSHAKE_CHECKER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace safe_browsing {

class UrlCheckerDelegate;

// Performs SafeBrowsing checks for Web API handshakes such as WebTransport.
class WebApiHandshakeChecker {
 public:
  using GetDelegateCallback =
      base::OnceCallback<scoped_refptr<UrlCheckerDelegate>()>;
  using GetWebContentsCallback =
      base::RepeatingCallback<content::WebContents*()>;

  enum class CheckResult {
    kProceed,
    kBlocked,
  };
  using CheckCallback = base::OnceCallback<void(CheckResult)>;

  WebApiHandshakeChecker(GetDelegateCallback delegate_getter,
                         const GetWebContentsCallback& web_contents_getter,
                         int frame_tree_node_id);
  ~WebApiHandshakeChecker();

  WebApiHandshakeChecker(const WebApiHandshakeChecker&) = delete;
  WebApiHandshakeChecker& operator=(const WebApiHandshakeChecker&) = delete;
  WebApiHandshakeChecker(WebApiHandshakeChecker&&) = delete;
  WebApiHandshakeChecker& operator=(WebApiHandshakeChecker&&) = delete;

  void Check(const GURL& url, CheckCallback callback);

 private:
  // Performs checks on the IO thread by using SafeBrowsingUrlCheckerImpl, which
  // must live on the IO thread.
  class CheckerOnIO;

  void OnCompleteCheck(bool slow_check,
                       bool proceed,
                       bool showed_interstitial,
                       bool did_perform_real_time_check,
                       bool did_check_allowlist);

  std::unique_ptr<CheckerOnIO> io_checker_;
  CheckCallback check_callback_;

  base::WeakPtrFactory<WebApiHandshakeChecker> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_API_HANDSHAKE_CHECKER_H_
