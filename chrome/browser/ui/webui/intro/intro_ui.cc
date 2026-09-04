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
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/intro/intro_handler.h"
#include "chrome/browser/ui/webui/intro/sign_in_promo_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/intro_resources.h"
#include "chrome/grit/intro_resources_map.h"
#include "chrome/grit/signin_resources.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/sync/base/features.h"
#include "content/public/browser/navigation_entry.h"
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

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
bool IsDefaultBrowserDisabledByPolicy() {
  const auto* local_state = g_browser_process->local_state();
  return local_state->IsManagedPreference(
             prefs::kDefaultBrowserSettingEnabled) &&
         !local_state->GetBoolean(prefs::kDefaultBrowserSettingEnabled);
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

bool ShouldShowDefaultBrowserToggle() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return !IsDefaultBrowserDisabledByPolicy() &&
         shell_integration::CanSetAsDefaultBrowser();
#else
  return false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

bool ShouldShowMetricsOptIn() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
  return !metrics::IsMetricsReportingPolicyManaged();
#else
  return false;
#endif
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
      // Strings for welcome subpage.
      {"welcomeTitle", IDS_FRE_WELCOME_TITLE},
      {"welcomeStartButtonLabel", IDS_FRE_WELCOME_START_BUTTON_LABEL},
      {"welcomeSetDefaultBrowser", IDS_FRE_DEFAULT_BROWSER_TITLE},
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
      // Strings for finish or continue subpage.
      {"finishOrContinueTitle", IDS_FRE_FINISH_OR_CONTINUE_TITLE},
      {"startBrowsingButtonLabel",
       IDS_FRE_FINISH_OR_CONTINUE_START_BROWSING_BUTTON_LABEL},
  };
  source->AddLocalizedStrings(localized_strings);

  // Metrics popup on welcome page is only shown for branded builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr webui::LocalizedString kMetricsStrings[] = {
      {"welcomeMetricsLabel", IDS_FRE_WELCOME_METRICS_LABEL},
      {"welcomeMetricsOffLabel", IDS_FRE_WELCOME_METRICS_OFF_LABEL},
      {"welcomeMetricsPopupTitle", IDS_FRE_WELCOME_METRICS_POPUP_TITLE},
      {"welcomeMetricsPopupDescription",
       IDS_FRE_WELCOME_METRICS_POPUP_DESCRIPTION},
      {"welcomeMetricsPopupTurnOffButtonLabel",
       IDS_FRE_WELCOME_METRICS_POPUP_TURN_OFF_BUTTON_LABEL},
      {"welcomeMetricsPopupTurnOnButtonLabel",
       IDS_FRE_WELCOME_METRICS_POPUP_TURN_ON_BUTTON_LABEL},
      {"welcomeMetricsPopupCloseButtonLabel",
       IDS_FRE_WELCOME_METRICS_POPUP_CLOSE_BUTTON_LABEL},
  };
  source->AddLocalizedStrings(kMetricsStrings);
#else
  source->AddString("welcomeMetricsLabel", "");
  source->AddString("welcomeMetricsOffLabel", "");
  source->AddString("welcomeMetricsPopupTitle", "");
  source->AddString("welcomeMetricsPopupDescription", "");
  source->AddString("welcomeMetricsPopupTurnOffButtonLabel", "");
  source->AddString("welcomeMetricsPopupTurnOnButtonLabel", "");
  source->AddString("welcomeMetricsPopupCloseButtonLabel", "");
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

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
  source->AddResourcePath(
      "images/shared_gradient_dark_background.svg",
      IDR_SIGNIN_IMAGES_SHARED_GRADIENT_DARK_BACKGROUND_SVG);
  source->AddResourcePath(
      "images/shared_gradient_light_background.svg",
      IDR_SIGNIN_IMAGES_SHARED_GRADIENT_LIGHT_BACKGROUND_SVG);
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
  source->AddBoolean("isFirstRunDesktopRevampEnabled",
                     is_first_run_desktop_revamp_enabled);
  source->AddBoolean("isPreFirstRunDesktopRefreshEnabled",
                     switches::IsPreFirstRunDesktopRefreshEnabled());
  source->AddBoolean("showDefaultBrowserToggle",
                     ShouldShowDefaultBrowserToggle());
  source->AddBoolean("showMetricsOptIn", ShouldShowMetricsOptIn());
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

  if (switches::IsPreFirstRunDesktopRefreshEnabled()) {
    source->AddResourcePath(
        chrome::kChromeUIIntroWelcomeSubPage,
        IDR_INTRO_WELCOME_WELCOME_HTML);
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
    source->AddResourcePath(
        chrome::kChromeUIIntroFinishOrContinueSubPage,
        IDR_INTRO_FINISH_OR_CONTINUE_FINISH_OR_CONTINUE_HTML);

    source->AddLocalizedString(
        "seeMoreTipsButtonLabel",
        IDS_FRE_FINISH_OR_CONTINUE_SEE_MORE_TIPS_BUTTON_LABEL);
    source->AddLocalizedString(
        "seeWhatsNewButtonLabel",
        IDS_FRE_FINISH_OR_CONTINUE_SEE_WHATS_NEW_BUTTON_LABEL);
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
  if (sign_in_promo_handler_) {
    sign_in_promo_handler_->ResetIntroButtons();
  }
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

void IntroUI::SetSignInCelebrationFinishedCallback(
    base::OnceClosure celebration_finished_callback) {
  initialize_handler_callback_ = base::BindOnce(
      &IntroUI::OnSignInCelebrationMojoHandlerReady,
      weak_ptr_factory_.GetWeakPtr(), std::move(celebration_finished_callback));
}

void IntroUI::BindInterface(
    mojo::PendingReceiver<intro::mojom::SignInCelebrationPageHandlerFactory>
        receiver) {
  sign_in_celebration_factory_receiver_.reset();
  sign_in_celebration_factory_receiver_.Bind(std::move(receiver));
}

void IntroUI::BindInterface(
    mojo::PendingReceiver<intro::mojom::SignInPromoPageHandlerFactory>
        receiver) {
  sign_in_promo_factory_receiver_.reset();
  sign_in_promo_factory_receiver_.Bind(std::move(receiver));
}

void IntroUI::CreateSignInCelebrationPageHandler(
    mojo::PendingRemote<intro::mojom::SignInCelebrationPage> page,
    mojo::PendingReceiver<intro::mojom::SignInCelebrationPageHandler>
        receiver) {
  CHECK(page);
  CHECK(receiver);
  if (!initialize_handler_callback_) {
    SetSignInCelebrationFinishedCallback(base::DoNothing());
  }
  std::move(initialize_handler_callback_)
      .Run(std::move(page), std::move(receiver));
}

void IntroUI::CreateSignInPromoPageHandler(
    mojo::PendingRemote<intro::mojom::SignInPromoPage> page,
    mojo::PendingReceiver<intro::mojom::SignInPromoPageHandler> receiver) {
  const bool is_device_managed =
      policy::ManagementServiceFactory::GetForPlatform()->IsManaged();

  sign_in_promo_handler_ = std::make_unique<SignInPromoHandler>(
      base::BindRepeating(&IntroUI::HandleSigninChoice, base::Unretained(this)),
      is_device_managed, std::move(page), std::move(receiver));
}

void IntroUI::OnSignInCelebrationMojoHandlerReady(
    base::OnceClosure celebration_finished_callback,
    mojo::PendingRemote<intro::mojom::SignInCelebrationPage> page,
    mojo::PendingReceiver<intro::mojom::SignInCelebrationPageHandler>
        receiver) {
  CHECK(!intro_sign_in_celebration_handler_);
  Profile* profile = Profile::FromWebUI(web_ui());
  intro_sign_in_celebration_handler_ =
      std::make_unique<SignInCelebrationHandler>(
          IdentityManagerFactory::GetForProfile(profile), std::move(page),
          std::move(receiver), std::move(celebration_finished_callback));
}

void IntroUI::BindInterface(
    mojo::PendingReceiver<intro::mojom::IntroPageHandlerFactory> receiver) {
  intro_factory_receiver_.reset();
  intro_factory_receiver_.Bind(std::move(receiver));
}

void IntroUI::CreateIntroPageHandler(
    mojo::PendingRemote<intro::mojom::IntroPage> page) {
  CHECK(page);
  intro_page_.reset();
  intro_page_.Bind(std::move(page));
  if (animations_active_.has_value()) {
    intro_page_->ToggleAnimations(*animations_active_);
  }
}

void IntroUI::ToggleAnimations(bool active) {
  animations_active_ = active;
  if (intro_page_.is_bound()) {
    intro_page_->ToggleAnimations(active);
  }
}

void IntroUI::SetFinishOrContinueCallback(
    base::OnceCallback<void(FinishOrContinueChoice)> callback) {
  CHECK(callback);
  finish_or_continue_callback_ = std::move(callback);
}

void IntroUI::BindInterface(
    mojo::PendingReceiver<intro::mojom::FinishOrContinuePageHandlerFactory>
        receiver) {
  finish_or_continue_factory_receiver_.reset();
  finish_or_continue_factory_receiver_.Bind(std::move(receiver));
}

void IntroUI::CreateFinishOrContinuePageHandler(
    mojo::PendingReceiver<intro::mojom::FinishOrContinuePageHandler> receiver) {
  CHECK(receiver);
  finish_or_continue_handler_ = std::make_unique<FinishOrContinueHandler>(
      base::BindOnce(&IntroUI::OnFinishOrContinueChoice,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(receiver));
}

void IntroUI::OnFinishOrContinueChoice(FinishOrContinueChoice choice) {
  if (finish_or_continue_callback_) {
    std::move(finish_or_continue_callback_).Run(choice);
  }
}

void IntroUI::SetWelcomeCallback(base::OnceClosure callback) {
  CHECK(callback);
  welcome_callback_ = std::move(callback);
}

void IntroUI::BindInterface(
    mojo::PendingReceiver<intro::mojom::WelcomePageHandlerFactory> receiver) {
  welcome_factory_receiver_.reset();
  welcome_factory_receiver_.Bind(std::move(receiver));
}

void IntroUI::CreateWelcomePageHandler(
    mojo::PendingReceiver<intro::mojom::WelcomePageHandler> receiver) {
  CHECK(receiver);
  welcome_handler_ = std::make_unique<WelcomeHandler>(
      base::BindOnce(&IntroUI::OnWelcomeContinue,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(receiver));
}

void IntroUI::OnWelcomeContinue() {
  CHECK(welcome_callback_);
  std::move(welcome_callback_).Run();
}

WEB_UI_CONTROLLER_TYPE_IMPL(IntroUI)
