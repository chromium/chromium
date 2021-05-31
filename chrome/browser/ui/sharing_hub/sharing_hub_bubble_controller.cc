// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ShowSharesheet();
#else
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  sharing_hub_bubble_view_ =
      browser->window()->ShowSharingHubBubble(web_contents_, this, true);
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

  // TODO(1186845): Check enterprise policy

  return true;
}

std::vector<SharingHubAction> SharingHubBubbleController::GetActions() const {
  SharingHubService* const service =
      SharingHubServiceFactory::GetForProfile(GetProfile());
  SharingHubModel* const model =
      service ? service->GetSharingHubModel() : nullptr;

  std::vector<SharingHubAction> actions;
  if (model)
    model->GetActionList(web_contents_, &actions);

  return actions;
}

void SharingHubBubbleController::OnActionSelected(int command_id,
                                                  bool is_first_party) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  // Can be null in tests.
  if (!browser)
    return;

  if (is_first_party) {
    chrome::ExecuteCommand(browser, command_id);
  } else {
    // TODO(1186833): execute 3p action
  }
}

void SharingHubBubbleController::OnBubbleClosed() {
  sharing_hub_bubble_view_ = nullptr;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void SharingHubBubbleController::ShowSharesheet() {
  if (!base::FeatureList::IsEnabled(features::kSharesheet) ||
      !base::FeatureList::IsEnabled(features::kChromeOSSharingHub)) {
    return;
  }

  Profile* const profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  DCHECK(profile);

  sharesheet::SharesheetService* const sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile);

  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromText(
      web_contents_->GetURL().spec(),
      base::UTF16ToUTF8(web_contents_->GetTitle()));
  sharesheet_service->ShowBubble(
      web_contents_, std::move(intent),
      sharesheet::SharesheetMetrics::LaunchSource::kOmniboxShare,
      base::BindOnce(&SharingHubBubbleController::OnSharesheetShown,
                     base::Unretained(this)));
}

void SharingHubBubbleController::OnSharesheetShown(
    sharesheet::SharesheetResult result) {
  LogCrOSSharesheetResult(result);
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
