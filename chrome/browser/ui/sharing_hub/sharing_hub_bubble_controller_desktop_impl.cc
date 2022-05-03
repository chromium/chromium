// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller_desktop_impl.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/share/share_metrics.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/sharing_hub/sharing_hub_service.h"
#include "chrome/browser/sharing_hub/sharing_hub_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace sharing_hub {

// static
// SharingHubBubbleController:
SharingHubBubbleController*
SharingHubBubbleController::CreateOrGetFromWebContents(
    content::WebContents* web_contents) {
  SharingHubBubbleControllerDesktopImpl::CreateForWebContents(web_contents);
  SharingHubBubbleControllerDesktopImpl* controller =
      SharingHubBubbleControllerDesktopImpl::FromWebContents(web_contents);
  return controller;
}

SharingHubBubbleControllerDesktopImpl::
    ~SharingHubBubbleControllerDesktopImpl() {
  if (sharing_hub_bubble_view_) {
    sharing_hub_bubble_view_->Hide();
  }
}

void SharingHubBubbleControllerDesktopImpl::HideBubble() {
  if (sharing_hub_bubble_view_) {
    sharing_hub_bubble_view_->Hide();
    sharing_hub_bubble_view_ = nullptr;
  }
}

void SharingHubBubbleControllerDesktopImpl::ShowBubble() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

  sharing_hub_bubble_view_ =
      browser->window()->ShowSharingHubBubble(web_contents());

  share::LogShareSourceDesktop(share::ShareSourceDesktop::kOmniboxSharingHub);
}

SharingHubBubbleView*
SharingHubBubbleControllerDesktopImpl::sharing_hub_bubble_view() const {
  return sharing_hub_bubble_view_;
}

bool SharingHubBubbleControllerDesktopImpl::ShouldOfferOmniboxIcon() {
  return SharingHubOmniboxEnabled(GetWebContents().GetBrowserContext());
}

std::u16string SharingHubBubbleControllerDesktopImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SHARING_HUB_TITLE);
}

Profile* SharingHubBubbleControllerDesktopImpl::GetProfile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

std::vector<SharingHubAction>
SharingHubBubbleControllerDesktopImpl::GetFirstPartyActions() {
  std::vector<SharingHubAction> actions;

  SharingHubModel* model = GetSharingHubModel();
  if (model)
    model->GetFirstPartyActionList(&GetWebContents(), &actions);

  return actions;
}

std::vector<SharingHubAction>
SharingHubBubbleControllerDesktopImpl::GetThirdPartyActions() {
  std::vector<SharingHubAction> actions;

  SharingHubModel* model = GetSharingHubModel();
  if (model)
    model->GetThirdPartyActionList(&actions);

  return actions;
}

bool SharingHubBubbleControllerDesktopImpl::ShouldUsePreview() {
  return share::GetDesktopSharePreviewVariant() !=
         share::DesktopSharePreviewVariant::kDisabled;
}

std::u16string SharingHubBubbleControllerDesktopImpl::GetPreviewTitle() {
  // TODO(https://crbug.com/1312524): get passed this state from the omnibox
  // instead.
  return GetWebContents().GetTitle();
}

GURL SharingHubBubbleControllerDesktopImpl::GetPreviewUrl() {
  // TODO(https://crbug.com/1312524): get passed this state from the omnibox
  // instead.
  return GetWebContents().GetVisibleURL();
}

ui::ImageModel SharingHubBubbleControllerDesktopImpl::GetPreviewImage() {
  return ui::ImageModel::FromImage(favicon::GetDefaultFavicon());
}

base::CallbackListSubscription
SharingHubBubbleControllerDesktopImpl::RegisterPreviewImageChangedCallback(
    PreviewImageChangedCallback callback) {
  return preview_image_changed_callbacks_.Add(callback);
}

void SharingHubBubbleControllerDesktopImpl::OnActionSelected(
    int command_id,
    bool is_first_party,
    std::string feature_name_for_metrics) {
  Browser* browser = chrome::FindBrowserWithWebContents(&GetWebContents());
  // Can be null in tests.
  if (!browser)
    return;

  if (is_first_party) {
    base::RecordComputedAction(feature_name_for_metrics);

    // Show a back button for 1P dialogs anchored to the sharing hub icon.
    if (command_id == IDC_QRCODE_GENERATOR) {
      qrcode_generator::QRCodeGeneratorBubbleController* qrcode_controller =
          qrcode_generator::QRCodeGeneratorBubbleController::Get(
              &GetWebContents());
      qrcode_controller->ShowBubble(GetWebContents().GetLastCommittedURL(),
                                    true);
    } else if (command_id == IDC_SEND_TAB_TO_SELF) {
      send_tab_to_self::SendTabToSelfBubbleController*
          send_tab_to_self_controller =
              send_tab_to_self::SendTabToSelfBubbleController::
                  CreateOrGetFromWebContents(&GetWebContents());
      send_tab_to_self_controller->ShowBubble(true);
    } else {
      chrome::ExecuteCommand(browser, command_id);
    }
  } else {
    SharingHubModel* model = GetSharingHubModel();
    DCHECK(model);
    model->ExecuteThirdPartyAction(&GetWebContents(), command_id);
  }
}

void SharingHubBubbleControllerDesktopImpl::OnBubbleClosed() {
  sharing_hub_bubble_view_ = nullptr;
}

SharingHubModel* SharingHubBubbleControllerDesktopImpl::GetSharingHubModel() {
  if (!sharing_hub_model_) {
    SharingHubService* const service =
        SharingHubServiceFactory::GetForProfile(GetProfile());
    if (!service)
      return nullptr;
    sharing_hub_model_ = service->GetSharingHubModel();
  }
  return sharing_hub_model_;
}

SharingHubBubbleControllerDesktopImpl::SharingHubBubbleControllerDesktopImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SharingHubBubbleControllerDesktopImpl>(
          *web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharingHubBubbleControllerDesktopImpl);

}  // namespace sharing_hub
