// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/async_check_tracker.h"

#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

WEB_CONTENTS_USER_DATA_KEY_IMPL(AsyncCheckTracker);

// static
AsyncCheckTracker* AsyncCheckTracker::GetOrCreateForWebContents(
    content::WebContents* web_contents,
    scoped_refptr<BaseUIManager> ui_manager) {
  CHECK(web_contents);
  // CreateForWebContents does nothing if the delegate instance already exists.
  AsyncCheckTracker::CreateForWebContents(web_contents, std::move(ui_manager));
  return AsyncCheckTracker::FromWebContents(web_contents);
}

AsyncCheckTracker::AsyncCheckTracker(content::WebContents* web_contents,
                                     scoped_refptr<BaseUIManager> ui_manager)
    : content::WebContentsUserData<AsyncCheckTracker>(*web_contents),
      ui_manager_(std::move(ui_manager)) {}

AsyncCheckTracker::~AsyncCheckTracker() {
  if (!base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                   std::move(pending_checker_));
  }
}

void AsyncCheckTracker::TransferUrlChecker(
    std::unique_ptr<UrlCheckerOnSB> checker) {
  // TODO(crbug.com/1501194): Replace callbacks in checker.
  pending_checker_ = std::move(checker);
}

bool AsyncCheckTracker::HasPendingCheckerForTesting() {
  return !!pending_checker_;
}

base::WeakPtr<AsyncCheckTracker> AsyncCheckTracker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace safe_browsing
