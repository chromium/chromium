// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"

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

AsyncCheckTracker::~AsyncCheckTracker() = default;

base::WeakPtr<AsyncCheckTracker> AsyncCheckTracker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace safe_browsing
