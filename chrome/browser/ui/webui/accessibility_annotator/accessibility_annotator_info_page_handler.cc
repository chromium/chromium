// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/accessibility_annotator/accessibility_annotator_info_page_handler.h"

#include <utility>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/accessibility_annotator/core/url_constants.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition.h"

namespace accessibility_annotator::info {

AccessibilityAnnotatorInfoPageHandler::AccessibilityAnnotatorInfoPageHandler(
    mojo::PendingReceiver<accessibility_annotator::info::mojom::PageHandler>
        receiver,
    base::OnceCallback<void(InfoDialogResult)> callback,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      callback_(std::move(callback)),
      web_contents_(web_contents) {}

AccessibilityAnnotatorInfoPageHandler::
    ~AccessibilityAnnotatorInfoPageHandler() {
  if (callback_) {
    std::move(callback_).Run(InfoDialogResult::kDismissed);
  }
}

void AccessibilityAnnotatorInfoPageHandler::GetAccountInfo(
    GetAccountInfoCallback callback) {
  auto account_info_mojom =
      accessibility_annotator::info::mojom::AccountInfo::New();

  if (web_contents_) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

    if (identity_manager) {
      CoreAccountInfo core_account_info =
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin);

      if (!core_account_info.IsEmpty()) {
        AccountInfo account_info =
            identity_manager->FindExtendedAccountInfoByAccountId(
                core_account_info.account_id);
        account_info_mojom->email = std::string(account_info.GetEmail());
        if (account_info.GetAvatarImage().has_value()) {
          account_info_mojom->avatar_url = webui::GetBitmapDataUrl(
              account_info.GetAvatarImage()->AsBitmap());
        } else {
          account_info_mojom->avatar_url =
              profiles::GetPlaceholderAvatarIconUrl();
        }
      }
    }
  }

  std::move(callback).Run(std::move(account_info_mojom));
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
  base::RecordAction(base::UserMetricsAction(
      "AccessibilityAnnotator.RemoteAnnotatorInfo.SettingsLinkClick"));

  if (auto* browser_window_interface =
          webui::GetBrowserWindowInterface(web_contents_)) {
    browser_window_interface->OpenURL(
        content::OpenURLParams(
            GURL(accessibility_annotator::kAccessibilityAnnotatorSettingsURL),
            content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
            ui::PAGE_TRANSITION_LINK,
            /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/base::DoNothing());
  }
}

void AccessibilityAnnotatorInfoPageHandler::OnLearnMoreClicked() {
  base::RecordAction(base::UserMetricsAction(
      "AccessibilityAnnotator.RemoteAnnotatorInfo.LearnMoreLinkClick"));

  if (auto* browser_window_interface =
          webui::GetBrowserWindowInterface(web_contents_)) {
    browser_window_interface->OpenURL(
        content::OpenURLParams(
            GURL(accessibility_annotator::kAccessibilityAnnotatorLearnMoreURL),
            content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
            ui::PAGE_TRANSITION_LINK,
            /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/base::DoNothing());
  }
}

}  // namespace accessibility_annotator::info
