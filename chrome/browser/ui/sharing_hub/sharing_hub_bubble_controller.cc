// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
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
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace sharing_hub {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Result of the CrOS sharesheet, i.e. whether the user selects a share target
// after opening the sharesheet.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// SharingHubSharesheetResult in src/tools/metrics/histograms/enums.xml.
enum class SharingHubSharesheetResult {
  SUCCESS = 0,
  CANCELED = 1,
  kMaxValue = CANCELED,
};

const char kSharesheetResult[] =
    "Sharing.SharingHubDesktop.CrOSSharesheetResult";

SharingHubSharesheetResult GetSharesheetResultHistogram(
    sharesheet::SharesheetResult result) {
  switch (result) {
    case sharesheet::SharesheetResult::kSuccess:
      return SharingHubSharesheetResult::SUCCESS;
    case sharesheet::SharesheetResult::kCancel:
    case sharesheet::SharesheetResult::kErrorAlreadyOpen:
    case sharesheet::SharesheetResult::kErrorWindowClosed:
      return SharingHubSharesheetResult::CANCELED;
  }
}

void LogCrOSSharesheetResult(sharesheet::SharesheetResult result) {
  UMA_HISTOGRAM_ENUMERATION(kSharesheetResult,
                            GetSharesheetResultHistogram(result));
}
#endif

}  // namespace

SharingHubBubbleController::~SharingHubBubbleController() {
  if (sharing_hub_bubble_view_) {
    sharing_hub_bubble_view_->Hide();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (bubble_showing_) {
    bubble_showing_ = false;
    // Close any remnant Sharesheet dialog.
    if (web_contents_containing_window_) {
      CloseSharesheet();
    }

    // We must deselect the icon manually since the Sharesheet will not be able
    // to invoke OnSharesheetClosed() at this point.
    DeselectIcon();
  }
#endif
}

// static
SharingHubBubbleController*
SharingHubBubbleController::CreateOrGetFromWebContents(
    content::WebContents* web_contents) {
  SharingHubBubbleController::CreateForWebContents(web_contents);
  SharingHubBubbleController* controller =
      SharingHubBubbleController::FromWebContents(web_contents);
  return controller;
}

void SharingHubBubbleController::HideBubble() {
  if (sharing_hub_bubble_view_) {
    sharing_hub_bubble_view_->Hide();
    sharing_hub_bubble_view_ = nullptr;
  }
}

void SharingHubBubbleController::ShowBubble() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Ignore subsequent calls to open the Sharesheet if it already is open. This
  // is especially for the Nearby Share dialog, where clicking outside of it
  // will not dismiss the dialog.
  if (bubble_showing_)
    return;
  bubble_showing_ = true;
  ShowSharesheet(browser->window()->GetSharingHubIconButton());
#else
  sharing_hub_bubble_view_ =
      browser->window()->ShowSharingHubBubble(web_contents(), this, true);
#endif

  share::LogShareSourceDesktop(share::ShareSourceDesktop::kOmniboxSharingHub);
}

SharingHubBubbleView* SharingHubBubbleController::sharing_hub_bubble_view()
    const {
  return sharing_hub_bubble_view_;
}

std::u16string SharingHubBubbleController::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SHARING_HUB_TITLE);
}

Profile* SharingHubBubbleController::GetProfile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

bool SharingHubBubbleController::ShouldOfferOmniboxIcon() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return !GetProfile()->IsIncognitoProfile() && !GetProfile()->IsGuestSession();
#else
  return SharingHubOmniboxEnabled(GetWebContents().GetBrowserContext());
#endif
}

std::vector<SharingHubAction>
SharingHubBubbleController::GetFirstPartyActions() {
  std::vector<SharingHubAction> actions;

  SharingHubModel* model = GetSharingHubModel();
  if (model)
    model->GetFirstPartyActionList(&GetWebContents(), &actions);

  return actions;
}

std::vector<SharingHubAction>
SharingHubBubbleController::GetThirdPartyActions() {
  std::vector<SharingHubAction> actions;

  SharingHubModel* model = GetSharingHubModel();
  if (model)
    model->GetThirdPartyActionList(&actions);

  return actions;
}

bool SharingHubBubbleController::ShouldUsePreview() {
  return share::GetDesktopSharePreviewVariant() !=
         share::DesktopSharePreviewVariant::kDisabled;
}

std::u16string SharingHubBubbleController::GetPreviewTitle() {
  // TODO(https://crbug.com/1312524): get passed this state from the omnibox
  // instead.
  return GetWebContents().GetTitle();
}

GURL SharingHubBubbleController::GetPreviewUrl() {
  // TODO(https://crbug.com/1312524): get passed this state from the omnibox
  // instead.
  return GetWebContents().GetVisibleURL();
}

ui::ImageModel SharingHubBubbleController::GetPreviewImage() {
  return ui::ImageModel::FromImage(favicon::GetDefaultFavicon());
}

base::CallbackListSubscription
SharingHubBubbleController::RegisterPreviewImageChangedCallback(
    PreviewImageChangedCallback callback) {
  return preview_image_changed_callbacks_.Add(callback);
}

void SharingHubBubbleController::OnActionSelected(
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

void SharingHubBubbleController::OnBubbleClosed() {
  sharing_hub_bubble_view_ = nullptr;
}

SharingHubModel* SharingHubBubbleController::GetSharingHubModel() {
  if (!sharing_hub_model_) {
    SharingHubService* const service =
        SharingHubServiceFactory::GetForProfile(GetProfile());
    if (!service)
      return nullptr;
    sharing_hub_model_ = service->GetSharingHubModel();
  }
  return sharing_hub_model_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
sharesheet::SharesheetService*
SharingHubBubbleController::GetSharesheetService() {
  if (!sharesheet_service_) {
    Profile* const profile =
        Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
    DCHECK(profile);

    sharesheet_service_ =
        sharesheet::SharesheetServiceFactory::GetForProfile(profile);
  }

  return sharesheet_service_;
}

void SharingHubBubbleController::ShowSharesheet(
    views::Button* highlighted_button) {
  DCHECK(highlighted_button);
  highlighted_button_tracker_.SetView(highlighted_button);

  sharesheet::SharesheetService* sharesheet_service = GetSharesheetService();
  if (!sharesheet_service)
    return;

  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromText(
      GetWebContents().GetLastCommittedURL().spec(),
      base::UTF16ToUTF8(GetWebContents().GetTitle()));
  sharesheet_service->ShowBubble(
      &GetWebContents(), std::move(intent),
      sharesheet::LaunchSource::kOmniboxShare,
      base::BindOnce(&SharingHubBubbleController::OnShareDelivered,
                     AsWeakPtr()),
      base::BindOnce(&SharingHubBubbleController::OnSharesheetClosed,
                     AsWeakPtr()));

  // Save the window in order to close the sharesheet if the tab is closed. This
  // will return the incorrect window if called later.
  web_contents_containing_window_ = GetWebContents().GetTopLevelNativeWindow();
}

void SharingHubBubbleController::CloseSharesheet() {
  sharesheet::SharesheetService* sharesheet_service = GetSharesheetService();
  if (!sharesheet_service)
    return;

  sharesheet::SharesheetController* sharesheet_controller =
      sharesheet_service->GetSharesheetController(
          web_contents_containing_window_);
  if (!sharesheet_controller)
    return;

  sharesheet_controller->CloseBubble(sharesheet::SharesheetResult::kCancel);

  // OnSharesheetClosed() is not guaranteed to be called by the
  // SharesheetController (specifically for the case where this is invoked by
  // our destructor). Hence, we must explicitly set this null here.
  web_contents_containing_window_ = nullptr;
}

void SharingHubBubbleController::OnShareDelivered(
    sharesheet::SharesheetResult result) {
  LogCrOSSharesheetResult(result);
}

void SharingHubBubbleController::OnSharesheetClosed(
    views::Widget::ClosedReason reason) {
  bubble_showing_ = false;
  web_contents_containing_window_ = nullptr;
  // Deselect the omnibox icon now that the sharesheet is closed.
  DeselectIcon();
}

void SharingHubBubbleController::DeselectIcon() {
  if (!highlighted_button_tracker_.view())
    return;

  views::Button* button =
      views::Button::AsButton(highlighted_button_tracker_.view());
  if (button)
    button->SetHighlighted(false);
}

void SharingHubBubbleController::OnVisibilityChanged(
    content::Visibility visibility) {
  // Cancel the current share attempt if the user switches to a different tab in
  // the window. Switching windows is permitted since a sharesheet is tied to
  // the native window.
  if (bubble_showing_ && visibility == content::Visibility::HIDDEN) {
    CloseSharesheet();
  }
}
#endif

SharingHubBubbleController::SharingHubBubbleController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SharingHubBubbleController>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharingHubBubbleController);

}  // namespace sharing_hub
