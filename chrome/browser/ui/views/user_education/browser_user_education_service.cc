// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/user_education/user_education_service_factory.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
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
#include "components/user_education/webui/floating_webui_help_bubble_factory.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "components/user_education/webui/tracked_element_webui.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/color/color_id.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/user_education/views/help_bubble_factory_views_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#include "components/user_education/views/help_bubble_factory_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

const char kTabGroupTutorialMetricPrefix[] = "TabGroup";
const char kTabGroupWithGroupTutorialMetricPrefix[] = "TabGroupWithGroup";
const char kSidePanelReadingListTutorialMetricPrefix[] = "SidePanelReadingList";
const char kCustomizeChromeTutorialMetricPrefix[] = "CustomizeChromeSidePanel";
const char kSideSearchTutorialMetricPrefix[] = "SideSearch";
const char kPasswordManagerTutorialMetricPrefix[] = "PasswordManager";
constexpr char kTabGroupHeaderElementName[] = "TabGroupHeader";
constexpr char kReadingListItemElementName[] = "ReadingListItem";
constexpr char kChromeThemeBackElementName[] = "ChromeThemeBackElement";

class BrowserHelpBubbleDelegate : public user_education::HelpBubbleDelegate {
 public:
  BrowserHelpBubbleDelegate() = default;
  ~BrowserHelpBubbleDelegate() override = default;

  std::vector<ui::Accelerator> GetPaneNavigationAccelerators(
      ui::TrackedElement* anchor_element) const override {
    std::vector<ui::Accelerator> result;
    if (anchor_element->IsA<views::TrackedElementViews>()) {
      auto* widget = anchor_element->AsA<views::TrackedElementViews>()
                         ->view()
                         ->GetWidget();
      if (widget) {
        auto* const client_view =
            widget->GetPrimaryWindowWidget()->client_view();
        if (client_view && views::IsViewClass<BrowserView>(client_view)) {
          auto* const browser_view = static_cast<BrowserView*>(client_view);
          ui::Accelerator accel;
          if (browser_view->GetAccelerator(IDC_FOCUS_NEXT_PANE, &accel))
            result.push_back(accel);
          if (browser_view->GetAccelerator(IDC_FOCUS_PREVIOUS_PANE, &accel))
            result.push_back(accel);
          if (browser_view->GetAccelerator(
                  IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY, &accel)) {
            result.push_back(accel);
          }
        }
      }
    }
    return result;
  }

  int GetTitleTextContext() const override {
    return ChromeTextContext::CONTEXT_IPH_BUBBLE_TITLE;
  }
  int GetBodyTextContext() const override {
    return ChromeTextContext::CONTEXT_IPH_BUBBLE_BODY;
  }

  // These methods return color codes that will be handled by the app's theming
  // system.
  ui::ColorId GetHelpBubbleBackgroundColorId() const override {
    return kColorFeaturePromoBubbleBackground;
  }
  ui::ColorId GetHelpBubbleForegroundColorId() const override {
    return kColorFeaturePromoBubbleForeground;
  }
  ui::ColorId GetHelpBubbleDefaultButtonBackgroundColorId() const override {
    return kColorFeaturePromoBubbleDefaultButtonBackground;
  }
  ui::ColorId GetHelpBubbleDefaultButtonForegroundColorId() const override {
    return kColorFeaturePromoBubbleDefaultButtonForeground;
  }
  ui::ColorId GetHelpBubbleButtonBorderColorId() const override {
    return kColorFeaturePromoBubbleButtonBorder;
  }
  ui::ColorId GetHelpBubbleCloseButtonInkDropColorId() const override {
    return kColorFeaturePromoBubbleCloseButtonInkDrop;
  }
};

// Help bubble factory that can show a floating (Views-based) help bubble on a
// WebUI element, but only for non-tab WebUI.
class FloatingWebUIHelpBubbleFactoryBrowser
    : public user_education::FloatingWebUIHelpBubbleFactory {
 public:
  explicit FloatingWebUIHelpBubbleFactoryBrowser(
      const user_education::HelpBubbleDelegate* delegate)
      : FloatingWebUIHelpBubbleFactory(delegate) {}
  ~FloatingWebUIHelpBubbleFactoryBrowser() override = default;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // HelpBubbleFactoryWebUIViews:
  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override {
    if (!element->IsA<user_education::TrackedElementWebUI>()) {
      return false;
    }

    // If this is a WebUI in a tab, then don't use this factory.
    const auto* contents = element->AsA<user_education::TrackedElementWebUI>()
                               ->handler()
                               ->GetWebContents();
    // Note: this checks all tabs for their WebContents.
    if (chrome::FindBrowserWithWebContents(contents)) {
      return false;
    }

    // Ensure that this WebUI fulfils the requirements for a floating help
    // bubble.
    return FloatingWebUIHelpBubbleFactory::CanBuildBubbleForTrackedElement(
        element);
  }
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(FloatingWebUIHelpBubbleFactoryBrowser)

}  // namespace

const char kSidePanelReadingListTutorialId[] =
    "Side Panel Reading List Tutorial";
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
  registry.MaybeRegister<user_education::HelpBubbleFactoryWebUI>();
#if BUILDFLAG(IS_MAC)
  registry.MaybeRegister<user_education::HelpBubbleFactoryMac>(delegate);
#endif
}

void MaybeRegisterChromeFeaturePromos(
    user_education::FeaturePromoRegistry& registry) {
  using user_education::FeaturePromoSpecification;
  using user_education::HelpBubbleArrow;

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
                    .SetBubbleIcon(&vector_icons::kLightbulbOutlineIcon)));

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
                auto* service = UserEducationServiceFactory::GetForProfile(
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
          .SetBubbleIcon(&vector_icons::kLightbulbOutlineIcon)
          .SetCustomActionIsDefault(true)
          .SetCustomActionDismissText(IDS_PROMO_SNOOZE_BUTTON)));

  // kIPHLiveCaptionFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHLiveCaptionFeature, kMediaButtonElementId,
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
      &feature_engagement::kIPHGMCCastStartStopFeature, kMediaButtonElementId,
      IDS_GLOBAL_MEDIA_CONTROLS_CONTROL_CAST_SESSIONS_PROMO));

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
          .SetBubbleIcon(&vector_icons::kLightbulbOutlineIcon)
          .SetBubbleTitleText(IDS_PASSWORD_MANAGER_IPH_CREATE_SHORTCUT_TITLE)));

  // kIPHPowerBookmarksSidePanelFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
      feature_engagement::kIPHPowerBookmarksSidePanelFeature,
      kSidePanelButtonElementId, IDS_POWER_BOOKMARKS_SIDE_PANEL_PROMO));

  // kIPHCompanionSidePanelFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
      feature_engagement::kIPHCompanionSidePanelFeature,
      kSidePanelCompanionToolbarButtonElementId,
      IDS_SIDE_PANEL_COMPANION_PROMO));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // kIPHSwitchProfileFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHProfileSwitchFeature, kAvatarButtonElementId,
      IDS_PROFILE_SWITCH_PROMO, IDS_PROFILE_SWITCH_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo(IDC_SHOW_AVATAR_MENU)));

  // kIPHPasswordsWebAppProfileSwitchFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature,
      kAvatarButtonElementId,
      IDS_PASSWORD_MANAGER_IPH_BODY_WEB_APP_PROFILE_SWITCH,
      IDS_PROFILE_SWITCH_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo()));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // kIPHReadingListDiscoveryFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHReadingListDiscoveryFeature,
      kSidePanelButtonElementId, IDS_READING_LIST_DISCOVERY_PROMO));

  // kIPHReadingListEntryPointFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
      feature_engagement::kIPHReadingListEntryPointFeature,
      kBookmarkStarViewElementId, IDS_READING_LIST_ENTRY_POINT_PROMO));

  // kIPHReadingListInSidePanelFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHReadingListInSidePanelFeature,
      kSidePanelButtonElementId, IDS_READING_LIST_IN_SIDE_PANEL_PROMO));

  // kIPHReadingModeSidePanelFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
      feature_engagement::kIPHReadingModeSidePanelFeature,
      kSidePanelButtonElementId, IDS_READING_MODE_SIDE_PANEL_PROMO));

  // kIPHSideSearchFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForTutorialPromo(
          feature_engagement::kIPHSideSearchFeature, kSideSearchButtonElementId,
          IDS_SIDE_SEARCH_PROMO, kSideSearchTutorialId)
          .SetBubbleArrow(HelpBubbleArrow::kBottomCenter)
          .SetBubbleIcon(&vector_icons::kLightbulbOutlineIcon)));

  // kIPHTabSearchFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHTabSearchFeature, kTabSearchButtonElementId,
      IDS_TAB_SEARCH_PROMO));

  // kIPHWebUITabStripFeature:
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHWebUITabStripFeature, kTabCounterButtonElementId,
      IDS_WEBUI_TAB_STRIP_PROMO));
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
          kBatterySaverButtonElementId, IDS_BATTERY_SAVER_MODE_PROMO_TEXT,
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
          kAppMenuButtonElementId, IDS_HIGH_EFFICIENCY_MODE_PROMO_TEXT,
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
          .SetBubbleTitleText(IDS_HIGH_EFFICIENCY_MODE_PROMO_TITLE)));

  // kIPHPriceTrackingInSidePanelFeature;
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHPriceTrackingInSidePanelFeature,
      kSidePanelButtonElementId, IDS_PRICE_TRACKING_SIDE_PANEL_IPH));

  // kIPHDownloadToolbarButtonFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForToastPromo(
                    feature_engagement::kIPHDownloadToolbarButtonFeature,
                    kDownloadToolbarButtonElementId, IDS_DOWNLOAD_BUBBLE_PROMO,
                    IDS_DOWNLOAD_BUBBLE_PROMO_SCREENREADER,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetBubbleArrow(HelpBubbleArrow::kTopRight)
                    .SetBubbleTitleText(IDS_DOWNLOAD_BUBBLE_PROMO_TITLE)));

  // kIPHBackNavigationMenuFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForSnoozePromo(
                    feature_engagement::kIPHBackNavigationMenuFeature,
                    kBackButtonElementId, IDS_BACK_NAVIGATION_MENU_PROMO,
                    IDS_BACK_NAVIGATION_MENU_PROMO_ACCESSIBLE_TEXT,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetBubbleArrow(HelpBubbleArrow::kTopLeft)));

  // kIPHPriceTrackingChipFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHPriceTrackingChipFeature,
      kPriceTrackingChipElementId, IDS_PRICE_TRACKING_CHIP_IPH));
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
        BubbleStep(kAppMenuButtonElementId)
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

  {  // Tab Group Tutorials

    // All but the first step is are same.
    auto common_steps = TutorialDescription::Steps(
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

    // The initial step is the only step that differs between
    // kTabGroupTutorialId and kTabGroupWithExistingGroupTutorialId.

    tutorial_registry.AddTutorial(
        kTabGroupTutorialId,
        TutorialDescription::Create<kTabGroupTutorialMetricPrefix>(
            BubbleStep(kTabStripRegionElementId)
                .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP),
            common_steps));

    tutorial_registry.AddTutorial(
        kTabGroupWithExistingGroupTutorialId,
        TutorialDescription::Create<kTabGroupWithGroupTutorialMetricPrefix>(
            BubbleStep(kTabStripRegionElementId)
                .SetBubbleBodyText(
                    IDS_TUTORIAL_ADD_TAB_TO_GROUP_WITH_EXISTING_GROUP_IN_TAB_STRIP),
            common_steps));
  }

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

  // Side panel reading list tutorial
  tutorial_registry.AddTutorial(
      kSidePanelReadingListTutorialId,
      TutorialDescription::Create<kSidePanelReadingListTutorialMetricPrefix>(

          // Open side panel
          BubbleStep(kSidePanelButtonElementId)
              .SetBubbleBodyText(
                  IDS_TUTORIAL_SIDE_PANEL_READING_LIST_OPEN_SIDE_PANEL)
              .SetBubbleArrow(HelpBubbleArrow::kTopRight),

          // Click "Add current tab"
          BubbleStep(kAddCurrentTabToReadingListElementId)
              .SetBubbleBodyText(IDS_TUTORIAL_SIDE_PANEL_READING_LIST_ADD_TAB)
              .SetBubbleArrow(HelpBubbleArrow::kRightTop)
              .InAnyContext(),

          // When shown, name the element
          HiddenStep::WaitForShowEvent(kSidePanelReadingListUnreadElementId)
              .InAnyContext()
              .NameElement(kReadingListItemElementName),

          // Mark as read
          BubbleStep(kReadingListItemElementName)
              .SetBubbleBodyText(IDS_TUTORIAL_SIDE_PANEL_READING_LIST_MARK_READ)
              .SetBubbleArrow(HelpBubbleArrow::kRightTop),

          EventStep(kSidePanelReadingMarkedAsReadEventId,
                    kReadingListItemElementName),

          // Click drop down
          BubbleStep(kSidePanelComboboxElementId)
              .SetBubbleBodyText(
                  IDS_TUTORIAL_SIDE_PANEL_READING_LIST_CLICK_DROPDOWN)
              .SetBubbleArrow(HelpBubbleArrow::kTopLeft),

          EventStep(kSidePanelComboboxChangedCustomEventId,
                    kSidePanelComboboxElementId),

          // Completion of the tutorial.
          BubbleStep(kTabStripRegionElementId)
              .SetBubbleTitleText(IDS_TUTORIAL_GENERIC_SUCCESS_TITLE)
              .SetBubbleBodyText(
                  IDS_TUTORIAL_SIDE_PANEL_READING_LIST_SUCCESS_BODY)));

  {  // Side Search tutorial
    auto side_search_tutorial =
        TutorialDescription::Create<kSideSearchTutorialMetricPrefix>(
            // 1st bubble appears and prompts users to open side search
            BubbleStep(kSideSearchButtonElementId)
                .SetBubbleBodyText(IDS_SIDE_SEARCH_TUTORIAL_OPEN_SIDE_PANEL)
                .SetBubbleArrow(HelpBubbleArrow::kBottomCenter),

            // 2nd bubble appears and prompts users to open a link
            BubbleStep(kSideSearchWebViewElementId)
                .SetBubbleBodyText(IDS_SIDE_SEARCH_TUTORIAL_OPEN_A_LINK_TO_TAB)
                .SetBubbleArrow(HelpBubbleArrow::kLeftCenter),

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
          TutorialDescription::BubbleStep(kAppMenuButtonElementId)
              .SetBubbleBodyText(IDS_TUTORIAL_PASSWORD_MANAGER_OPEN_APP_MENU)
              .SetBubbleArrow(HelpBubbleArrow::kTopRight),

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
