// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"

#include "base/bind.h"
#include "components/safe_browsing/content/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "components/safe_browsing/core/web_ui/constants.h"

namespace safe_browsing {

void SafeBrowsingUrlCheckerImpl::LogRTLookupRequest(
    const RTLookupRequest& request,
    const std::string& oauth_token) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  // The following is to log this RTLookupRequest on any open
  // chrome://safe-browsing pages.
  GetTaskRunner(ThreadID::UI)
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&WebUIInfoSingleton::AddToRTLookupPings,
                         base::Unretained(WebUIInfoSingleton::GetInstance()),
                         request, oauth_token),
          base::BindOnce(&SafeBrowsingUrlCheckerImpl::SetWebUIToken,
                         weak_factory_.GetWeakPtr()));
}

void SafeBrowsingUrlCheckerImpl::LogRTLookupResponse(
    const RTLookupResponse& response) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  if (url_web_ui_token_ != -1) {
    // The following is to log this RTLookupResponse on any open
    // chrome://safe-browsing pages.
    GetTaskRunner(ThreadID::UI)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&WebUIInfoSingleton::AddToRTLookupResponses,
                           base::Unretained(WebUIInfoSingleton::GetInstance()),
                           url_web_ui_token_, response));
  }
}

}  // namespace safe_browsing
