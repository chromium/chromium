// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/accessibility_annotator/accessibility_annotator_info_page_handler.h"

#include <utility>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

namespace accessibility_annotator::info {

AccessibilityAnnotatorInfoPageHandler::AccessibilityAnnotatorInfoPageHandler(
    mojo::PendingReceiver<accessibility_annotator::info::mojom::PageHandler>
        receiver,
    base::OnceCallback<void(InfoDialogResult)> callback,
    content::BrowserContext* browser_context)
    : receiver_(this, std::move(receiver)),
      callback_(std::move(callback)),
      browser_context_(browser_context) {}

AccessibilityAnnotatorInfoPageHandler::
    ~AccessibilityAnnotatorInfoPageHandler() {
  if (callback_) {
    std::move(callback_).Run(InfoDialogResult::kDismissed);
  }
}

void AccessibilityAnnotatorInfoPageHandler::GetAccountInfo(
    GetAccountInfoCallback callback) {
  // TODO(b/488266696): Retrieve account info from the profile.

  auto account_info = accessibility_annotator::info::mojom::AccountInfo::New();
  account_info->email = "user@example.com";
  account_info->avatar_url = "https://example.com/avatar.png";

  std::move(callback).Run(std::move(account_info));
}

void AccessibilityAnnotatorInfoPageHandler::OnInfoAcknowledged() {
  if (callback_) {
    std::move(callback_).Run(InfoDialogResult::kAcknowledged);
  }
}

void AccessibilityAnnotatorInfoPageHandler::OnInfoDismissed() {
  if (callback_) {
    std::move(callback_).Run(InfoDialogResult::kDismissed);
  }
}

void AccessibilityAnnotatorInfoPageHandler::OnManageSettingsClicked() {
  // TODO(b/488320942): Open settings page and record metric.
}

void AccessibilityAnnotatorInfoPageHandler::OnLearnMoreClicked() {
  // TODO(b/488320942): Open learn more page and record metric.
}

}  // namespace accessibility_annotator::info
