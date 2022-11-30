// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/intro_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/webui/intro/intro_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/intro_resources.h"
#include "chrome/grit/intro_resources_map.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#endif

namespace {

void AddStrings(content::WebUIDataSource* html_source) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"pageTitle", IDS_FRE_SIGN_IN_TITLE},
      {"pageSubtitle", IDS_FRE_SIGN_IN_SUBTITLE},
      {"devicesCardTitle", IDS_FRE_DEVICES_CARD_TITLE},
      {"devicesCardDescription", IDS_FRE_DEVICES_CARD_DESCRIPTION},
      {"securityCardTitle", IDS_FRE_SECURITY_CARD_TITLE},
      {"securityCardDescription", IDS_FRE_SECURITY_CARD_DESCRIPTION},
      {"backupCardTitle", IDS_FRE_BACKUP_CARD_TITLE},
      {"backupCardDescription", IDS_FRE_BACKUP_CARD_DESCRIPTION},
      {"declineSignInButtonTitle", IDS_FRE_DECLINE_SIGN_IN_BUTTON_TITLE},
      {"acceptSignInButtonTitle", IDS_FRE_ACCEPT_SIGN_IN_BUTTON_TITLE},
  };

  html_source->AddLocalizedStrings(kLocalizedStrings);
#endif
}

}  // namespace

IntroUI::IntroUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  DCHECK(base::FeatureList::IsEnabled(kForYouFre));
  web_ui->AddMessageHandler(std::make_unique<IntroHandler>());

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIIntroHost);

  webui::SetupWebUIDataSource(
      source, base::make_span(kIntroResources, kIntroResourcesSize),
      IDR_INTRO_INTRO_HTML);

  AddStrings(source);
}

IntroUI::~IntroUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(IntroUI)
