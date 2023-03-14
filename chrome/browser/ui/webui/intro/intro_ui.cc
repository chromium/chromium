// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/intro_ui.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/intro/intro_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/intro_resources.h"
#include "chrome/grit/intro_resources_map.h"
#include "chrome/grit/signin_resources.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_google_chrome_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

IntroUI::IntroUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  DCHECK(base::FeatureList::IsEnabled(kForYouFre));
  auto* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIIntroHost);

  webui::SetupWebUIDataSource(
      source, base::make_span(kIntroResources, kIntroResourcesSize),
      IDR_INTRO_INTRO_HTML);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  int title_id = 0;
  int subtitle_id = 0;
  switch (kForYouFreSignInPromoVariant.Get()) {
    case SigninPromoVariant::kSignIn: {
      title_id = IDS_FRE_SIGN_IN_TITLE_0;
      subtitle_id = IDS_FRE_SIGN_IN_SUBTITLE_0;
      break;
    }
    case SigninPromoVariant::kMakeYourOwn: {
      title_id = IDS_FRE_SIGN_IN_TITLE_1;
      subtitle_id = IDS_FRE_SIGN_IN_SUBTITLE_1;
      break;
    }
    case SigninPromoVariant::kDoMore: {
      title_id = IDS_FRE_SIGN_IN_TITLE_2;
      subtitle_id = IDS_FRE_SIGN_IN_SUBTITLE_1;
      break;
    }
    default:
      NOTREACHED();
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  int title_id = IDS_PRIMARY_PROFILE_FIRST_RUN_NO_NAME_TITLE;
#endif

  // Setting the title here instead of relying on the one provided from the
  // page itself makes it available much earlier, and avoids having to fallback
  // to the one obtained from `NavigationEntry::GetTitleForDisplay()` (which
  // ends up being the URL) when we try to get it on startup for a11y purposes.
  web_ui->OverrideTitle(l10n_util::GetStringUTF16(title_id));

  webui::LocalizedString localized_strings[] = {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    {"pageTitle", title_id},
    {"pageSubtitle", subtitle_id},
    {"devicesCardTitle", IDS_FRE_DEVICES_CARD_TITLE},
    {"devicesCardDescription", IDS_FRE_DEVICES_CARD_DESCRIPTION},
    {"securityCardTitle", IDS_FRE_SECURITY_CARD_TITLE},
    {"securityCardDescription", IDS_FRE_SECURITY_CARD_DESCRIPTION},
    {"backupCardTitle", IDS_FRE_BACKUP_CARD_TITLE},
    {"backupCardDescription", IDS_FRE_BACKUP_CARD_DESCRIPTION},
    {"declineSignInButtonTitle", IDS_FRE_DECLINE_SIGN_IN_BUTTON_TITLE},
    {"acceptSignInButtonTitle", IDS_FRE_ACCEPT_SIGN_IN_BUTTON_TITLE},
    {"productLogoAltText", IDS_SHORT_PRODUCT_LOGO_ALT_TEXT},
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    {"proceedLabel", IDS_PRIMARY_PROFILE_FIRST_RUN_NEXT_BUTTON_LABEL},
#endif
  };
  source->AddLocalizedStrings(localized_strings);

  const bool is_device_managed =
      policy::ManagementServiceFactory::GetForPlatform()->IsManaged();
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  source->AddBoolean("isDeviceManaged", is_device_managed);

  source->AddResourcePath("images/product-logo.svg", IDR_PRODUCT_LOGO_SVG);
  source->AddResourcePath("images/product-logo-animation.svg",
                          IDR_PRODUCT_LOGO_ANIMATION_SVG);
  source->AddResourcePath("images/left_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_SVG);
  source->AddResourcePath("images/left_illustration_dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_DARK_SVG);
  source->AddResourcePath("images/right_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_SVG);
  source->AddResourcePath("images/right_illustration_dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_DARK_SVG);
  source->AddResourcePath("tangible_sync_style_shared.css.js",
                          IDR_SIGNIN_TANGIBLE_SYNC_STYLE_SHARED_CSS_JS);
  source->AddResourcePath("signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath("images/gshield.svg", IDR_GSHIELD_ICON_SVG);
#endif
#endif

  // Unretained ok: `this` owns the handler.
  auto intro_handler = std::make_unique<IntroHandler>(
      base::BindRepeating(&IntroUI::HandleSigninChoice, base::Unretained(this)),
      is_device_managed);
  intro_handler_ = intro_handler.get();
  web_ui->AddMessageHandler(std::move(intro_handler));
}

IntroUI::~IntroUI() {
  if (!signin_choice_callback_->is_null()) {
    std::move(signin_choice_callback_.value()).Run(IntroChoice::kQuit);
  }
}

void IntroUI::SetSigninChoiceCallback(IntroSigninChoiceCallback callback) {
  DCHECK(!callback->is_null());
  signin_choice_callback_ = std::move(callback);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  intro_handler_->ResetIntroButtons();
#endif
}

void IntroUI::HandleSigninChoice(IntroChoice choice) {
  if (signin_choice_callback_->is_null()) {
    LOG(WARNING) << "Unexpected signin choice event";
  } else {
    std::move(signin_choice_callback_.value()).Run(choice);
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(IntroUI)
