// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/intro/intro_ui.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/intro/intro_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/intro_resources.h"
#include "chrome/grit/intro_resources_map.h"
#include "chrome/grit/signin_resources.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_branded_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

IntroUI::IntroUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  auto* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIIntroHost);

  webui::SetupWebUIDataSource(
      source, base::make_span(kIntroResources, kIntroResourcesSize),
      IDR_INTRO_INTRO_HTML);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  int title_id = IDS_FRE_SIGN_IN_TITLE_0;
  int backupCardDescription =
      base::FeatureList::IsEnabled(switches::kExplicitBrowserSigninUIOnDesktop)
          ? IDS_UNO_FRE_BACKUP_CARD_DESCRIPTION
          : IDS_FRE_BACKUP_CARD_DESCRIPTION;

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
      {"pageSubtitle", IDS_FRE_SIGN_IN_SUBTITLE_0},
      {"devicesCardTitle", IDS_FRE_DEVICES_CARD_TITLE},
      {"devicesCardDescription", IDS_FRE_DEVICES_CARD_DESCRIPTION},
      {"securityCardTitle", IDS_FRE_SECURITY_CARD_TITLE},
      {"securityCardDescription", IDS_FRE_SECURITY_CARD_DESCRIPTION},
      {"backupCardTitle", IDS_FRE_BACKUP_CARD_TITLE},
      {"backupCardDescription", backupCardDescription},
      {"declineSignInButtonTitle", IDS_FRE_DECLINE_SIGN_IN_BUTTON_TITLE},
      {"acceptSignInButtonTitle", IDS_FRE_ACCEPT_SIGN_IN_BUTTON_TITLE},
      {"productLogoAltText", IDS_SHORT_PRODUCT_LOGO_ALT_TEXT},
      // Strings for default browser promo subpage.
      {"defaultBrowserTitle", IDS_FRE_DEFAULT_BROWSER_TITLE_NEW},
      {"defaultBrowserSubtitle", IDS_FRE_DEFAULT_BROWSER_SUBTITLE_NEW},
      {"defaultBrowserIllustrationAltText",
       IDS_FRE_DEFAULT_BROWSER_ILLUSTRATION_ALT_TEXT},
      {"defaultBrowserSetAsDefault", IDS_FRE_DEFAULT_BROWSER_SET_AS_DEFAULT},
      {"defaultBrowserSkip", IDS_FRE_DEFAULT_BROWSER_SKIP},
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      {"proceedLabel", IDS_PRIMARY_PROFILE_FIRST_RUN_NEXT_BUTTON_LABEL},
#endif
  };
  source->AddLocalizedStrings(localized_strings);

  const bool is_device_managed =
      policy::ManagementServiceFactory::GetForPlatform()->IsManaged();

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
  source->AddResourcePath("tangible_sync_style_shared_lit.css.js",
                          IDR_SIGNIN_TANGIBLE_SYNC_STYLE_SHARED_LIT_CSS_JS);
  source->AddResourcePath("signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  source->AddBoolean("isDeviceManaged", is_device_managed);

  // Setup chrome://intro/default-browser UI.
  source->AddResourcePath(chrome::kChromeUIIntroDefaultBrowserSubPage,
                          IDR_INTRO_DEFAULT_BROWSER_DEFAULT_BROWSER_HTML);

  source->AddResourcePath("images/product-logo.svg", IDR_PRODUCT_LOGO_SVG);
  source->AddResourcePath("images/product-logo-animation.svg",
                          IDR_PRODUCT_LOGO_ANIMATION_SVG);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath("images/gshield.svg", IDR_GSHIELD_ICON_SVG);
#endif
#endif

  // Unretained ok: `this` owns the handler.
  auto intro_handler = std::make_unique<IntroHandler>(
      base::BindRepeating(&IntroUI::HandleSigninChoice, base::Unretained(this)),
      base::BindOnce(&IntroUI::HandleDefaultBrowserChoice,
                     base::Unretained(this)),
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

void IntroUI::SetDefaultBrowserCallback(DefaultBrowserCallback callback) {
  DCHECK(!callback->is_null());
  default_browser_callback_ = std::move(callback);
  intro_handler_->ResetDefaultBrowserButtons();
}

void IntroUI::HandleSigninChoice(IntroChoice choice) {
  if (signin_choice_callback_->is_null()) {
    LOG(WARNING) << "Unexpected signin choice event";
  } else {
    std::move(signin_choice_callback_.value()).Run(choice);
  }
}

// For a given `IntroUI` instance, this will be called only once, even if
// `SetDefaultBrowserCallback()` is called again. This is because after the
// first call, the handler will drop the link, since it took a OnceCallback.
// This is fine because the step should not be shown more than once.
void IntroUI::HandleDefaultBrowserChoice(DefaultBrowserChoice choice) {
  if (default_browser_callback_->is_null()) {
    LOG(WARNING) << "Unexpected default browser choice event";
  } else {
    std::move(default_browser_callback_.value()).Run(choice);
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(IntroUI)
