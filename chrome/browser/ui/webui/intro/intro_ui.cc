// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/intro_ui.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/intro/intro_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/intro_resources.h"
#include "chrome/grit/intro_resources_map.h"
#include "chrome/grit/signin_resources.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/sync/base/features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/webui_util.h"

namespace {
int GetBackupCardDescriptionId(bool is_first_run_desktop_refresh_enabled) {
  if (!syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    return IDS_UNO_FRE_BACKUP_CARD_DESCRIPTION;
  }

  return is_first_run_desktop_refresh_enabled
             ? IDS_UNO_FRE_REFRESH_BACKUP_CARD_DESCRIPTION_WITH_PASSWORDS
             : IDS_UNO_FRE_BACKUP_CARD_DESCRIPTION_WITH_PASSWORDS;
}
}  // namespace

IntroUI::IntroUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  auto* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIIntroHost);

  const bool is_in_search_engine_choice_region =
      CHECK_DEREF(regional_capabilities::RegionalCapabilitiesServiceFactory::
                      GetForProfile(profile))
          .IsInSearchEngineChoiceScreenRegion();
  const bool is_first_run_desktop_refresh_enabled =
      switches::IsFirstRunDesktopRefreshEnabled(
          is_in_search_engine_choice_region);
  const bool is_first_run_desktop_revamp_enabled =
      switches::IsFirstRunDesktopRevampEnabled(
          is_in_search_engine_choice_region);
  webui::SetupWebUIDataSource(source, kIntroResources,
                              is_first_run_desktop_refresh_enabled
                                  ? IDR_INTRO_INTRO_REFRESH_HTML
                                  : IDR_INTRO_INTRO_HTML);

  const bool is_dont_sign_in_on_gaia_page_variation =
      is_first_run_desktop_refresh_enabled &&
      switches::kFirstRunDesktopSignInPromoVariation.Get() ==
          switches::FirstRunDesktopSignInPromoVariation::kDontSignInOnGaiaPage;

  const int title_id = is_dont_sign_in_on_gaia_page_variation
                           ? IDS_FRE_GET_YOUR_BROWSER_READY_TITLE
                           : IDS_FRE_SIGN_IN_TITLE_0;

  // Setting the title here instead of relying on the one provided from the
  // page itself makes it available much earlier, and avoids having to fallback
  // to the one obtained from `NavigationEntry::GetTitleForDisplay()` (which
  // ends up being the URL) when we try to get it on startup for a11y purposes.
  web_ui->OverrideTitle(l10n_util::GetStringUTF16(title_id));

  constexpr webui::LocalizedString localized_strings[] = {
      {"pageSubtitle", IDS_FRE_SIGN_IN_SUBTITLE_0},
      {"devicesCardTitle", IDS_FRE_DEVICES_CARD_TITLE},
      {"devicesCardDescription", IDS_FRE_DEVICES_CARD_DESCRIPTION},
      {"securityCardTitle", IDS_FRE_SECURITY_CARD_TITLE},
      {"securityCardDescription", IDS_FRE_SECURITY_CARD_DESCRIPTION},
      {"backupCardTitle", IDS_FRE_BACKUP_CARD_TITLE},
      {"acceptSignInButtonTitle", IDS_FRE_ACCEPT_SIGN_IN_BUTTON_TITLE},
      {"createAccountDisclaimer", IDS_FRE_CREATE_ACCOUNT_DESCRIPTION},
      {"productLogoAltText", IDS_SHORT_PRODUCT_LOGO_ALT_TEXT},
      // Strings for default browser promo subpage.
      {"defaultBrowserTitle", IDS_FRE_DEFAULT_BROWSER_TITLE_NEW},
      {"defaultBrowserSubtitle", IDS_FRE_DEFAULT_BROWSER_SUBTITLE_NEW},
      {"defaultBrowserIllustrationAltText",
       IDS_FRE_DEFAULT_BROWSER_ILLUSTRATION_ALT_TEXT},
      {"defaultBrowserSetAsDefault", IDS_FRE_DEFAULT_BROWSER_SET_AS_DEFAULT},
      {"defaultBrowserSkip", IDS_FRE_DEFAULT_BROWSER_SKIP},
      // Strings for refreshed default browser promo subpage.
      {"refreshDefaultBrowserTitle", IDS_FRE_REFRESH_DEFAULT_BROWSER_TITLE},
      {"refreshDefaultBrowserSubtitle",
       IDS_FRE_REFRESH_DEFAULT_BROWSER_SUBTITLE},
      {"refreshDefaultBrowserSetAsDefault",
       IDS_FRE_REFRESH_DEFAULT_BROWSER_SET_AS_DEFAULT},
      {"refreshDefaultBrowserNoThanks",
       IDS_FRE_REFRESH_DEFAULT_BROWSER_NO_THANKS},
  };
  source->AddLocalizedStrings(localized_strings);

  source->AddLocalizedString("pageTitle", title_id);
  source->AddLocalizedString(
      "backupCardDescription",
      GetBackupCardDescriptionId(is_first_run_desktop_refresh_enabled));
  source->AddLocalizedString(
      "declineSignInButtonTitle",
      base::FeatureList::IsEnabled(
          switches::kProfileCreationDeclineSigninCTAExperiment)
          ? IDS_FRE_STAY_SIGNED_OUT_BUTTON_TITLE
          : IDS_FRE_DECLINE_SIGN_IN_BUTTON_TITLE);

  source->AddLocalizedString("acceptSignInButtonTitle",
                             is_dont_sign_in_on_gaia_page_variation
                                 ? IDS_FRE_NEXT_BUTTON_TITLE
                                 : IDS_FRE_ACCEPT_SIGN_IN_BUTTON_TITLE);

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
  source->AddResourcePath("signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS);
  source->AddResourcePath(
      "animations/avatar_sign_in_celebration.json",
      IDR_SIGNIN_ANIMATIONS_AVATAR_SIGN_IN_CELEBRATION_JSON);
  source->AddResourcePath(
      "animations/avatar_sign_in_celebration_dark.json",
      IDR_SIGNIN_ANIMATIONS_AVATAR_SIGN_IN_CELEBRATION_DARK_JSON);

  source->AddString("accountPicturePlaceholderUrl",
                    profiles::GetPlaceholderAvatarIconUrl());
  source->AddBoolean("isDeviceManaged", is_device_managed);
  source->AddBoolean("usePrimaryAndTonalButtonsForPromos",
                     base::FeatureList::IsEnabled(
                         switches::kUsePrimaryAndTonalButtonsForPromos));
  if (base::FeatureList::IsEnabled(
          switches::kDisableFirstRunAnimationsForTesting)) {
    CHECK_IS_TEST();
    source->AddBoolean("disableAnimations", true);
  } else {
    source->AddBoolean("disableAnimations", false);
  }

  if (is_first_run_desktop_refresh_enabled) {
    source->AddInteger(
        "signInPromoVariation",
        static_cast<int>(switches::kFirstRunDesktopSignInPromoVariation.Get()));
  }

  // Setup chrome://intro/default-browser UI.
  source->AddResourcePath(
      chrome::kChromeUIIntroDefaultBrowserSubPage,
      is_first_run_desktop_refresh_enabled
          ? IDR_INTRO_DEFAULT_BROWSER_DEFAULT_BROWSER_REFRESH_HTML
          : IDR_INTRO_DEFAULT_BROWSER_DEFAULT_BROWSER_HTML);

  if (is_first_run_desktop_revamp_enabled) {
    source->AddResourcePath(
        chrome::kChromeUIIntroSignInCelebrationSubPage,
        IDR_INTRO_SIGN_IN_CELEBRATION_SIGN_IN_CELEBRATION_HTML);
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath("images/refresh_showcase_illustration.png",
                          IDR_DEFAULT_BROWSER_SHOWCASE_CHROME);
#else
  source->AddResourcePath(
      "images/refresh_showcase_illustration.png",
      IDR_INTRO_IMAGES_REFRESH_SHOWCASE_ILLUSTRATION_CHROMIUM_PNG);
#endif

  source->AddResourcePath("images/product-logo.svg", IDR_PRODUCT_LOGO_SVG);
  source->AddResourcePath("images/product-logo-animation.svg",
                          IDR_PRODUCT_LOGO_ANIMATION_SVG);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath("images/gshield.svg", IDR_GSHIELD_ICON_SVG);
#endif

  if (is_first_run_desktop_refresh_enabled) {
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::WorkerSrc,
        "worker-src blob: chrome://resources 'self';");
  }

  // Unretained ok: `this` owns the handler.
  auto intro_handler = std::make_unique<IntroHandler>(
      base::BindRepeating(&IntroUI::HandleSigninChoice, base::Unretained(this)),
      base::BindOnce(&IntroUI::HandleDefaultBrowserChoice,
                     base::Unretained(this)),
      is_device_managed, chrome::kChromeUIIntroHost);
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

  intro_handler_->ResetIntroButtons();
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

void IntroUI::SetCanPinToTaskbar(bool can_pin) {
  intro_handler_->SetCanPinToTaskbar(can_pin);
}

void IntroUI::BindInterface(
    mojo::PendingReceiver<intro::mojom::PageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void IntroUI::CreatePageHandler(
    mojo::PendingRemote<intro::mojom::Page> page,
    mojo::PendingReceiver<intro::mojom::PageHandler> receiver) {
  Profile* profile = Profile::FromWebUI(web_ui());
  intro_sign_in_celebration_handler_ =
      std::make_unique<SignInCelebrationHandler>(
          IdentityManagerFactory::GetForProfile(profile), std::move(page),
          std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(IntroUI)
