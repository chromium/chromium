// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/intro_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/intro/intro_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/intro_resources.h"
#include "chrome/grit/intro_resources_map.h"
#include "chrome/grit/signin_resources.h"
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

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIIntroHost);

  webui::SetupWebUIDataSource(
      source, base::make_span(kIntroResources, kIntroResourcesSize),
      IDR_INTRO_INTRO_HTML);

  AddStrings(source);

  // TODO(crbug.com/1409028): Replace this function by a call to
  // chrome::GetDeviceManagerIdentity()
  bool is_device_managed =
      chrome::ShouldDisplayManagedUi(Profile::FromWebUI(web_ui));
  source->AddBoolean("isDeviceManaged", is_device_managed);

  source->AddResourcePath("product-logo.svg", IDR_PRODUCT_LOGO_SVG);
  source->AddResourcePath("product-logo-animation.svg",
                          IDR_PRODUCT_LOGO_ANIMATION_SVG);
  source->AddResourcePath(
      "left_illustration.svg",
      IDR_SIGNIN_SYNC_CONFIRMATION_IMAGES_TANGIBLE_SYNC_WINDOW_LEFT_ILLUSTRATION_SVG);
  source->AddResourcePath(
      "left_illustration_dark.svg",
      IDR_SIGNIN_SYNC_CONFIRMATION_IMAGES_TANGIBLE_SYNC_WINDOW_LEFT_ILLUSTRATION_DARK_SVG);
  source->AddResourcePath(
      "right_illustration.svg",
      IDR_SIGNIN_SYNC_CONFIRMATION_IMAGES_TANGIBLE_SYNC_WINDOW_RIGHT_ILLUSTRATION_SVG);
  source->AddResourcePath(
      "right_illustration_dark.svg",
      IDR_SIGNIN_SYNC_CONFIRMATION_IMAGES_TANGIBLE_SYNC_WINDOW_RIGHT_ILLUSTRATION_DARK_SVG);

  // Unretained ok: `this` owns the handler.
  web_ui->AddMessageHandler(std::make_unique<IntroHandler>(
      base::BindRepeating(&IntroUI::HandleSigninChoice, base::Unretained(this)),
      is_device_managed));
}

IntroUI::~IntroUI() = default;

void IntroUI::SetSigninChoiceCallback(IntroSigninChoiceCallback callback) {
  DCHECK(!callback->is_null());
  signin_choice_callback_ = std::move(callback);
}

void IntroUI::HandleSigninChoice(bool sign_in) {
  if (signin_choice_callback_->is_null()) {
    LOG(WARNING) << "Unexpected signin choice event";
  } else {
    // TODO(crbug.com/1347507): Reflect in the UI that the actions are not
    // available, with a spinner and/or disabled buttons.
    std::move(signin_choice_callback_.value()).Run(sign_in);
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(IntroUI)
