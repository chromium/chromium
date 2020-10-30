// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/promo_browser_command/promo_browser_command_handler.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/webui_url_constants.h"
#include "components/safe_browsing/content/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

using promo_browser_command::mojom::ClickInfoPtr;
using promo_browser_command::mojom::Command;
using promo_browser_command::mojom::CommandHandler;

// static
const char PromoBrowserCommandHandler::kPromoBrowserCommandHistogramName[] =
    "NewTabPage.Promos.PromoBrowserCommand";

PromoBrowserCommandHandler::PromoBrowserCommandHandler(
    mojo::PendingReceiver<CommandHandler> pending_page_handler,
    Profile* profile)
    : profile_(profile),
      command_updater_(std::make_unique<CommandUpdaterImpl>(this)),
      page_handler_(this, std::move(pending_page_handler)) {
  if (!base::FeatureList::IsEnabled(features::kPromoBrowserCommands))
    return;
  EnableCommands();
}

PromoBrowserCommandHandler::~PromoBrowserCommandHandler() = default;

void PromoBrowserCommandHandler::CanShowPromoWithCommand(
    promo_browser_command::mojom::Command command_id,
    CanShowPromoWithCommandCallback callback) {
  bool can_show = false;
  switch (static_cast<Command>(command_id)) {
    case Command::kUnknownCommand:
      // Nothing to do.
      break;
    case Command::kOpenSafetyCheck:
      can_show = true;
      break;
    case Command::kOpenSafeBrowsingEnhancedProtectionSettings: {
      bool managed = safe_browsing::SafeBrowsingPolicyHandler::
          IsSafeBrowsingProtectionLevelSetByPolicy(profile_->GetPrefs());
      bool already_enabled =
          safe_browsing::IsEnhancedProtectionEnabled(*(profile_->GetPrefs()));
      can_show = !managed && !already_enabled;
    } break;
    default:
      NOTREACHED() << "Unspecified behavior for command " << command_id;
      break;
  }
  std::move(callback).Run(can_show);
}

void PromoBrowserCommandHandler::ExecuteCommand(
    Command command_id,
    ClickInfoPtr click_info,
    ExecuteCommandCallback callback) {
  const auto disposition = ui::DispositionFromClick(
      click_info->middle_button, click_info->alt_key, click_info->ctrl_key,
      click_info->meta_key, click_info->shift_key);
  const bool command_executed =
      GetCommandUpdater()->ExecuteCommandWithDisposition(
          static_cast<int>(command_id), disposition);
  std::move(callback).Run(command_executed);
}

void PromoBrowserCommandHandler::ExecuteCommandWithDisposition(
    int id,
    WindowOpenDisposition disposition) {
  const auto command = static_cast<Command>(id);
  base::UmaHistogramEnumeration(kPromoBrowserCommandHistogramName, command);

  switch (command) {
    case Command::kUnknownCommand:
      // Nothing to do.
      break;
    case Command::kOpenSafetyCheck:
      NavigateToURL(GURL(chrome::GetSettingsUrl(chrome::kSafetyCheckSubPage)),
                    disposition);
      base::RecordAction(
          base::UserMetricsAction("NewTabPage_Promos_SafetyCheck"));
      break;
    case Command::kOpenSafeBrowsingEnhancedProtectionSettings:
      NavigateToURL(GURL(chrome::GetSettingsUrl(
                        chrome::kSafeBrowsingEnhancedProtectionSubPage)),
                    disposition);
      base::RecordAction(
          base::UserMetricsAction("NewTabPage_Promos_EnhancedProtection"));
      break;
    default:
      NOTREACHED() << "Unspecified behavior for command " << id;
      break;
  }
}

void PromoBrowserCommandHandler::EnableCommands() {
  // Explicitly enable supported commands.
  GetCommandUpdater()->UpdateCommandEnabled(
      static_cast<int>(Command::kUnknownCommand), true);
  GetCommandUpdater()->UpdateCommandEnabled(
      static_cast<int>(Command::kOpenSafetyCheck), true);
  GetCommandUpdater()->UpdateCommandEnabled(
      static_cast<int>(Command::kOpenSafeBrowsingEnhancedProtectionSettings),
      true);
}

CommandUpdater* PromoBrowserCommandHandler::GetCommandUpdater() {
  return command_updater_.get();
}

void PromoBrowserCommandHandler::NavigateToURL(
    const GURL& url,
    WindowOpenDisposition disposition) {
  NavigateParams params(profile_, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = disposition;
  Navigate(&params);
}
