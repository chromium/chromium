// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/reading_list_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/show_promo_in_page.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_registry.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/user_education/views/help_bubble_factory_views_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#include "components/user_education/views/help_bubble_factory_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

const char kTabGroupTutorialMetricPrefix[] = "TabGroup";
const char kCustomizeChromeTutorialMetricPrefix[] = "CustomizeChromeSidePanel";
const char kSideSearchTutorialMetricPrefix[] = "SideSearch";
const char kPasswordManagerTutorialMetricPrefix[] = "PasswordManager";
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

bool HasTabGroups(const BrowserView* browser_view) {
  return !browser_view->browser()
              ->tab_strip_model()
              ->group_model()
              ->ListTabGroups()
              .empty();
}

}  // namespace

const char kSideSearchTutorialId[] = "Side Search Tutorial";

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
    user_education::FeaturePromoRegistry& registry) {
  using user_education::FeaturePromoSpecification;
  using user_education::HelpBubbleArrow;

  // This icon got updated, so select which is used based on whether refresh is
  // enabled. Note that the WebUI refresh state is not taken into account, so
  // this selection will affect both Views and WebUI help bubbles.
  const gfx::VectorIcon* const kLightbulbOutlineIcon =
      features::IsChromeRefresh2023()
          ? &vector_icons::kLightbulbOutlineChromeRefreshIcon
          : &vector_icons::kLightbulbOutlineIcon;

  // Verify that we haven't already registered the expected features.
  // TODO(dfried): figure out if we should do something more sophisticated here.
  if (registry.IsFeatureRegistered(
          feature_engagement::kIPHDesktopPwaInstallFeature))
    return;

  // TODO(1432894): Use toast or snooze instead of legacy promo.
  // kIPHAutofillExternalAccountProfileSuggestionFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &feature_engagement::
                        kIPHAutofillExternalAccountProfileSuggestionFeature,
                    kAutofillSuggestionElementId,
                    IDS_AUTOFILL_IPH_EXTERNAL_ACCOUNT_PROFILE_SUGGESTION)
                    .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)));

  // kIPHAutofillVirtualCardCVCSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature,
          kAutofillStandaloneCvcSuggestionElementId,
          IDS_AUTOFILL_VIRTUAL_CARD_STANDALONE_CVC_SUGGESTION_IPH_BUBBLE_LABEL,
          IDS_AUTOFILL_VIRTUAL_CARD_STANDALONE_CVC_SUGGESTION_IPH_BUBBLE_LABEL_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)));

  // kIPHAutofillVirtualCardSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature,
          kAutofillCreditCardSuggestionEntryElementId,
          IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_IPH_BUBBLE_LABEL)
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)));

  // kIPHDesktopPwaInstallFeature:
  registry.RegisterFeature(
      user_education::FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHDesktopPwaInstallFeature,
          kInstallPwaElementId, IDS_DESKTOP_PWA_INSTALL_PROMO));

  // kIPHDesktopTabGroupsNewGroupFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForTutorialPromo(
                    feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
                    kTabStripRegionElementId, IDS_TAB_GROUPS_NEW_GROUP_PROMO,
                    kTabGroupTutorialId)
                    .SetBubbleArrow(HelpBubbleArrow::kNone)
                    .SetBubbleIcon(kLightbulbOutlineIcon)));

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
          // See: crbug.com/1494923
          .OverrideFocusOnShow(false)));

  // kIPHDesktopNewTabPageModulesCustomizeFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHDesktopNewTabPageModulesCustomizeFeature,
          NewTabPageUI::kModulesCustomizeIPHAnchorElement,
          IDS_NTP_MODULES_CUSTOMIZE_IPH)
          .SetBubbleArrow(HelpBubbleArrow::kBottomRight)
          .SetBubbleIcon(&vector_icons::kLightbulbOutlineIcon)
          .SetInAnyContext(true)
          // See: crbug.com/1494923
          .OverrideFocusOnShow(false)));

  // IPH promo for experimental AI that shows two buttons.
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
          .SetCustomActionIsDefault(true)));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // kIPHExtensionsMenuFeature:
  registry.RegisterFeature(std::move(
      user_education::FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHExtensionsMenuFeature,
          kExtensionsMenuButtonElementId,
          IDS_EXTENSIONS_MENU_IPH_ENTRY_POINT_BODY)
          .SetBubbleTitleText(IDS_EXTENSIONS_MENU_IPH_ENTRY_POINT_TITLE)));

  // kIPHExtensionsRequestAccessButtonFeature
  registry.RegisterFeature(std::move(
      user_education::FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHExtensionsRequestAccessButtonFeature,
          kExtensionsRequestAccessButtonElementId,
          IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_IPH_ENTRY_POINT_BODY)
          .SetBubbleTitleText(
              IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_IPH_ENTRY_POINT_TITLE)));
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

  // kIPHPasswordsAccountStorageFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHPasswordsAccountStorageFeature,
          kSavePasswordComboboxElementId,
          IDS_PASSWORD_MANAGER_IPH_BODY_SAVE_TO_ACCOUNT)
          .SetBubbleTitleText(IDS_PASSWORD_MANAGER_IPH_TITLE_SAVE_TO_ACCOUNT)
          .SetBubbleArrow(HelpBubbleArrow::kRightCenter)));

  // kIPHPasswordsManagementBubbleAfterSaveFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHPasswordsManagementBubbleAfterSaveFeature,
      kPasswordsOmniboxKeyIconElementId,
      IDS_PASSWORD_MANAGER_IPH_MANAGEMENT_BUBBLE_AFTER_SAVE,
      IDS_PASSWORD_MANAGER_IPH_MANAGEMENT_BUBBLE_AFTER_SAVE_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo()));

  // kIPHPasswordsManagementBubbleDuringSigninFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHPasswordsManagementBubbleDuringSigninFeature,
      kPasswordsOmniboxKeyIconElementId,
      IDS_PASSWORD_MANAGER_IPH_MANAGEMENT_BUBBLE_DURING_SIGNIN,
      IDS_PASSWORD_MANAGER_IPH_MANAGEMENT_BUBBLE_DURING_SIGNIN_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo()));

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
  if (features::IsSidePanelPinningEnabled()) {
    registry.RegisterFeature(
        std::move(FeaturePromoSpecification::CreateForSnoozePromo(
                      feature_engagement::kIPHPowerBookmarksSidePanelFeature,
                      kToolbarAppMenuButtonElementId,
                      IDS_POWER_BOOKMARKS_SIDE_PANEL_PROMO_PINNING)
                      .SetHighlightedMenuItem(
                          BookmarkSubMenuModel::kShowBookmarkSidePanelItem)));
  } else {
    registry.RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
        feature_engagement::kIPHPowerBookmarksSidePanelFeature,
        kToolbarSidePanelButtonElementId,
        IDS_POWER_BOOKMARKS_SIDE_PANEL_PROMO));
  }

  // kIPHCompanionSidePanelFeature:
  if (features::IsSidePanelPinningEnabled()) {
    registry.RegisterFeature(std::move(
        FeaturePromoSpecification::CreateForToastPromo(
            feature_engagement::kIPHCompanionSidePanelFeature,
            kToolbarAppMenuButtonElementId,
            IDS_SIDE_PANEL_COMPANION_PROMO_PINNING,
            IDS_SIDE_PANEL_COMPANION_PROMO_SCREEN_READER,
            FeaturePromoSpecification::AcceleratorInfo())
            .SetHighlightedMenuItem(AppMenuModel::kShowSearchCompanion)));
  } else {
    registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
        feature_engagement::kIPHCompanionSidePanelFeature,
        kSidePanelCompanionToolbarButtonElementId,
        IDS_SIDE_PANEL_COMPANION_PROMO,
        IDS_SIDE_PANEL_COMPANION_PROMO_SCREEN_READER,
        FeaturePromoSpecification::AcceleratorInfo()));
  }

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
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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

  // kIPH3pcdUserBypassFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPH3pcdUserBypassFeature,
          kCookieControlsIconElementId, IDS_3PCD_USER_BYPASS_PROMO_TEXT,
          IDS_3PCD_USER_BYPASS_PROMO_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetBubbleTitleText(IDS_3PCD_USER_BYPASS_PROMO_TITLE)));

  // kIPHReadingListDiscoveryFeature:
  if (features::IsSidePanelPinningEnabled()) {
    registry.RegisterFeature(
        std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                      &feature_engagement::kIPHReadingListDiscoveryFeature,
                      kToolbarAppMenuButtonElementId,
                      IDS_READING_LIST_DISCOVERY_PROMO_PINNING)
                      .SetHighlightedMenuItem(
                          ReadingListSubMenuModel::kReadingListMenuShowUI)));
  } else {
    registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
        &feature_engagement::kIPHReadingListDiscoveryFeature,
        kToolbarSidePanelButtonElementId, IDS_READING_LIST_DISCOVERY_PROMO));
  }

  // kIPHReadingListEntryPointFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
      feature_engagement::kIPHReadingListEntryPointFeature,
      kBookmarkStarViewElementId, IDS_READING_LIST_ENTRY_POINT_PROMO));

  // kIPHReadingListInSidePanelFeature:
  if (features::IsSidePanelPinningEnabled()) {
    registry.RegisterFeature(
        std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                      &feature_engagement::kIPHReadingListInSidePanelFeature,
                      kToolbarAppMenuButtonElementId,
                      IDS_READING_LIST_IN_SIDE_PANEL_PROMO_PINNING)
                      .SetHighlightedMenuItem(
                          BookmarkSubMenuModel::kReadingListMenuItem)));
  } else {
    registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
        &feature_engagement::kIPHReadingListInSidePanelFeature,
        kToolbarSidePanelButtonElementId,
        IDS_READING_LIST_IN_SIDE_PANEL_PROMO));
  }

  // kIPHReadingModeSidePanelFeature:
  if (features::IsSidePanelPinningEnabled()) {
    registry.RegisterFeature(std::move(
        FeaturePromoSpecification::CreateForSnoozePromo(
            feature_engagement::kIPHReadingModeSidePanelFeature,
            kToolbarAppMenuButtonElementId,
            IDS_READING_MODE_SIDE_PANEL_PROMO_PINNING)
            .SetHighlightedMenuItem(ToolsMenuModel::kReadingModeMenuItem)));
  } else {
    registry.RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
        feature_engagement::kIPHReadingModeSidePanelFeature,
        kToolbarSidePanelButtonElementId, IDS_READING_MODE_SIDE_PANEL_PROMO));
  }

  if (features::IsSidePanelPinningEnabled()) {
    // kIPHSidePanelGenericMenuFeature:
    registry.RegisterFeature(std::move(
        FeaturePromoSpecification::CreateForToastPromo(
            feature_engagement::kIPHSidePanelGenericMenuFeature,
            kToolbarAppMenuButtonElementId, IDS_SIDE_PANEL_GENERIC_MENU_IPH,
            IDS_SIDE_PANEL_GENERIC_MENU_IPH_SCREENREADER,
            FeaturePromoSpecification::AcceleratorInfo())
            .SetBubbleArrow(HelpBubbleArrow::kTopRight)));

    // kIPHSidePanelGenericPinnableFeature:
    registry.RegisterFeature(std::move(
        FeaturePromoSpecification::CreateForToastPromo(
            feature_engagement::kIPHSidePanelGenericPinnableFeature,
            kSidePanelPinButtonElementId, IDS_SIDE_PANEL_GENERIC_PINNABLE_IPH,
            IDS_SIDE_PANEL_GENERIC_PINNABLE_IPH_SCREENREADER,
            FeaturePromoSpecification::AcceleratorInfo())
            .SetBubbleArrow(HelpBubbleArrow::kTopRight)));
  }

  // kIPHSideSearchFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForTutorialPromo(
          feature_engagement::kIPHSideSearchFeature, kSideSearchButtonElementId,
          IDS_SIDE_SEARCH_PROMO, kSideSearchTutorialId)
          .SetBubbleArrow(HelpBubbleArrow::kTopCenter)
          .SetBubbleIcon(kLightbulbOutlineIcon)));

  // kIPHTabOrganizationSuccessFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForToastPromo(
                    feature_engagement::kIPHTabOrganizationSuccessFeature,
                    kTabGroupHeaderElementId, IDS_TAB_ORGANIZATION_SUCCESS_IPH,
                    IDS_TAB_ORGANIZATION_SUCCESS_IPH_SCREENREADER,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetBubbleArrow(HelpBubbleArrow::kTopLeft)));

  // kIPHTabSearchFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHTabSearchFeature, kTabSearchButtonElementId,
      IDS_TAB_SEARCH_PROMO));

  // Tracking Protection Offboarding IPH
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHTrackingProtectionOffboardingFeature,
          kLocationIconElementId,
          IDS_TRACKING_PROTECTION_OFFBOARDING_NOTICE_BODY,
          IDS_TRACKING_PROTECTION_ONBOARDING_NOTICE_SETTINGS_BUTTON_LABEL,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                chrome::ShowSettingsSubPage(browser,
                                            chrome::kCookieSettingsSubPage);
              }))
          .SetBubbleTitleText(IDS_TRACKING_PROTECTION_OFFBOARDING_NOTICE_TITLE)
          .SetPromoSubtype(
              FeaturePromoSpecification::PromoSubtype::kLegalNotice)
          .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
          .SetCustomActionIsDefault(false)));

  // Tracking Protection Onboarding IPH
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHTrackingProtectionOnboardingFeature,
          kLocationIconElementId,
          IDS_TRACKING_PROTECTION_ONBOARDING_NOTICE_BODY,
          IDS_TRACKING_PROTECTION_ONBOARDING_NOTICE_SETTINGS_BUTTON_LABEL,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                chrome::ShowSettingsSubPage(browser,
                                            chrome::kCookieSettingsSubPage);
              }))
          .SetBubbleTitleText(IDS_TRACKING_PROTECTION_ONBOARDING_NOTICE_TITLE)
          .SetPromoSubtype(
              FeaturePromoSpecification::PromoSubtype::kLegalNotice)
          .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
          .SetBubbleIcon(&views::kEyeCrossedIcon)
          .SetCustomActionIsDefault(false)));

  // kIPHWebUITabStripFeature:
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHWebUITabStripFeature,
      kToolbarTabCounterButtonElementId, IDS_WEBUI_TAB_STRIP_PROMO));
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
          .SetBubbleIcon(&vector_icons::kCelebrationIcon)));

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
                if (browser)
                  chrome::ShowSettingsSubPage(browser,
                                              chrome::kPerformanceSubPage);
                RecordBatterySaverIPHOpenSettings(browser != nullptr);
              }))
          .SetBubbleTitleText(IDS_BATTERY_SAVER_MODE_PROMO_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)));

  // kIPHHighEfficiencyModeFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHHighEfficiencyModeFeature,
          kToolbarAppMenuButtonElementId, IDS_HIGH_EFFICIENCY_MODE_PROMO_TEXT,
          IDS_HIGH_EFFICIENCY_MODE_PROMO_ACTION_TEXT,
          base::BindRepeating(
              [](ui::ElementContext context,
                 user_education::FeaturePromoHandle promo_handle) {
                performance_manager::user_tuning::UserPerformanceTuningManager::
                    GetInstance()
                        ->SetHighEfficiencyModeEnabled(true);
                RecordHighEfficiencyIPHEnableMode(true);
              }))
          .SetCustomActionIsDefault(true)
          .SetCustomActionDismissText(IDS_NO_THANKS)
          .SetBubbleTitleText(IDS_HIGH_EFFICIENCY_MODE_PROMO_TITLE)
          .SetHighlightedMenuItem(ToolsMenuModel::kPerformanceMenuItem)));

  // kIPHPriceTrackingInSidePanelFeature;
  if (!features::IsSidePanelPinningEnabled()) {
    registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
        &feature_engagement::kIPHPriceTrackingInSidePanelFeature,
        kToolbarSidePanelButtonElementId, IDS_PRICE_TRACKING_SIDE_PANEL_IPH));
  }

  // kIPHDownloadToolbarButtonFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForToastPromo(
                    feature_engagement::kIPHDownloadToolbarButtonFeature,
                    kToolbarDownloadButtonElementId, IDS_DOWNLOAD_BUBBLE_PROMO,
                    IDS_DOWNLOAD_BUBBLE_PROMO_SCREENREADER,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetBubbleArrow(HelpBubbleArrow::kTopRight)
                    .SetBubbleTitleText(IDS_DOWNLOAD_BUBBLE_PROMO_TITLE)));

  // kIPHBackNavigationMenuFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForSnoozePromo(
                    feature_engagement::kIPHBackNavigationMenuFeature,
                    kToolbarBackButtonElementId, IDS_BACK_NAVIGATION_MENU_PROMO,
                    IDS_BACK_NAVIGATION_MENU_PROMO_ACCESSIBLE_TEXT,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetBubbleArrow(HelpBubbleArrow::kTopLeft)));

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
                auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                if (!browser) {
                  return;
                }
                TabStripModel* tab_strip_model = browser->tab_strip_model();
                if (tab_strip_model) {
                  content::WebContents* web_contents =
                      tab_strip_model->GetActiveWebContents();
                  GURL final_url;
                  const webapps::AppId* app_id =
                      web_app::WebAppTabHelper::GetAppId(web_contents);
                  CHECK(app_id);
                  final_url =
                      GURL(chrome::kChromeUIWebAppSettingsURL + *app_id);
                  if (web_contents &&
                      web_contents->GetURL() != browser->GetNewTabURL()) {
                    NavigateParams params(browser->profile(), final_url,
                                          ui::PAGE_TRANSITION_LINK);
                    params.disposition =
                        WindowOpenDisposition::NEW_FOREGROUND_TAB;
                    Navigate(&params);
                  }
                }
              }))
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetPromoSubtype(user_education::FeaturePromoSpecification::
                               PromoSubtype::kPerApp)));

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
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
  if (tutorial_registry.IsTutorialRegistered(kTabGroupTutorialId))
    return;

  {  // Menu item bubble test.
    TutorialDescription test_description;
    test_description.steps = {
        BubbleStep(kToolbarAppMenuButtonElementId)
            .SetBubbleBodyText(IDS_OK)
            .SetBubbleArrow(HelpBubbleArrow::kTopRight),
        BubbleStep(AppMenuModel::kDownloadsMenuItem)
            .SetBubbleBodyText(IDS_OK)
            .SetBubbleArrow(HelpBubbleArrow::kRightCenter),
        HiddenStep::WaitForHidden(AppMenuModel::kDownloadsMenuItem),
        BubbleStep(kTopContainerElementId).SetBubbleBodyText(IDS_OK)};
    tutorial_registry.AddTutorial("Menu item bubble test tutorial",
                                  std::move(test_description));
  }

  // Tab Group tutorial.
  tutorial_registry.AddTutorial(
      kTabGroupTutorialId,
      TutorialDescription::Create<kTabGroupTutorialMetricPrefix>(
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
              .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_SUCCESS_DESCRIPTION)));

  // Side panel customize chrome
  tutorial_registry.AddTutorial(
      kSidePanelCustomizeChromeTutorialId,
      TutorialDescription::Create<kCustomizeChromeTutorialMetricPrefix>(
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
              .SetBubbleBodyText(
                  IDS_TUTORIAL_CUSTOMIZE_CHROME_SELECT_COLLECTION)
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
              .InAnyContext()));

  {  // Side Search tutorial
    auto side_search_tutorial =
        TutorialDescription::Create<kSideSearchTutorialMetricPrefix>(
            // 1st bubble appears and prompts users to open side search
            BubbleStep(kSideSearchButtonElementId)
                .SetBubbleBodyText(IDS_SIDE_SEARCH_TUTORIAL_OPEN_SIDE_PANEL)
                .SetBubbleArrow(HelpBubbleArrow::kTopCenter),

            // 2nd bubble appears and prompts users to open a link
            BubbleStep(kSideSearchWebViewElementId)
                .SetBubbleBodyText(IDS_SIDE_SEARCH_TUTORIAL_OPEN_A_LINK_TO_TAB)
                .SetBubbleArrow(HelpBubbleArrow::kRightCenter),

            // Hidden step that detects a link is pressed
            EventStep(kSideSearchResultsClickedCustomEventId,
                      kSideSearchWebViewElementId),

            // 3rd bubble appears and prompts users to press close button
            BubbleStep(kSidePanelCloseButtonElementId)
                .SetBubbleBodyText(IDS_SIDE_SEARCH_TUTORIAL_CLOSE_SIDE_PANEL)
                .SetBubbleArrow(HelpBubbleArrow::kTopRight),

            // Completion of the tutorial.
            BubbleStep(kSideSearchButtonElementId)
                .SetBubbleTitleText(IDS_TUTORIAL_GENERIC_SUCCESS_TITLE)
                .SetBubbleBodyText(IDS_SIDE_SEARCH_PROMO)
                .SetBubbleArrow(HelpBubbleArrow::kTopRight));
    side_search_tutorial.can_be_restarted = true;
    tutorial_registry.AddTutorial(kSideSearchTutorialId,
                                  std::move(side_search_tutorial));
  }

  // Password Manager tutorial
  tutorial_registry.AddTutorial(
      kPasswordManagerTutorialId,
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
              .SetBubbleArrow(HelpBubbleArrow::kNone)));
}
