// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/privacy_review_handler.h"

#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"

namespace settings {

namespace {

bool IsPrivacyReviewAvailable(Profile* profile) {
  return base::FeatureList::IsEnabled(features::kPrivacyReview) &&
         !chrome::enterprise_util::IsBrowserManaged(profile) &&
         !profile->IsChild();
}

}  // namespace

void PrivacyReviewHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "isPrivacyReviewAvailable",
      base::BindRepeating(&PrivacyReviewHandler::HandleIsPrivacyReviewAvailable,
                          base::Unretained(this)));
}

void PrivacyReviewHandler::HandleIsPrivacyReviewAvailable(
    base::Value::ConstListView args) {
  // Check that there is only one argument which is the callback id.
  DCHECK_EQ(args.size(), 1u);

  AllowJavascript();

  ResolveJavascriptCallback(
      args[0],
      base::Value(IsPrivacyReviewAvailable(Profile::FromWebUI(web_ui()))));
}

}  // namespace settings
