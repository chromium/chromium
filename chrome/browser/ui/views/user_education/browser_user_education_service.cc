// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/reading_list_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/low_usage_help_controller.h"
#include "chrome/browser/ui/user_education/show_promo_in_page.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/user_education/autofill_help_bubble_factory.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/browser/user_education/tutorial_identifiers.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/compose/buildflags.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/lens/lens_features.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/safe_browsing/core/common/safebrowsing_referral_methods.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/new_badge_specification.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_registry.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_metadata.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/views/user_education/low_usage_promo.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/user_education/views/help_bubble_factory_views_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#include "components/user_education/views/help_bubble_factory_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

using Platforms = user_education::Metadata::Platforms;
constexpr std::initializer_list<Platforms> kComposePlatforms{
    Platforms::kWindows, Platforms::kMac, Platforms::kLinux};

constexpr char kTabGroupHeaderElementName[] = "TabGroupHeader";
constexpr char kChromeThemeBackElementName[] = "ChromeThemeBackElement";

class IfView : public user_education::TutorialDescription::If {
 public:
  template <typename V>
  IfView(user_education::TutorialDescription::ElementSpecifier element,
         base::RepeatingCallback<bool(const V*)> if_condition)
      : If(element,
           base::BindRepeating(
               [](base::RepeatingCallback<bool(const V*)> if_condition,
                  const ui::TrackedElement* el) {
                 return if_condition.Run(views::AsViewClass<V>(
                     el->AsA<views::TrackedElementViews>()->view()));
               },
               std::move(if_condition))) {}
};

class ScopedSavedTabGroupTutorialState
    : public user_education::ScopedTutorialState {
 public:
  explicit ScopedSavedTabGroupTutorialState(ui::ElementContext ctx)
      : user_education::ScopedTutorialState(ctx) {
    auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
    CHECK(browser);
    browser_ = browser->AsWeakPtr();
    browser_->SetForceShowBookmarkBarFlag(
        Browser::ForceShowBookmarkBarFlag::kTabGroupsTutorialActive);
  }

  ~ScopedSavedTabGroupTutorialState() override {
    if (browser_ && !browser_->IsBrowserClosing()) {
      browser_->ClearForceShowBookmarkBarFlag(
          Browser::ForceShowBookmarkBarFlag::kTabGroupsTutorialActive);
    }
  }

 private:
  base::WeakPtr<Browser> browser_;
};

bool HasTabGroups(const BrowserView* browser_view) {
  return !browser_view->browser()
              ->tab_strip_model()
              ->group_model()
              ->ListTabGroups()
              .empty();
}

}  // namespace

user_education::HelpBubbleDelegate* GetHelpBubbleDelegate() {
  static base::NoDestructor<BrowserHelpBubbleDelegate> delegate;
  return delegate.get();
}

void RegisterChromeHelpBubbleFactories(
    user_education::HelpBubbleFactoryRegistry& registry) {
  const user_education::HelpBubbleDelegate* const delegate =
      GetHelpBubbleDelegate();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(http://b/277994050): Move registration after Lacros launch.
  // Try to create an Ash-specific help bubble first. Note that an Ash-specific
  // help bubble will only take precedence over a standard Views-specific help
  // bubble if the tracked element's help bubble context is explicitly set to
  // `ash::HelpBubbleContext::kAsh`.
  registry.MaybeRegister<ash::HelpBubbleFactoryViewsAsh>(delegate);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  // Autofill bubbles require special handling.
  registry.MaybeRegister<AutofillHelpBubbleFactory>(delegate);
  registry.MaybeRegister<user_education::HelpBubbleFactoryViews>(delegate);
  // Try to create a floating bubble first, if it's allowed.
  registry.MaybeRegister<FloatingWebUIHelpBubbleFactoryBrowser>(delegate);
  // Fall back to in-WebUI help bubble if the floating bubble doesn't apply.
  registry.MaybeRegister<TabWebUIHelpBubbleFactoryBrowser>();
#if BUILDFLAG(IS_MAC)
  registry.MaybeRegister<user_education::HelpBubbleFactoryMac>(delegate);
#endif
}

void MaybeRegisterChromeFeaturePromos(
    user_education::FeaturePromoRegistry& registry,
    Profile* profile) {
  using user_education::FeaturePromoSpecification;
  using user_education::HelpBubbleArrow;
  using user_education::Metadata;
  using AdditionalCondition = user_education::FeaturePromoSpecification::
      AdditionalConditions::AdditionalCondition;
  using AdditionalConditions =
      user_education::FeaturePromoSpecification::AdditionalConditions;

  // This icon got updated, so select the 2023 Refresh version.
  // Note that the WebUI refresh state is not taken into account, so
  // this selection will affect both Views and WebUI help bubbles.
  const gfx::VectorIcon* const kLightbulbOutlineIcon =
      &vector_icons::kLightbulbOutlineChromeRefreshIcon;

  // Verify that we haven't already registered the expected features.
  // Use a known test feature that is unlikely to change.
  if (registry.IsFeatureRegistered(
          feature_engagement::kIPHWebUiHelpBubbleTestFeature)) {
    return;
  }

  // kIPHAutofillCreditCardBenefitFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillCreditCardBenefitFeature,
          kAutofillCreditCardBenefitElementId,
          IDS_AUTOFILL_CREDIT_CARD_BENEFIT_IPH_BUBBLE_LABEL,
          IDS_AUTOFILL_CREDIT_CARD_BENEFIT_IPH_BUBBLE_LABEL_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(125, "alexandertekle@google.com",
                       "Triggered after a credit card benefit is displayed for "
                       "the first time.")));

  // TODO(crbug.com/40264177): Use toast or snooze instead of legacy promo.
  // kIPHAutofillExternalAccountProfileSuggestionFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &feature_engagement::
                        kIPHAutofillExternalAccountProfileSuggestionFeature,
                    kAutofillSuggestionElementId,
                    IDS_AUTOFILL_IPH_EXTERNAL_ACCOUNT_PROFILE_SUGGESTION)
                    .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
                    .SetMetadata(115, "vykochko@chromium.org",
                                 "Triggered after autofill popup appears.")));

  // kIPHAutofillManualFallbackFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillManualFallbackFeature,
          kAutofillManualFallbackElementId, IDS_AUTOFILL_IPH_MANUAL_FALLBACK,
          IDS_AUTOFILL_IPH_MANUAL_FALLBACK_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetMetadata(
              123, "theocristea@chromium.org",
              "User focuses a field, but autofill cannot be triggered "
              "automatically because the field has autocomplete=garbage. In "
              "this case, autofill can be triggered from the context menu.")));

  // kIPHAutofillPredictionImprovementsFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHAutofillPredictionImprovementsFeature,
          kAutofillPredictionImprovementsIphElementId,
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_IPH_BODY,
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_IPH_GO_TO_SETTINGS,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                chrome::ShowSettingsSubPage(
                    browser, chrome::kAutofillPredictionImprovementsSubPage);
              }))
          .SetBubbleTitleText(IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_IPH_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetMetadata(131, "brunobraga@google.com",
                       "Displayed on input fields that are eligible for "
                       "improved predictions. These can be input fields on any "
                       "website, as long as the page and field supports the "
                       "feature. The IPH is displayed when the user clicks on "
                       "such input field and is anchored against it.")));

  // kIPHAutofillVirtualCardCVCSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature,
          kAutofillStandaloneCvcSuggestionElementId,
          IDS_AUTOFILL_VIRTUAL_CARD_STANDALONE_CVC_SUGGESTION_IPH_BUBBLE_LABEL,
          IDS_AUTOFILL_VIRTUAL_CARD_STANDALONE_CVC_SUGGESTION_IPH_BUBBLE_LABEL_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(118, "alexandertekle@google.com",
                       "Triggered after autofill popup appears.")));

  // kIPHAutofillVirtualCardSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature,
          kAutofillCreditCardSuggestionEntryElementId,
          IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_IPH_BUBBLE_LABEL)
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(100, "siyua@chromium.org",
                       "Triggered after autofill popup appears.")));

  // kIPHAutofillDisabledVirtualCardSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillDisabledVirtualCardSuggestionFeature,
          kAutofillCreditCardSuggestionEntryElementId,
          IDS_AUTOFILL_DISABLED_VIRTUAL_CARD_SUGGESTION_IPH_BUBBLE_LABEL_DESKTOP,
          IDS_AUTOFILL_DISABLED_VIRTUAL_CARD_SUGGESTION_IPH_BUBBLE_LABEL_DESKTOP_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(130, "hvs@google.com",
                       "Triggered after autofill popup appears for disabled "
                       "virtual card.")));

  // kIPHCreatePlusAddressSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHPlusAddressCreateSuggestionFeature,
          kPlusAddressCreateSuggestionElementId,
          IDS_PLUS_ADDRESS_CREATE_SUGGESTION_IPH,
          IDS_PLUS_ADDRESS_CREATE_SUGGESTION_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(128, "vidhanj@google.com",
                       "Triggered after create plus address popup appears.")));

  // kIPHDesktopPwaInstallFeature:
  registry.RegisterFeature(
      std::move(user_education::FeaturePromoSpecification::CreateForLegacyPromo(
                    &feature_engagement::kIPHDesktopPwaInstallFeature,
                    kInstallPwaElementId, IDS_DESKTOP_PWA_INSTALL_PROMO)
                    .SetMetadata(89, "phillis@chromium.org",
                                 "Triggered after user navigates to a "
                                 "page with a promotable PWA.")));

  // kIPHDesktopCustomizeChromeFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHDesktopCustomizeChromeFeature,
          kTopContainerElementId,
          IDS_TUTORIAL_CUSTOMIZE_CHROME_START_TUTORIAL_IPH,
          IDS_PROMO_SHOW_TUTORIAL_BUTTON,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                if (!search::DefaultSearchProviderIsGoogle(
                        browser->profile())) {
                  return;
                }
                auto* service =
                    UserEducationServiceFactory::GetForBrowserContext(
                        browser->profile());
                user_education::TutorialService* tutorial_service =
                    service ? &service->tutorial_service() : nullptr;
                if (!tutorial_service) {
                  return;
                }
                TabStripModel* tab_strip_model = browser->tab_strip_model();
                if (tab_strip_model) {
                  content::WebContents* web_contents =
                      tab_strip_model->GetActiveWebContents();
                  if (web_contents &&
                      web_contents->GetURL() != browser->GetNewTabURL()) {
                    NavigateParams params(browser->profile(),
                                          GURL(chrome::kChromeUINewTabPageURL),
                                          ui::PAGE_TRANSITION_LINK);
                    params.disposition =
                        WindowOpenDisposition::NEW_FOREGROUND_TAB;
                    Navigate(&params);
                  }
                }
                user_education::TutorialIdentifier tutorial_id =
                    kSidePanelCustomizeChromeTutorialId;

                tutorial_service->StartTutorial(tutorial_id, ctx);
                tutorial_service->LogIPHLinkClicked(tutorial_id, true);
              }))
          .SetBubbleArrow(HelpBubbleArrow::kNone)
          .SetBubbleIcon(kLightbulbOutlineIcon)
          .SetCustomActionIsDefault(true)
          .SetCustomActionDismissText(IDS_PROMO_SNOOZE_BUTTON)
          // This provides backwards-compatibility with legacy conditions used
          // before feature auto-configuration was enabled.
          .SetAdditionalConditions(std::move(
              AdditionalConditions().AddAdditionalCondition(AdditionalCondition{
                  feature_engagement::events::kCustomizeChromeOpened,
                  AdditionalConditions::Constraint::kAtMost, 0})))
          // See: crbug.com/1494923
          .OverrideFocusOnShow(false)));

  // kIPHDesktopCustomizeChromeRefreshFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature,
          kTopContainerElementId, IDS_IPH_CUSTOMIZE_CHROME_REFRESH_BODY,
          IDS_IPH_CUSTOMIZE_CHROME_REFRESH_CUSTOM_ACTION,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                ShowPromoInPage::Params params;
                params.bubble_anchor_id =
                    NewTabPageUI::kCustomizeChromeButtonElementId;
                params.bubble_arrow =
                    user_education::HelpBubbleArrow::kBottomRight;
                params.bubble_text = l10n_util::GetStringUTF16(
                    IDS_IPH_CUSTOMIZE_CHROME_REFRESH_POINTER_BODY);
                ShowPromoInPage::Start(browser, std::move(params));
              }))
          .SetBubbleArrow(HelpBubbleArrow::kNone)
          .SetCustomActionIsDefault(false)
          .SetCustomActionDismissText(IDS_PROMO_DISMISS_BUTTON)
          // This provides backwards-compatibility with legacy conditions used
          // before feature auto-configuration was enabled.
          .SetAdditionalConditions(std::move(
              AdditionalConditions().AddAdditionalCondition(AdditionalCondition{
                  feature_engagement::events::kCustomizeChromeOpened,
                  AdditionalConditions::Constraint::kAtMost, 0})))
          // See: crbug.com/1494923
          .OverrideFocusOnShow(false)
          .SetMetadata(119, "mickeyburks@chromium.org",
                       "Triggered after user is updated to "
                       "the new Chrome Refresh design.")));

  // kIPHDesktopNewTabPageModulesCustomizeFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHDesktopNewTabPageModulesCustomizeFeature,
          NewTabPageUI::kModulesCustomizeIPHAnchorElement,
          IDS_NTP_MODULES_CUSTOMIZE_IPH)
          .SetBubbleArrow(HelpBubbleArrow::kBottomRight)
          .SetBubbleIcon(kLightbulbOutlineIcon)
          .SetInAnyContext(true)
          // This provides backwards-compatibility with legacy conditions used
          // before feature auto-configuration was enabled.
          .SetAdditionalConditions(std::move(
              AdditionalConditions().AddAdditionalCondition(AdditionalCondition{
                  feature_engagement::events::kDesktopNTPModuleUsed,
                  AdditionalConditions::Constraint::kAtMost, 0})))
          // See: crbug.com/1494923
          .OverrideFocusOnShow(false)
          .SetMetadata(122, "romanarora@chromium.org",
                       "Triggered when there is atleast one "
                       "new module on the NTP page.")));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // kIPHDesktopReEngagementFeature:
  registry.RegisterFeature(
      std::move(CreateLowUsagePromoSpecification(profile).SetMetadata(
          126, "dfried@google.com",
          "Helpful messages for low-usage users; runs on new session.")));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // kIPHExperimentalAIPromoFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHExperimentalAIPromoFeature,
          kToolbarAppMenuButtonElementId, IDS_IPH_EXPERIMENTAL_AI_PROMO_BODY,
          IDS_IPH_EXPERIMENTAL_AI_PROMO_BUTTON_CONTINUE,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                chrome::ShowSettingsSubPage(
                    browser, chrome::kExperimentalAISettingsSubPage);
                base::RecordAction(base::UserMetricsAction(
                    "ExperimentalAI_IPHPromo_SettingsPageOpened"));
              }))
          .SetBubbleTitleText(IDS_IPH_EXPERIMENTAL_AI_PROMO)
          .SetCustomActionDismissText(IDS_NO_THANKS)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetCustomActionIsDefault(true)
          .OverrideFocusOnShow(false)));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // kIPHExtensionsMenuFeature:
  registry.RegisterFeature(std::move(
      user_education::FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHExtensionsMenuFeature,
          kExtensionsMenuButtonElementId,
          IDS_EXTENSIONS_MENU_IPH_ENTRY_POINT_BODY)
          .SetBubbleTitleText(IDS_EXTENSIONS_MENU_IPH_ENTRY_POINT_TITLE)
          .SetMetadata(
              117, "emiliapaz@chromium.org",
              "Triggered when an extension already has access permission.")));

  // kIPHExtensionsRequestAccessButtonFeature
  registry.RegisterFeature(std::move(
      user_education::FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHExtensionsRequestAccessButtonFeature,
          kExtensionsRequestAccessButtonElementId,
          IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_IPH_ENTRY_POINT_BODY)
          .SetBubbleTitleText(
              IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_IPH_ENTRY_POINT_TITLE)
          .SetMetadata(117, "emiliapaz@chromium.org",
                       "Triggered when an extension "
                       "requests access permission.")));
#endif

  // kIPHLiveCaptionFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHLiveCaptionFeature, kToolbarMediaButtonElementId,
      IDS_LIVE_CAPTION_PROMO, IDS_LIVE_CAPTION_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo()));

  // kIPHTabAudioMutingFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHTabAudioMutingFeature,
          kTabAlertIndicatorButtonElementId, IDS_TAB_AUDIO_MUTING_PROMO,
          IDS_LIVE_CAPTION_PROMO_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopCenter)));

  // kIPHGMCCastStartStopFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHGMCCastStartStopFeature,
      kToolbarMediaButtonElementId,
      IDS_GLOBAL_MEDIA_CONTROLS_CONTROL_CAST_SESSIONS_PROMO));

  // kIPHGMCLocalMediaCastingFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHGMCLocalMediaCastingFeature,
      kToolbarMediaButtonElementId, IDS_GMC_LOCAL_MEDIA_CAST_SESSIONS_PROMO,
      IDS_GMC_LOCAL_MEDIA_CAST_START_PROMO,
      FeaturePromoSpecification::AcceleratorInfo()));

  // kIPHPasswordsManagementBubbleAfterSaveFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHPasswordsManagementBubbleAfterSaveFeature,
          kPasswordsOmniboxKeyIconElementId,
          IDS_PASSWORD_MANAGER_IPH_MANAGEMENT_BUBBLE_AFTER_SAVE,
          IDS_PASSWORD_MANAGER_IPH_MANAGEMENT_BUBBLE_AFTER_SAVE_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetMetadata(113, "mamir@chromium.org",
                       "Triggered once when user has saved a password.")));

  // kIPHPasswordsManagementBubbleDuringSigninFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHPasswordsManagementBubbleDuringSigninFeature,
      kPasswordsOmniboxKeyIconElementId,
      IDS_PASSWORD_MANAGER_IPH_MANAGEMENT_BUBBLE_DURING_SIGNIN,
      IDS_PASSWORD_MANAGER_IPH_MANAGEMENT_BUBBLE_DURING_SIGNIN_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo()));

  // kIPHPasswordManagerShortcutFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForTutorialPromo(
          feature_engagement::kIPHPasswordManagerShortcutFeature,
          kPasswordsOmniboxKeyIconElementId,
          IDS_PASSWORD_MANAGER_IPH_CREATE_SHORTCUT_BODY,
          kPasswordManagerTutorialId)
          .SetBubbleArrow(HelpBubbleArrow::kBottomRight)
          .SetBubbleIcon(kLightbulbOutlineIcon)
          .SetBubbleTitleText(IDS_PASSWORD_MANAGER_IPH_CREATE_SHORTCUT_TITLE)));

  // kIPHPasswordSharingFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForToastPromo(
                    feature_engagement::kIPHPasswordSharingFeature,
                    PasswordManagerUI::kSharePasswordElementId,
                    IDS_PASSWORD_MANAGER_IPH_SHARE_PASSWORD_BUTTON,
                    IDS_PASSWORD_MANAGER_IPH_SHARE_PASSWORD_BUTTON_SCREENREADER,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetInAnyContext(true)
                    .SetBubbleIcon(kLightbulbOutlineIcon)
                    .SetBubbleArrow(HelpBubbleArrow::kTopRight)));

  // kIPHPowerBookmarksSidePanelFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForSnoozePromo(
                    feature_engagement::kIPHPowerBookmarksSidePanelFeature,
                    kToolbarAppMenuButtonElementId,
                    IDS_POWER_BOOKMARKS_SIDE_PANEL_PROMO_PINNING)
                    .SetHighlightedMenuItem(
                        BookmarkSubMenuModel::kShowBookmarkSidePanelItem)
                    .SetMetadata(121, "emshack@chromium.org",
                                 "Triggered when a bookmark is added from the "
                                 "bookmark page action in omnibox.")));

  // kIPHCompanionSidePanelFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHCompanionSidePanelFeature,
          kToolbarAppMenuButtonElementId,
          IDS_SIDE_PANEL_COMPANION_PROMO_PINNING,
          IDS_SIDE_PANEL_COMPANION_PROMO_SCREEN_READER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetHighlightedMenuItem(AppMenuModel::kShowSearchCompanion)
          .SetMetadata(
              115, "corising@chromium.org",
              "Triggered to encourage users to try out the CSC feature.")));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // kIPHSwitchProfileFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHProfileSwitchFeature,
      kToolbarAvatarButtonElementId, IDS_PROFILE_SWITCH_PROMO,
      IDS_PROFILE_SWITCH_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo(IDC_SHOW_AVATAR_MENU)));

  // kIPHPasswordsWebAppProfileSwitchFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature,
      kToolbarAvatarButtonElementId,
      IDS_PASSWORD_MANAGER_IPH_BODY_WEB_APP_PROFILE_SWITCH,
      IDS_PROFILE_SWITCH_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo()));

  // kIPHSignoutWebInterceptFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHSignoutWebInterceptFeature,
          kToolbarAvatarButtonElementId,
          IDS_SIGNOUT_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNOUT_IPH_TEXT,
          IDS_SIGNOUT_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNOUT_IPH_TEXT_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetPromoSubtype(user_education::FeaturePromoSpecification::
                               PromoSubtype::kKeyedNotice)));

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  // kIPHExplicitBrowserSigninPreferenceRememberedFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::
              kIPHExplicitBrowserSigninPreferenceRememberedFeature,
          kToolbarAvatarButtonElementId,
          IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_IPH_TEXT_SIGNIN_REMINDER,
          IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_IPH_TEXT_SIGNIN_REMINDER_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo(IDC_SHOW_AVATAR_MENU))
          .SetPromoSubtype(user_education::FeaturePromoSpecification::
                               PromoSubtype::kKeyedNotice)
          .SetBubbleTitleText(
              IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_IPH_TITLE_SIGNIN_REMINDER)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetBubbleIcon(&vector_icons::kCelebrationIcon)
          .SetReshowPolicy(base::Days(14), /*max_show_count=*/6)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)

  // kIPHCookieControlsFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHCookieControlsFeature,
          kCookieControlsIconElementId, IDS_COOKIE_CONTROLS_PROMO_TEXT,
          IDS_COOKIE_CONTROLS_PROMO_SEE_HOW_BUTTON_TEXT,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* cookie_controls_icon_view =
                    views::ElementTrackerViews::GetInstance()
                        ->GetFirstMatchingViewAs<CookieControlsIconView>(
                            kCookieControlsIconElementId, ctx);
                if (cookie_controls_icon_view != nullptr) {
                  cookie_controls_icon_view->ShowCookieControlsBubble();
                }
              }))
          .SetBubbleTitleText(IDS_COOKIE_CONTROLS_PROMO_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetBubbleIcon(kLightbulbOutlineIcon)
          .SetCustomActionIsDefault(true)
          .SetCustomActionDismissText(
              IDS_COOKIE_CONTROLS_PROMO_CLOSE_BUTTON_TEXT)));

  // kIPHReadingListDiscoveryFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &feature_engagement::kIPHReadingListDiscoveryFeature,
                    kToolbarAppMenuButtonElementId,
                    IDS_READING_LIST_DISCOVERY_PROMO_PINNING)
                    .SetHighlightedMenuItem(
                        ReadingListSubMenuModel::kReadingListMenuShowUI)));

  // kIPHReadingListEntryPointFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
      feature_engagement::kIPHReadingListEntryPointFeature,
      kBookmarkStarViewElementId, IDS_READING_LIST_ENTRY_POINT_PROMO));

  // kIPHReadingListInSidePanelFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHReadingListInSidePanelFeature,
          kToolbarAppMenuButtonElementId,
          IDS_READING_LIST_IN_SIDE_PANEL_PROMO_PINNING)
          .SetHighlightedMenuItem(BookmarkSubMenuModel::kReadingListMenuItem)));

  // kIPHReadingModeSidePanelFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHReadingModeSidePanelFeature,
          kToolbarAppMenuButtonElementId,
          IDS_READING_MODE_SIDE_PANEL_PROMO_PINNING)
          .SetHighlightedMenuItem(ToolsMenuModel::kReadingModeMenuItem)
          .SetMetadata(115, "jocelyntran@chromium.org",
                       "Triggered to encourage users to try out the reading "
                       "mode feature.")));

  // kIPHSidePanelGenericPinnableFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHSidePanelGenericPinnableFeature,
          kSidePanelPinButtonElementId, IDS_SIDE_PANEL_GENERIC_PINNABLE_IPH,
          IDS_SIDE_PANEL_GENERIC_PINNABLE_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetMetadata(121, "corising@chromium.org",
                       "Triggered when a pinnable side panel is opened.")));

  // kIPHSidePanelLensOverlayPinnableFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHSidePanelLensOverlayPinnableFeature,
          kSidePanelPinButtonElementId,
          IDS_SIDE_PANEL_LENS_OVERLAY_PINNABLE_IPH,
          IDS_SIDE_PANEL_LENS_OVERLAY_PINNABLE_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kRightCenter)
          .SetMetadata(126, "dfried@chromium.org, jdonnelly@google.com",
                       "Triggered when a pinnable lens overlay side panel is "
                       "opened.")));

  // kIPHSidePanelLensOverlayPinnableFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHSidePanelLensOverlayPinnableFollowupFeature,
          kPinnedActionToolbarButtonElementId,
          IDS_SIDE_PANEL_LENS_OVERLAY_PINNABLE_FOLLOWUP_IPH,
          IDS_SIDE_PANEL_LENS_OVERLAY_PINNABLE_FOLLOWUP_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetBubbleIcon(&vector_icons::kCelebrationIcon)
          .SetMetadata(126, "dfried@chromium.org, jdonnelly@google.com",
                       "Triggered when the lens overlay side panel is pinned.")
          .SetAnchorElementFilter(base::BindRepeating(
              [](const ui::ElementTracker::ElementList& elements)
                  -> ui::TrackedElement* {
                // Locate the action button associated with the Lens Overlay
                // feature. The button must be present in the Actions
                // container in the toolbar.
                for (auto* element : elements) {
                  auto* const button =
                      views::AsViewClass<PinnedActionToolbarButton>(
                          element->AsA<views::TrackedElementViews>()->view());
                  if (button && button->GetActionId() ==
                                    kActionSidePanelShowLensOverlayResults) {
                    return element;
                  }
                }
                return nullptr;
              }))));

  if (tab_groups::IsTabGroupsSaveV2Enabled()) {
    registry.RegisterFeature(std::move(
        FeaturePromoSpecification::CreateForToastPromo(
            feature_engagement::kIPHTabGroupsSaveV2CloseGroupFeature,
            kToolbarAppMenuButtonElementId,
            IDS_SAVED_TAB_GROUPS_V2_INTRO_IPH_APP_MENU_NOT_SYNCED_BODY,
            IDS_SAVED_TAB_GROUPS_V2_INTRO_DEFAULT_BODY_A11Y,
            FeaturePromoSpecification::AcceleratorInfo())
            .SetBubbleArrow(HelpBubbleArrow::kTopRight)
            .SetAnchorElementFilter(
                base::BindRepeating(&tab_groups::SavedTabGroupUtils::
                                        GetAnchorElementForTabGroupsV2IPH))
            .SetMetadata(127, "dpenning@chromium.org",
                         "triggered on startup when the saved tab groups are "
                         "defaulted to saved for the first time.")));

    registry.RegisterFeature(std::move(
        FeaturePromoSpecification::CreateForCustomAction(
            feature_engagement::kIPHTabGroupsSaveV2IntroFeature,
            kToolbarAppMenuButtonElementId,
            IDS_WILDCARD,  // Replaced by caller with the correct IDS string.
            IDS_LEARN_MORE,
            base::BindRepeating(
                [](ui::ElementContext ctx,
                   user_education::FeaturePromoHandle promo_handle) {
                  auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                  if (!browser) {
                    return;
                  }

                  const GURL learn_more_link{chrome::kTabGroupsLearnMoreURL};
                  NavigateParams params(browser->profile(), learn_more_link,
                                        ui::PAGE_TRANSITION_LINK);
                  params.disposition =
                      WindowOpenDisposition::NEW_FOREGROUND_TAB;
                  params.browser = browser;
                  Navigate(&params);
                }))
            .SetBubbleArrow(HelpBubbleArrow::kTopRight)
            .SetAnchorElementFilter(
                base::BindRepeating(&tab_groups::SavedTabGroupUtils::
                                        GetAnchorElementForTabGroupsV2IPH))
            .SetMetadata(127, "dpenning@chromium.org",
                         "triggered on startup when the saved tab groups are "
                         "defaulted to saved for the first time.")));
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // kIPHSupervisedUserProfileSigninFeature
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHSupervisedUserProfileSigninFeature,
          kToolbarAvatarButtonElementId,
          IDS_SUPERVISED_USER_PROFILE_SIGNIN_IPH_TEXT, IDS_LEARN_MORE,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                // Open parental controls page.
                ShowSingletonTab(
                    browser,
                    GURL(supervised_user::kManagedByParentUiMoreInfoUrl));
                base::RecordAction(base::UserMetricsAction(
                    "SupervisedUserProfileSignIn_IPHPromo_"
                    "ParentalControlsPageOpened"));
              }))
          // TODO(b/351333491): Clarify if we need to mark and approve as a
          // keyed promo, displaying the IPH once per key per device.
          .SetBubbleIcon(&vector_icons::kFamilyLinkIcon)
          .SetBubbleTitleText(IDS_SUPERVISED_USER_PROFILE_SIGNIN_IPH_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetCustomActionIsDefault(false)
          .SetMetadata(128, "anthie@google.com",
                       "Triggered on signin-in a supervised user to "
                       "a new profile or an existing local profile")));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

  // kIPHTabOrganizationSuccessFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHTabOrganizationSuccessFeature,
          kTabGroupHeaderElementId, IDS_TAB_ORGANIZATION_SUCCESS_IPH,
          IDS_TAB_ORGANIZATION_SUCCESS_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
          .SetMetadata(121, "dpenning@chromium.org",
                       "Triggered when tab organization is accepted.")));

  // kIPHTabSearchFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &feature_engagement::kIPHTabSearchFeature,
                    kTabSearchButtonElementId, IDS_TAB_SEARCH_PROMO)
                    .SetBubbleArrow(user_education::HelpBubbleArrow::kTopLeft)
                    .SetMetadata(92, "tluk@chromium.org",
                                 "Triggered once when there are more than 8 "
                                 "tabs in the tab strip.")));

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  // kIPHWebUITabStripFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHWebUITabStripFeature,
          kToolbarTabCounterButtonElementId, IDS_WEBUI_TAB_STRIP_PROMO)
          .SetMetadata(81, "dpenning@chromium.org",
                       "Triggered when touch mode changed.")));
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  // kIPHDesktopSharedHighlightingFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &feature_engagement::kIPHDesktopSharedHighlightingFeature,
                    kTopContainerElementId, IDS_SHARED_HIGHLIGHTING_PROMO)
                    .SetBubbleArrow(HelpBubbleArrow::kNone)));

  // kIPHWebUiHelpBubbleTestFeature
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHWebUiHelpBubbleTestFeature,
          kWebUIIPHDemoElementIdentifier,
          IDS_PASSWORD_MANAGER_IPH_BODY_SAVE_TO_ACCOUNT)
          .SetBubbleTitleText(IDS_PASSWORD_MANAGER_IPH_TITLE_SAVE_TO_ACCOUNT)
          .SetInAnyContext(true)
          .SetBubbleArrow(HelpBubbleArrow::kBottomRight)
          .SetBubbleIcon(&vector_icons::kCelebrationIcon)
          .SetMetadata(
              90, "dfried@chromium.org",
              "This is a test IPH, designed to verify that IPH can attach to "
              "elements in WebUI in the main browser tab.",
              // These are not required features; they are just an example to
              // ensure that the tester page formats this data correctly.
              Metadata::FeatureSet{
                  &feature_engagement::kIPHWebUiHelpBubbleTestFeature})));

  // kIPHBatterySaverModeFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHBatterySaverModeFeature,
          kToolbarBatterySaverButtonElementId,
          IDS_BATTERY_SAVER_MODE_PROMO_TEXT,
          IDS_BATTERY_SAVER_MODE_PROMO_ACTION_TEXT,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                if (browser) {
                  chrome::ShowSettingsSubPage(browser,
                                              chrome::kPerformanceSubPage);
                }
                RecordBatterySaverIPHOpenSettings(browser != nullptr);
              }))
          .SetBubbleTitleText(IDS_BATTERY_SAVER_MODE_PROMO_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetMetadata(108, "agale@chromium.org",
                       "Triggered when Battery Saver Mode is active.")));

  // kIPHMemorySaverModeFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHMemorySaverModeFeature,
          kToolbarAppMenuButtonElementId, IDS_MEMORY_SAVER_MODE_PROMO_TEXT,
          IDS_MEMORY_SAVER_MODE_PROMO_ACTION_TEXT,
          base::BindRepeating(
              [](ui::ElementContext context,
                 user_education::FeaturePromoHandle promo_handle) {
                performance_manager::user_tuning::UserPerformanceTuningManager::
                    GetInstance()
                        ->SetMemorySaverModeEnabled(true);
                RecordMemorySaverIPHEnableMode(true);
              }))
          .SetCustomActionIsDefault(true)
          .SetCustomActionDismissText(IDS_NO_THANKS)
          .SetBubbleTitleText(IDS_MEMORY_SAVER_MODE_PROMO_TITLE)
          .SetHighlightedMenuItem(ToolsMenuModel::kPerformanceMenuItem)
          .SetPromoSubtype(
              FeaturePromoSpecification::PromoSubtype::kActionableAlert)
          .SetMetadata(108, "agale@chromium.org",
                       "Triggered when device is low on memory.")));

  // kIPHDiscardRingFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHDiscardRingFeature, kTabIconElementId,
          IDS_DISCARD_RING_PROMO_TEXT, IDS_DISCARD_RING_PROMO_ACTION_TEXT,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                ShowPromoInPage::Params params;
                params.target_url =
                    chrome::GetSettingsUrl(chrome::kPerformanceSubPage);
                params.bubble_anchor_id = kInactiveTabSettingElementId;
                params.bubble_arrow =
                    user_education::HelpBubbleArrow::kBottomRight;
                params.bubble_text =
                    l10n_util::GetStringUTF16(IDS_DISCARD_RING_SETTINGS_TOAST);
                params.close_button_alt_text_id = IDS_CLOSE_PROMO;

                ShowPromoInPage::Start(browser, std::move(params));
              }))
          .SetAnchorElementFilter(base::BindRepeating(
              [](const ui::ElementTracker::ElementList& elements)
                  -> ui::TrackedElement* {
                for (auto* element : elements) {
                  auto* tab_icon = views::AsViewClass<TabIcon>(
                      element->AsA<views::TrackedElementViews>()->view());
                  if (tab_icon->GetShowingDiscardIndicator()) {
                    return element;
                  }
                }
                return nullptr;
              }))
          .SetCustomActionDismissText(IDS_PROMO_DISMISS_BUTTON)
          .SetBubbleTitleText(IDS_DISCARD_RING_PROMO_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
          // See: crbug.com/358451018
          .OverrideFocusOnShow(false)
          .SetMetadata(126, "agale@chromium.org",
                       "Triggered when a tab is discarded.")));

  // kIPHPriceTrackingInSidePanelFeature;
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHPriceTrackingInSidePanelFeature,
          kToolbarSidePanelButtonElementId, IDS_PRICE_TRACKING_SIDE_PANEL_IPH)
          .SetMetadata(120, "yuezhanggg@chromium.org",
                       "Triggered when a price tracking is enabled.")));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // kIPHDownloadEsbPromoFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHDownloadEsbPromoFeature,
          kToolbarDownloadButtonElementId, IDS_DOWNLOAD_BUBBLE_ESB_PROMO,
          IDS_DOWNLOAD_BUBBLE_ESB_PROMO_CUSTOM_ACTION,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                chrome::ShowSafeBrowsingEnhancedProtectionWithIph(
                    browser, safe_browsing::SafeBrowsingSettingReferralMethod::
                                 kDownloadButtonIphPromo);
              }))
          .SetCustomActionIsDefault(true)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetBubbleTitleText(IDS_DOWNLOAD_BUBBLE_ESB_PROMO_TITLE)
          .SetCustomActionDismissText(IDS_DOWNLOAD_BUBBLE_ESB_PROMO_DISMISS)
          .SetBubbleIcon(&vector_icons::kGshieldIcon)
          .SetPromoSubtype(
              FeaturePromoSpecification::PromoSubtype::kActionableAlert)
          .SetMetadata(
              122, "awado@chromium.org",
              "Triggered when user is using standard protection mode.")));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // kIPHBackNavigationMenuFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForSnoozePromo(
                    feature_engagement::kIPHBackNavigationMenuFeature,
                    kToolbarBackButtonElementId, IDS_BACK_NAVIGATION_MENU_PROMO,
                    IDS_BACK_NAVIGATION_MENU_PROMO_ACCESSIBLE_TEXT,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetBubbleArrow(HelpBubbleArrow::kTopLeft)));

  // kIPHLensOverlayTranslateButtonFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHLensOverlayTranslateButtonFeature,
          kLensOverlayTranslateButtonElementId,
          IDS_LENS_OVERLAY_TRANSLATE_BUTTON_IPH,
          IDS_LENS_OVERLAY_TRANSLATE_BUTTON_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetInAnyContext(true)
          .SetMetadata(131, "juanmojica@google.com",
                       "Triggered to inform users of the availability of the "
                       "new translate screen feature on the Lens Overlay.")));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  // kIPHDesktopPWAsLinkCapturingLaunch:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch,
          kToolbarAppMenuButtonElementId, IDS_DESKTOP_PWA_LINK_CAPTURING_TEXT,
          IDS_DESKTOP_PWA_LINK_CAPTURING_SETTINGS,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* const browser =
                    chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                TabStripModel* const tab_strip_model =
                    browser->tab_strip_model();
                if (!tab_strip_model) {
                  return;
                }
                content::WebContents* const web_contents =
                    tab_strip_model->GetActiveWebContents();
                const webapps::AppId* app_id =
                    web_app::WebAppTabHelper::GetAppId(web_contents);
                if (!app_id) {
                  return;
                }
                const GURL final_url(chrome::kChromeUIWebAppSettingsURL +
                                     *app_id);
                if (web_contents &&
                    web_contents->GetURL() != browser->GetNewTabURL()) {
                  NavigateParams params(browser->profile(), final_url,
                                        ui::PAGE_TRANSITION_LINK);
                  params.disposition =
                      WindowOpenDisposition::NEW_FOREGROUND_TAB;
                  Navigate(&params);
                }
              }))
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetPromoSubtype(user_education::FeaturePromoSpecification::
                               PromoSubtype::kKeyedNotice)
          .SetMetadata(122, "dibyapal@chromium.org",
                       "Triggered once per-app when a link is captured and "
                       "opened in a PWA.")));

  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHHistorySearchFeature,
          kHistorySearchInputElementId, IDS_HISTORY_EMBEDDINGS_IPH_BODY,
          IDS_HISTORY_EMBEDDINGS_IPH_ACTION,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* const browser =
                    chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                chrome::ShowSettingsSubPage(browser,
                                            chrome::kHistorySearchSubpage);
              }))
          .SetCustomActionIsDefault(true)
          .SetCustomActionDismissText(IDS_NO_THANKS)
          .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
          .SetInAnyContext(true)
          .SetMetadata(130, "johntlee@chromium.org",
                       "Triggered after user lands on chrome://history.")));

  // kIPHToolbarManagementButtonFeature
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHToolbarManagementButtonFeature,
          kToolbarManagementButtonElementId,
          IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED, IDS_LEARN_MORE,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* const browser =
                    chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                chrome::ShowEnterpriseManagementPageInTabbedBrowser(browser);
              }))
          .SetCustomActionIsDefault(true)
          .SetMetadata(129, "ydago@chromium.org",
                       "Triggered after a user uses managed browser where the "
                       "toolbar management button is visible.")));
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_COMPOSE)
  // kIPHComposeMSBBSettingsFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForToastPromo(
                    feature_engagement::kIPHComposeMSBBSettingsFeature,
                    kAnonymizedUrlCollectionPersonalizationSettingId,
                    IDS_COMPOSE_MSBB_IPH_BUBBLE_TEXT,
                    IDS_COMPOSE_MSBB_IPH_BUBBLE_TEXT_SCREENREADER,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetBubbleArrow(HelpBubbleArrow::kBottomRight)));
#endif  // BUILDFLAG(ENABLE_COMPOSE)
}

void MaybeRegisterChromeFeaturePromos(
    user_education::FeaturePromoRegistry& registry) {
  MaybeRegisterChromeFeaturePromos(registry, nullptr);
}

void MaybeRegisterChromeTutorials(
    user_education::TutorialRegistry& tutorial_registry) {
  using user_education::HelpBubbleArrow;
  using user_education::TutorialDescription;
  using BubbleStep = user_education::TutorialDescription::BubbleStep;
  using EventStep = user_education::TutorialDescription::EventStep;
  using HiddenStep = user_education::TutorialDescription::HiddenStep;

  // TODO (dfried): we might want to do something more sophisticated in the
  // future.
  if (tutorial_registry.IsTutorialRegistered(kTabGroupTutorialId)) {
    return;
  }

  {  // Tab Group tutorial.
    auto tab_group_tutorial = TutorialDescription::Create<
        kTabGroupTutorialMetricPrefix>(
        // The initial step. This is the only step that differs depending on
        // whether there is an existing group.
        IfView(kBrowserViewElementId, base::BindRepeating(&HasTabGroups))
            .Then(
                BubbleStep(kTabStripRegionElementId)
                    .SetBubbleBodyText(
                        IDS_TUTORIAL_ADD_TAB_TO_GROUP_WITH_EXISTING_GROUP_IN_TAB_STRIP))
            .Else(BubbleStep(kTabStripRegionElementId)
                      .SetBubbleBodyText(
                          IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)),

        // Getting the new tab group (hidden step).
        HiddenStep::WaitForShowEvent(kTabGroupHeaderElementId)
            .NameElement(kTabGroupHeaderElementName),

        // The menu step.
        BubbleStep(kTabGroupEditorBubbleId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE)
            .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
            .AbortIfVisibilityLost(false),

        HiddenStep::WaitForHidden(kTabGroupEditorBubbleId),

        // Drag tab into the group.
        BubbleStep(kTabStripRegionElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_DRAG_TAB),

        EventStep(kTabGroupedCustomEventId).AbortIfVisibilityLost(true),

        // Click to collapse the tab group.
        BubbleStep(kTabGroupHeaderElementName)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_COLLAPSE)
            .SetBubbleArrow(HelpBubbleArrow::kTopCenter),

        HiddenStep::WaitForActivated(kTabGroupHeaderElementId),

        // Completion of the tutorial.
        BubbleStep(kTabStripRegionElementId)
            .SetBubbleTitleText(IDS_TUTORIAL_GENERIC_SUCCESS_TITLE)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_SUCCESS_DESCRIPTION));

    tab_group_tutorial.metadata.additional_description =
        "Tutorial for creating new tab groups.";
    tab_group_tutorial.metadata.launch_milestone = 106;
    tab_group_tutorial.metadata.owners = "dpenning@chromium.org";

    tutorial_registry.AddTutorial(kTabGroupTutorialId,
                                  std::move(tab_group_tutorial));
  }

  if (!tab_groups::IsTabGroupsSaveV2Enabled()) {  // Saved Tab Group tutorial.
    auto saved_tab_group_tutorial =
        TutorialDescription::Create<kSavedTabGroupTutorialMetricPrefix>(
            IfView(kBrowserViewElementId, base::BindRepeating(&HasTabGroups))
                .Then(
                    // Point at the tab group header and say rick-click on
                    // group name to open the editor bubble.
                    BubbleStep(kTabGroupHeaderElementId)
                        .SetBubbleBodyText(
                            IDS_TUTORIAL_SAVED_TAB_GROUP_OPEN_EDITOR)
                        .SetBubbleArrow(HelpBubbleArrow::kTopCenter))
                .Else(
                    // Point at the tab strip and say right-click a tab and
                    // choose "Add tab to new group".
                    BubbleStep(kTabStripRegionElementId)
                        .SetBubbleBodyText(
                            IDS_TUTORIAL_SAVED_TAB_GROUP_ADD_TAB_TO_GROUP),

                    // Wait for the tab group to be created.
                    HiddenStep::WaitForShowEvent(kTabGroupHeaderElementId)),

            // Wait for the editor bubble to appear.
            HiddenStep::WaitForShowEvent(kTabGroupEditorBubbleId),

            // Point at editor bubble "Name your group, turn on save".
            BubbleStep(kTabGroupEditorBubbleSaveToggleId)
                .SetBubbleBodyText(IDS_TUTORIAL_SAVED_TAB_GROUP_NAME_SAVE_GROUP)
                .SetBubbleArrow(HelpBubbleArrow::kLeftCenter),

            // Wait for save group sync to be enabled.
            EventStep(kTabGroupSavedCustomEventId).AbortIfVisibilityLost(true),

            // Point at editor bubble "Hide group" to save it for later in the
            // bookmarks bar.
            BubbleStep(kTabGroupEditorBubbleCloseGroupButtonId)
                .SetBubbleBodyText(IDS_TUTORIAL_SAVED_TAB_GROUP_HIDE_GROUP)
                .SetBubbleArrow(HelpBubbleArrow::kLeftCenter),

            // Wait for the hide group to be pressed.
            HiddenStep::WaitForActivated(
                kTabGroupEditorBubbleCloseGroupButtonId),

            // Wait for the bookmarks bar to show.
            HiddenStep::WaitForShown(kBookmarkBarElementId),

            // Point at bookmark bar with message to open the previously
            // closed saved tab group.
            BubbleStep(kSavedTabGroupButtonElementId)
                .SetBubbleBodyText(IDS_TUTORIAL_SAVED_TAB_GROUP_REOPEN_GROUP)
                .SetBubbleArrow(HelpBubbleArrow::kTopLeft),

            // Wait for the saved tab groups button in bookmarks bar to be
            // activated.
            HiddenStep::WaitForActivated(kSavedTabGroupButtonElementId),

            // Wait for saved tabs groups to be reopened.
            HiddenStep::WaitForShowEvent(kTabGroupHeaderElementId)
                .NameElement(kTabGroupHeaderElementName),

            // Point at tab group header and show the success message for the
            // tutorial.
            BubbleStep(kTabGroupHeaderElementName)
                .SetBubbleTitleText(IDS_TUTORIAL_GENERIC_SUCCESS_TITLE)
                .SetBubbleBodyText(
                    IDS_TUTORIAL_SAVED_TAB_GROUP_SUCCESS_DESCRIPTION)
                .SetBubbleArrow(HelpBubbleArrow::kTopCenter));
    // Attach a temporary state callback to force show bookmarks bar
    // during the lifetime of the tutorial.
    saved_tab_group_tutorial.temporary_state_callback = base::BindRepeating(
        [](ui::ElementContext context)
            -> std::unique_ptr<user_education::ScopedTutorialState> {
          return base::WrapUnique(
              new ScopedSavedTabGroupTutorialState(context));
        });

    saved_tab_group_tutorial.metadata.additional_description =
        "Tutorial for saving tab groups.";
    saved_tab_group_tutorial.metadata.launch_milestone = 122;
    saved_tab_group_tutorial.metadata.owners = "dpenning@chromium.org";

    tutorial_registry.AddTutorial(kSavedTabGroupTutorialId,
                                  std::move(saved_tab_group_tutorial));
  }

  {  // Side panel customize chrome
    auto customize_chrome_tutorial = TutorialDescription::Create<
        kSidePanelCustomizeChromeTutorialMetricPrefix>(
        // Bubble step - customize chrome button
        BubbleStep(NewTabPageUI::kCustomizeChromeButtonElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_CUSTOMIZE_CHROME_OPEN_SIDE_PANEL)
            .SetBubbleArrow(HelpBubbleArrow::kBottomRight)
            .InAnyContext(),

        // Bubble step - change theme button
        BubbleStep(CustomizeChromeUI::kChangeChromeThemeButtonElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_CUSTOMIZE_CHROME_CHANGE_THEME)
            .SetBubbleArrow(HelpBubbleArrow::kRightCenter)
            .AbortIfVisibilityLost(false)
            .InAnyContext(),

        // Bubble step - select collection
        BubbleStep(CustomizeChromeUI::kChromeThemeCollectionElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_CUSTOMIZE_CHROME_SELECT_COLLECTION)
            .SetBubbleArrow(HelpBubbleArrow::kRightCenter)
            .AbortIfVisibilityLost(false)
            .InAnyContext(),

        // Bubble step - select theme
        BubbleStep(CustomizeChromeUI::kChromeThemeElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_CUSTOMIZE_CHROME_APPLY_THEME)
            .SetBubbleArrow(HelpBubbleArrow::kRightCenter)
            .AbortIfVisibilityLost(false)
            .InAnyContext(),

        // Event step - select theme event
        EventStep(kBrowserThemeChangedEventId, kBrowserViewElementId),

        // Bubble step - back button
        BubbleStep(CustomizeChromeUI::kChromeThemeBackElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_CUSTOMIZE_CHROME_CLICK_BACK_ARROW)
            .SetBubbleArrow(HelpBubbleArrow::kRightCenter)
            .NameElement(kChromeThemeBackElementName)
            .AbortIfVisibilityLost(false)
            .InAnyContext(),

        // Hidden step - back button
        HiddenStep::WaitForHidden(kChromeThemeBackElementName),

        // Completion of the tutorial.
        BubbleStep(NewTabPageUI::kCustomizeChromeButtonElementId)
            .SetBubbleTitleText(IDS_TUTORIAL_GENERIC_SUCCESS_TITLE)
            .SetBubbleArrow(HelpBubbleArrow::kBottomRight)
            .SetBubbleBodyText(IDS_TUTORIAL_CUSTOMIZE_CHROME_SUCCESS_BODY)
            .InAnyContext());

    customize_chrome_tutorial.metadata.additional_description =
        "Tutorial for customizing themes using side panel.";
    customize_chrome_tutorial.metadata.launch_milestone = 114;
    customize_chrome_tutorial.metadata.owners = "mickeyburks@chromium.org";

    tutorial_registry.AddTutorial(kSidePanelCustomizeChromeTutorialId,
                                  std::move(customize_chrome_tutorial));
  }

  {  // Password Manager tutorial
    auto password_manager_tutorial =
        TutorialDescription::Create<kPasswordManagerTutorialMetricPrefix>(
            // Bubble step - Browser app menu
            TutorialDescription::BubbleStep(kToolbarAppMenuButtonElementId)
                .SetBubbleBodyText(IDS_TUTORIAL_PASSWORD_MANAGER_OPEN_APP_MENU)
                .SetBubbleArrow(HelpBubbleArrow::kTopRight),

            // Wait for one of the next elements so the If step can check
            // for the optional element.
            TutorialDescription::WaitForAnyOf(
                AppMenuModel::kPasswordAndAutofillMenuItem)
                .Or(AppMenuModel::kPasswordManagerMenuItem),

            TutorialDescription::If(AppMenuModel::kPasswordAndAutofillMenuItem)
                .Then(
                    // Bubble step - Passwords and Autofill sub menu item
                    TutorialDescription::BubbleStep(
                        AppMenuModel::kPasswordAndAutofillMenuItem)
                        .SetBubbleBodyText(
                            IDS_TUTORIAL_PASSWORD_MANAGER_CLICK_PASSWORDS_MENU)
                        .SetBubbleArrow(HelpBubbleArrow::kRightCenter)),

            // Bubble step - "Password Manager" menu item
            TutorialDescription::BubbleStep(
                AppMenuModel::kPasswordManagerMenuItem)
                .SetBubbleBodyText(
                    IDS_TUTORIAL_PASSWORD_MANAGER_CLICK_PASSWORD_MANAGER)
                .SetBubbleArrow(HelpBubbleArrow::kRightCenter)
                .AbortIfVisibilityLost(false),

            // Bubble step - "Add shortcut" row
            TutorialDescription::BubbleStep(
                PasswordManagerUI::kAddShortcutElementId)
                .SetBubbleBodyText(IDS_TUTORIAL_PASSWORD_MANAGER_ADD_SHORTCUT)
                .SetBubbleArrow(HelpBubbleArrow::kTopCenter)
                .InAnyContext(),

            // Event step - Click on "Add shortcut"
            TutorialDescription::EventStep(
                PasswordManagerUI::kAddShortcutCustomEventId)
                .InSameContext(),

            // Bubble step - "Install" row
            TutorialDescription::BubbleStep(
                PWAConfirmationBubbleView::kInstallButton)
                .SetBubbleBodyText(IDS_TUTORIAL_PASSWORD_MANAGER_CLICK_INSTALL)
                .SetBubbleArrow(HelpBubbleArrow::kTopRight),

            // Event step - Click on "Add shortcut"
            TutorialDescription::EventStep(
                PWAConfirmationBubbleView::kInstalledPWAEventId)
                .InSameContext(),

            // Completion of the tutorial.
            TutorialDescription::BubbleStep(kTopContainerElementId)
                .SetBubbleTitleText(IDS_TUTORIAL_GENERIC_SUCCESS_TITLE)
                .SetBubbleBodyText(IDS_TUTORIAL_PASSWORD_MANAGER_SUCCESS_BODY)
                .SetBubbleArrow(HelpBubbleArrow::kNone));

    password_manager_tutorial.metadata.additional_description =
        "Tutorial for installing password manager.";
    password_manager_tutorial.metadata.launch_milestone = 116;
    password_manager_tutorial.metadata.owners = "mickeyburks@chromium.org";

    tutorial_registry.AddTutorial(kPasswordManagerTutorialId,
                                  std::move(password_manager_tutorial));
  }
}

// Note: If you add a badge here, be sure to add the name of the corresponding
// feature to tools/metrics/histograms/metadata/user_education/histograms.xml
void MaybeRegisterChromeNewBadges(user_education::NewBadgeRegistry& registry) {
  if (registry.IsFeatureRegistered(
          user_education::features::kNewBadgeTestFeature)) {
    return;
  }

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      user_education::features::kNewBadgeTestFeature,
      user_education::Metadata(124, "Frizzle Team",
                               "Used to test \"New\" Badge logic.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      compose::features::kEnableCompose,
      user_education::Metadata(124, "dewittj@chromium.org",
                               "Shown in Help Me Write context menu item.", {},
                               kComposePlatforms)));
  registry.RegisterFeature(user_education::NewBadgeSpecification(
      compose::features::kEnableComposeSavedStateNudge,
      user_education::Metadata(124, "dewittj@chromium.org",
                               "Shown in autofill-style suggestion UI to "
                               "resume an ongoing Compose session.",
                               {}, kComposePlatforms)));
  registry.RegisterFeature(user_education::NewBadgeSpecification(
      compose::features::kEnableComposeProactiveNudge,
      user_education::Metadata(126, "dewittj@chromium.org",
                               "Shown in autofill-style suggestion UI when "
                               "Compose proactive nudge is shown.",
                               {}, kComposePlatforms)));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      lens::features::kLensOverlay,
      user_education::Metadata(126, "jdonnelly@google.com, dfried@google.com",
                               "Shown in app and web context menus.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kTabOrganization,
      user_education::Metadata(
          126, "emshack@chromium.org",
          "Shown in app menu when TabOrganizationAppMenuItem is enabled.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      autofill::features::kAutofillForUnclassifiedFieldsAvailable,
      user_education::Metadata(
          125, "vidhanj@google.com",
          "Shown in the autofill section of the context menu for address and "
          "credit card autofill entries when autofill for unclassified fields "
          "is enabled.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      plus_addresses::features::kPlusAddressFallbackFromContextMenu,
      user_education::Metadata(
          128, "jkeitel@google.com",
          "Shown in the autofill section of the context menu where manual "
          "fallback for plus addresses is offered.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      plus_addresses::features::kPlusAddressesEnabled,
      user_education::Metadata(128, "jkeitel@google.com",
                               "Shown in the autofill popup for suggestions to "
                               "create a new plus address.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      password_manager::features::kPasswordManualFallbackAvailable,
      user_education::Metadata(
          128, "theocristea@google.com",
          "For passwords manual fallback; shown in the context menu.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kTabstripDeclutter,
      user_education::Metadata(
          132, "emshack@chromium.org",
          "Shown in app menu when Tab Declutter menu item is enabled.")));
}

std::unique_ptr<BrowserFeaturePromoController> CreateUserEducationResources(
    BrowserView* browser_view) {
  Profile* const profile = browser_view->GetProfile();

  // Get the user education service.
  if (!UserEducationServiceFactory::ProfileAllowsUserEducation(profile)) {
    return nullptr;
  }
  UserEducationService* const user_education_service =
      UserEducationServiceFactory::GetForBrowserContext(profile);
  if (!user_education_service) {
    return nullptr;
  }

  // Consider registering factories, etc.
  RegisterChromeHelpBubbleFactories(
      user_education_service->help_bubble_factory_registry());
  MaybeRegisterChromeFeaturePromos(
      user_education_service->feature_promo_registry(), profile);
  MaybeRegisterChromeTutorials(user_education_service->tutorial_registry());
  CHECK(user_education_service->new_badge_registry());

  MaybeRegisterChromeNewBadges(*user_education_service->new_badge_registry());
  user_education_service->new_badge_controller()->InitData();

  LowUsageHelpController::MaybeCreateForProfile(browser_view->GetProfile());

  return std::make_unique<BrowserFeaturePromoController>(
      browser_view,
      feature_engagement::TrackerFactory::GetForBrowserContext(profile),
      &user_education_service->feature_promo_registry(),
      &user_education_service->help_bubble_factory_registry(),
      &user_education_service->feature_promo_storage_service(),
      &user_education_service->feature_promo_session_policy(),
      &user_education_service->tutorial_service(),
      &user_education_service->product_messaging_controller());
}
