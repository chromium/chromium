// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/sharing_hub/sharing_hub_service.h"
#include "chrome/browser/sharing_hub/sharing_hub_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button.h"

namespace sharing_hub {

namespace {

// The source from which the sharing hub was launched from.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with ShareSourceDesktop
// in src/tools/metrics/histograms/enums.xml.
enum class ShareSourceDesktop {
  kUnknown = 0,
  kOmniboxSharingHub = 1,
  kMaxValue = kOmniboxSharingHub,
};

const char kAnyShareStarted[] = "Sharing.AnyShareStartedDesktop";

void LogShareSourceDesktop(ShareSourceDesktop source) {
  UMA_HISTOGRAM_ENUMERATION(kAnyShareStarted, source);
}

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
  if (base::FeatureList::IsEnabled(features::kSharesheet) &&
      base::FeatureList::IsEnabled(features::kChromeOSSharingHub) &&
      sharesheet_service_ && native_window_) {
    sharesheet_service_->CloseBubble(native_window_,
                                     sharesheet::SharesheetResult::kCancel);
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
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ShowSharesheet(browser->window()->GetSharingHubIconButton());
#else
  sharing_hub_bubble_view_ =
      browser->window()->ShowSharingHubBubble(web_contents_, this, true);
  LogShareSourceDesktop(ShareSourceDesktop::kOmniboxSharingHub);
#endif
}

SharingHubBubbleView* SharingHubBubbleController::sharing_hub_bubble_view()
    const {
  return sharing_hub_bubble_view_;
}

std::u16string SharingHubBubbleController::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SHARING_HUB_TITLE);
}

Profile* SharingHubBubbleController::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

bool SharingHubBubbleController::ShouldOfferOmniboxIcon() {
  if (!web_contents_)
    return false;

  if (GetProfile()->IsIncognitoProfile() || GetProfile()->IsGuestSession())
    return false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(features::kSharesheet) &&
         base::FeatureList::IsEnabled(features::kChromeOSSharingHub);
#else
  return SharingHubOmniboxEnabled(web_contents_->GetBrowserContext());
#endif
}

std::vector<SharingHubAction>
SharingHubBubbleController::GetFirstPartyActions() {
  std::vector<SharingHubAction> actions;

  SharingHubModel* model = GetSharingHubModel();
  if (model)
    model->GetFirstPartyActionList(web_contents_, &actions);

  return actions;
}

std::vector<SharingHubAction>
SharingHubBubbleController::GetThirdPartyActions() {
  std::vector<SharingHubAction> actions;

  SharingHubModel* model = GetSharingHubModel();
  if (model)
    model->GetThirdPartyActionList(web_contents_, &actions);

  return actions;
}

void SharingHubBubbleController::OnActionSelected(
    int command_id,
    bool is_first_party,
    std::string feature_name_for_metrics) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  // Can be null in tests.
  if (!browser)
    return;

  if (is_first_party) {
    base::RecordComputedAction(feature_name_for_metrics);
    chrome::ExecuteCommand(browser, command_id);
  } else {
    SharingHubModel* model = GetSharingHubModel();
    DCHECK(model);
    model->ExecuteThirdPartyAction(web_contents_, command_id);
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
void SharingHubBubbleController::ShowSharesheet(
    views::Button* highlighted_button) {
  if (!base::FeatureList::IsEnabled(features::kSharesheet) ||
      !base::FeatureList::IsEnabled(features::kChromeOSSharingHub)) {
    return;
  }

  DCHECK(highlighted_button);
  highlighted_button_tracker_.SetView(highlighted_button);

  if (!sharesheet_service_) {
    Profile* const profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    DCHECK(profile);

    sharesheet_service_ =
        sharesheet::SharesheetServiceFactory::GetForProfile(profile);
  }

  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromText(
      web_contents_->GetURL().spec(),
      base::UTF16ToUTF8(web_contents_->GetTitle()));
  sharesheet_service_->ShowBubble(
      web_contents_, std::move(intent),
      sharesheet::SharesheetMetrics::LaunchSource::kOmniboxShare,
      base::BindOnce(&SharingHubBubbleController::OnShareDelivered,
                     base::Unretained(this)),
      base::BindOnce(&SharingHubBubbleController::OnSharesheetClosed,
                     base::Unretained(this)));

  // Save the window in order to close the sharesheet if the tab is closed.
  native_window_ = web_contents_->GetTopLevelNativeWindow();
}

void SharingHubBubbleController::OnShareDelivered(
    sharesheet::SharesheetResult result) {
  LogCrOSSharesheetResult(result);
}

void SharingHubBubbleController::OnSharesheetClosed(
    views::Widget::ClosedReason reason) {
  // Deselect the omnibox icon now that the sharesheet is closed.
  views::Button* button =
      views::Button::AsButton(highlighted_button_tracker_.view());
  if (button)
    button->SetHighlighted(false);

  native_window_ = nullptr;
}
#endif

SharingHubBubbleController::SharingHubBubbleController() = default;

SharingHubBubbleController::SharingHubBubbleController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharingHubBubbleController)

}  // namespace sharing_hub
