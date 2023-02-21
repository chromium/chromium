// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/intro_ui.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
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
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
std::string GetPictureUrl(content::WebUI& web_ui,
                          const ProfileAttributesEntry& profile_entry) {
  const int avatar_size = 100;
  const int avatar_icon_size = avatar_size * web_ui.GetDeviceScaleFactor();
  return webui::GetBitmapDataUrl(
      profiles::GetSizedAvatarIcon(
          profile_entry.GetAvatarIcon(avatar_icon_size), avatar_icon_size,
          avatar_icon_size)
          .AsBitmap());
}

std::string GetLacrosIntroWelcomeTitle(
    const ProfileAttributesEntry& profile_entry) {
  auto given_name = profile_entry.GetGAIAGivenName();
  base::UmaHistogramBoolean("Profile.LacrosFre.WelcomeHasGivenName",
                            given_name.empty());
  return !given_name.empty()
             ? l10n_util::GetStringFUTF8(IDS_PRIMARY_PROFILE_FIRST_RUN_TITLE,
                                         given_name)
             : l10n_util::GetStringUTF8(
                   IDS_PRIMARY_PROFILE_FIRST_RUN_NO_NAME_TITLE);
}

std::string GetLacrosIntroManagementDisclaimer(
    Profile& profile,
    const ProfileAttributesEntry& profile_entry) {
  // TODO(crbug.com/1416511): Fix logic mismatch in device/account management
  // between Lacros and DICE.
  const bool is_managed_account =
      profile.GetProfilePolicyConnector()->IsManaged();
  std::string hosted_domain = profile_entry.GetHostedDomain();
  if (!is_managed_account || hosted_domain == kNoHostedDomainFound) {
    return std::string();
  }

  if (hosted_domain.empty()) {
    const auto* identity_manager =
        IdentityManagerFactory::GetForProfile(&profile);
    const CoreAccountInfo core_account_info =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    AccountInfo account_info =
        identity_manager->FindExtendedAccountInfoByAccountId(
            core_account_info.account_id);
    hosted_domain = gaia::ExtractDomainName(account_info.email);
  }
  return l10n_util::GetStringFUTF8(
      IDS_PRIMARY_PROFILE_FIRST_RUN_SESSION_MANAGED_BY_DESCRIPTION,
      base::UTF8ToUTF16(hosted_domain));
}
#endif
}  // namespace

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
#endif

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
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    {"proceedLabel", IDS_PRIMARY_PROFILE_FIRST_RUN_NEXT_BUTTON_LABEL},
#endif
  };
  source->AddLocalizedStrings(localized_strings);

  // TODO(crbug.com/1409028): Replace this function by a call to
  // chrome::GetDeviceManagerIdentity()
  const bool is_device_managed = chrome::ShouldDisplayManagedUi(profile);
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath("images/gshield.svg", IDR_GSHIELD_ICON_SVG);
#endif
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const auto profile_path = profile->GetPath();
  const auto* profile_entry = g_browser_process->profile_manager()
                                  ->GetProfileAttributesStorage()
                                  .GetProfileAttributesWithPath(profile_path);
  DCHECK(profile_entry);

  source->AddString("pictureUrl", GetPictureUrl(*web_ui, *profile_entry));
  source->AddString("subtitle", l10n_util::GetStringFUTF8(
                                    IDS_PRIMARY_PROFILE_FIRST_RUN_SUBTITLE,
                                    profile_entry->GetUserName()));
  source->AddString("title", GetLacrosIntroWelcomeTitle(*profile_entry));
  source->AddString("enterpriseInfo", GetLacrosIntroManagementDisclaimer(
                                          *profile, *profile_entry));
  source->AddResourcePath(
      "images/lacros_intro_banner.svg",
      IDR_SIGNIN_ENTERPRISE_PROFILE_WELCOME_IMAGES_LACROS_ENTERPRISE_PROFILE_WELCOME_ILLUSTRATION_SVG);
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
