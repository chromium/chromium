// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"

#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
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

#if BUILDFLAG(IS_MAC)
#include "components/user_education/views/help_bubble_factory_mac.h"
#endif

namespace {

const char kTabGroupTutorialMetricPrefix[] = "TabGroup";
const char kTabGroupWithGroupTutorialMetricPrefix[] = "TabGroupWithGroup";
const char kSidePanelReadingListTutorialMetricPrefix[] = "SidePanelReadingList";
const char kCustomizeChromeTutorialMetricPrefix[] = "CustomizeChromeSidePanel";
const char kSideSearchTutorialMetricPrefix[] = "SideSearch";
constexpr char kTabGroupHeaderElementName[] = "TabGroupHeader";
constexpr char kReadingListItemElementName[] = "ReadingListItem";

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

const char kSidePanelCustomizeChromeTutorialId[] =
    "Side Panel Customize Chrome Tutorial";
const char kTabGroupTutorialId[] = "Tab Group Tutorial";
const char kTabGroupWithExistingGroupTutorialId[] =
    "Tab Group With Existing Group Tutorial";
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
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForTutorialPromo(
                    feature_engagement::kIPHDesktopCustomizeChromeFeature,
                    kTopContainerElementId,
                    IDS_TUTORIAL_CUSTOMIZE_CHROME_START_TUTORIAL_IPH,
                    kSidePanelCustomizeChromeTutorialId)
                    .SetBubbleArrow(HelpBubbleArrow::kNone)
                    .SetBubbleIcon(&vector_icons::kLightbulbOutlineIcon)));

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

  // kIPHPasswordsWebAppProfileSwitchFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForSnoozePromo(
                    feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature,
                    kAvatarButtonElementId,
                    IDS_PASSWORD_MANAGER_IPH_BODY_WEB_APP_PROFILE_SWITCH)
                    .SetBubbleIcon(&vector_icons::kLightbulbOutlineIcon)));

  // kIPHPowerBookmarksSidePanelFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
      feature_engagement::kIPHPowerBookmarksSidePanelFeature,
      kSidePanelButtonElementId, IDS_POWER_BOOKMARKS_SIDE_PANEL_PROMO));

  // kIPHSwitchProfileFeature:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHProfileSwitchFeature, kAvatarButtonElementId,
      IDS_PROFILE_SWITCH_PROMO, IDS_PROFILE_SWITCH_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo(IDC_SHOW_AVATAR_MENU)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // kIPHReadingListDiscoveryFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHReadingListDiscoveryFeature,
      kSidePanelButtonElementId, IDS_READING_LIST_DISCOVERY_PROMO));

  // kIPHReadingListEntryPointFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
      feature_engagement::kIPHReadingListEntryPointFeature,
      kBookmarkStarViewElementId, IDS_READING_LIST_ENTRY_POINT_PROMO));

  // kIPHIntentChipFeature
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHIntentChipFeature, kIntentChipElementId,
#if BUILDFLAG(IS_CHROMEOS)
      IDS_INTENT_CHIP_IPH_CHROME_OS
#else
      IDS_INTENT_CHIP_IPH_GENERIC
#endif
      ));

  // kIPHReadingListInSidePanelFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHReadingListInSidePanelFeature,
      kSidePanelButtonElementId, IDS_READING_LIST_IN_SIDE_PANEL_PROMO));

  // kIPHReopenTabFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHReopenTabFeature, kAppMenuButtonElementId,
      IDS_REOPEN_TAB_PROMO, IDS_REOPEN_TAB_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo(IDC_RESTORE_TAB)));

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

  // kIPHHighEfficiencyInfoModeFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHHighEfficiencyInfoModeFeature,
          kHighEfficiencyChipElementId,
          IDS_HIGH_EFFICIENCY_INFO_MODE_PROMO_TEXT,
          IDS_HIGH_EFFICIENCY_INFO_MODE_PROMO_ACTION_TEXT,
          base::BindRepeating(
              [](ui::ElementContext ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = chrome::FindBrowserWithUiElementContext(ctx);
                if (browser)
                  chrome::ShowSettingsSubPage(browser,
                                              chrome::kPerformanceSubPage);
                RecordHighEfficiencyInfoIPHOpenSettings(browser != nullptr);
              }))
          .SetBubbleTitleText(IDS_HIGH_EFFICIENCY_INFO_MODE_PROMO_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopCenter)));

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
          .SetCustomActionDismissText(IDS_NOT_NOW)
          .SetBubbleTitleText(IDS_HIGH_EFFICIENCY_MODE_PROMO_TITLE)));

  // kIPHPriceTrackingInSidePanelFeature;
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHPriceTrackingInSidePanelFeature,
      kSidePanelButtonElementId, IDS_PRICE_TRACKING_SIDE_PANEL_IPH));

  // kIPHDownloadToolbarButtonFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForSnoozePromo(
                    feature_engagement::kIPHDownloadToolbarButtonFeature,
                    kDownloadToolbarButtonElementId, IDS_DOWNLOAD_BUBBLE_PROMO)
                    .SetBubbleArrow(HelpBubbleArrow::kTopRight)
                    .SetBubbleTitleText(IDS_DOWNLOAD_BUBBLE_PROMO_TITLE)));
}

void MaybeRegisterChromeTutorials(
    user_education::TutorialRegistry& tutorial_registry) {
  using user_education::HelpBubbleArrow;
  using user_education::TutorialDescription;

  // TODO (dfried): we might want to do something more sophisticated in the
  // future.
  if (tutorial_registry.IsTutorialRegistered(kTabGroupTutorialId))
    return;

  {  // Tab Group Tutorials

    // The Description for kTabGroupTutorialId.
    TutorialDescription without_group_description;

    // The Description for kTabGroupWithExistingGroupTutorialId.
    TutorialDescription with_group_description;

    // The initial step. This is the only step that differs between
    // kTabGroupTutorialId and kTabGroupWithExistingGroupTutorialId.
    TutorialDescription::Step create_tabgroup_step(
        0, IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
        ui::InteractionSequence::StepType::kShown, kTabStripRegionElementId,
        std::string(), HelpBubbleArrow::kNone);
    without_group_description.steps.emplace_back(create_tabgroup_step);

    TutorialDescription::Step create_tabgroup_with_existing_group_step(
        0, IDS_TUTORIAL_ADD_TAB_TO_GROUP_WITH_EXISTING_GROUP_IN_TAB_STRIP,
        ui::InteractionSequence::StepType::kShown, kTabStripRegionElementId,
        std::string(), HelpBubbleArrow::kNone);
    with_group_description.steps.emplace_back(
        create_tabgroup_with_existing_group_step);

    // Getting the new tab group (hidden step).
    TutorialDescription::Step new_tab_group_step(
        0, 0, ui::InteractionSequence::StepType::kShown,
        kTabGroupHeaderElementId, std::string(), HelpBubbleArrow::kNone,
        ui::CustomElementEventType(), /* must_remain_visible =*/true,
        /* transition_only_on_event =*/true,
        base::BindRepeating(
            [](ui::InteractionSequence* sequence, ui::TrackedElement* element) {
              sequence->NameElement(
                  element, base::StringPiece(kTabGroupHeaderElementName));
              return true;
            }));
    without_group_description.steps.emplace_back(new_tab_group_step);
    with_group_description.steps.emplace_back(new_tab_group_step);

    // The menu step.
    TutorialDescription::Step bubble_menu_edit_step(
        0, IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE,
        ui::InteractionSequence::StepType::kShown, kTabGroupEditorBubbleId,
        std::string(), HelpBubbleArrow::kLeftCenter,
        ui::CustomElementEventType(),
        /*must_remain_visible =*/false);
    without_group_description.steps.emplace_back(bubble_menu_edit_step);
    with_group_description.steps.emplace_back(bubble_menu_edit_step);

    TutorialDescription::Step bubble_menu_edit_ended_step(
        0, 0, ui::InteractionSequence::StepType::kHidden,
        kTabGroupEditorBubbleId, std::string(), HelpBubbleArrow::kNone,
        ui::CustomElementEventType(),
        /*must_remain_visible =*/false);
    without_group_description.steps.emplace_back(bubble_menu_edit_ended_step);
    with_group_description.steps.emplace_back(bubble_menu_edit_ended_step);

    // Drag tab into the group.
    TutorialDescription::Step drag_tab_into_group_step(
        0, IDS_TUTORIAL_TAB_GROUP_DRAG_TAB,
        ui::InteractionSequence::StepType::kShown, kTabStripRegionElementId,
        std::string(), HelpBubbleArrow::kNone);
    without_group_description.steps.emplace_back(drag_tab_into_group_step);
    with_group_description.steps.emplace_back(drag_tab_into_group_step);

    TutorialDescription::Step successfully_drag_tab_into_group_step(
        0, 0, ui::InteractionSequence::StepType::kCustomEvent,
        ui::ElementIdentifier(), std::string(), HelpBubbleArrow::kTopCenter,
        kTabGroupedCustomEventId, /*must_remain_visible =*/true);
    without_group_description.steps.emplace_back(
        successfully_drag_tab_into_group_step);
    with_group_description.steps.emplace_back(
        successfully_drag_tab_into_group_step);

    // Click to collapse the tab group.
    TutorialDescription::Step collapse_step(
        0, IDS_TUTORIAL_TAB_GROUP_COLLAPSE,
        ui::InteractionSequence::StepType::kShown, ui::ElementIdentifier(),
        kTabGroupHeaderElementName, HelpBubbleArrow::kTopCenter);
    without_group_description.steps.emplace_back(collapse_step);
    with_group_description.steps.emplace_back(collapse_step);

    TutorialDescription::Step detect_collapse_step(
        0, 0, ui::InteractionSequence::StepType::kActivated,
        kTabGroupHeaderElementId, std::string(), HelpBubbleArrow::kNone);
    without_group_description.steps.emplace_back(detect_collapse_step);
    with_group_description.steps.emplace_back(detect_collapse_step);

    // Completion of the tutorial.
    TutorialDescription::Step success_step(
        IDS_TUTORIAL_GENERIC_SUCCESS_TITLE,
        IDS_TUTORIAL_TAB_GROUP_SUCCESS_DESCRIPTION,
        ui::InteractionSequence::StepType::kShown, kTabStripRegionElementId,
        std::string(), HelpBubbleArrow::kNone);
    without_group_description.steps.emplace_back(success_step);
    with_group_description.steps.emplace_back(success_step);

    without_group_description.histograms =
        user_education::MakeTutorialHistograms<kTabGroupTutorialMetricPrefix>(
            without_group_description.steps.size());
    tutorial_registry.AddTutorial(kTabGroupTutorialId,
                                  std::move(without_group_description));

    with_group_description.histograms = user_education::MakeTutorialHistograms<
        kTabGroupWithGroupTutorialMetricPrefix>(
        with_group_description.steps.size());
    tutorial_registry.AddTutorial(kTabGroupWithExistingGroupTutorialId,
                                  std::move(with_group_description));
  }

  {  // Side panel customize chrome

    // The Description for kSidePanelCustomizeChromeTutorialId
    TutorialDescription customize_chrome_description;

    // Bubble step - customize chrome button
    TutorialDescription::Step open_customize_chrome_step(
        0, IDS_TUTORIAL_CUSTOMIZE_CHROME_OPEN_SIDE_PANEL,
        ui::InteractionSequence::StepType::kShown,
        NewTabPageUI::kCustomizeChromeButtonElementId, std::string(),
        HelpBubbleArrow::kBottomRight, ui::CustomElementEventType(),
        absl::nullopt,
        /* transition_only_on_event =*/false,
        user_education::TutorialDescription::NameElementsCallback(),
        TutorialDescription::ContextMode::kAny);
    customize_chrome_description.steps.emplace_back(open_customize_chrome_step);

    // Bubble step - change theme button
    TutorialDescription::Step change_chrome_theme_step(
        0, IDS_TUTORIAL_CUSTOMIZE_CHROME_CHANGE_THEME,
        ui::InteractionSequence::StepType::kShown,
        CustomizeChromeUI::kChangeChromeThemeButtonElementId, std::string(),
        HelpBubbleArrow::kRightCenter, ui::CustomElementEventType(),
        /* must_remain_visible =*/false,
        /* transition_only_on_event =*/false,
        user_education::TutorialDescription::NameElementsCallback(),
        TutorialDescription::ContextMode::kAny);
    customize_chrome_description.steps.emplace_back(change_chrome_theme_step);

    // Bubble step - select collection
    TutorialDescription::Step select_collection_step(
        0, IDS_TUTORIAL_CUSTOMIZE_CHROME_SELECT_COLLECTION,
        ui::InteractionSequence::StepType::kShown,
        CustomizeChromeUI::kChromeThemeCollectionElementId, std::string(),
        HelpBubbleArrow::kRightCenter, ui::CustomElementEventType(),
        /* must_remain_visible =*/false,
        /* transition_only_on_event =*/false,
        user_education::TutorialDescription::NameElementsCallback(),
        TutorialDescription::ContextMode::kAny);
    customize_chrome_description.steps.emplace_back(select_collection_step);

    // Bubble step - select theme
    TutorialDescription::Step select_theme_step(
        0, IDS_TUTORIAL_CUSTOMIZE_CHROME_APPLY_THEME,
        ui::InteractionSequence::StepType::kShown,
        CustomizeChromeUI::kChromeThemeElementId, std::string(),
        HelpBubbleArrow::kRightCenter, ui::CustomElementEventType(),
        /* must_remain_visible =*/false,
        /* transition_only_on_event =*/false,
        user_education::TutorialDescription::NameElementsCallback(),
        TutorialDescription::ContextMode::kAny);
    customize_chrome_description.steps.emplace_back(select_theme_step);

    // Event step - select theme event
    TutorialDescription::Step select_theme_event_step(
        0, 0, ui::InteractionSequence::StepType::kCustomEvent,
        kBrowserViewElementId, std::string(), HelpBubbleArrow::kNone,
        kBrowserThemeChangedEventId,
        /* must_remain_visible =*/false);
    select_theme_event_step.must_be_visible = false;
    customize_chrome_description.steps.emplace_back(select_theme_event_step);

    // Bubble step - back button
    TutorialDescription::Step back_button_step(
        0, IDS_TUTORIAL_CUSTOMIZE_CHROME_CLICK_BACK_ARROW,
        ui::InteractionSequence::StepType::kShown,
        CustomizeChromeUI::kChromeThemeBackElementId, std::string(),
        HelpBubbleArrow::kRightCenter, ui::CustomElementEventType(),
        /* must_remain_visible =*/false,
        /* transition_only_on_event =*/false,
        user_education::TutorialDescription::NameElementsCallback(),
        TutorialDescription::ContextMode::kAny);
    customize_chrome_description.steps.emplace_back(back_button_step);

    // Completion of the tutorial.
    TutorialDescription::Step success_step(
        IDS_TUTORIAL_GENERIC_SUCCESS_TITLE,
        IDS_TUTORIAL_CUSTOMIZE_CHROME_SUCCESS_BODY,
        ui::InteractionSequence::StepType::kShown, kTopContainerElementId,
        std::string(), HelpBubbleArrow::kNone, ui::CustomElementEventType(),
        /* must_remain_visible =*/false,
        /* transition_only_on_event =*/false,
        user_education::TutorialDescription::NameElementsCallback(),
        TutorialDescription::ContextMode::kAny);
    customize_chrome_description.steps.emplace_back(success_step);

    customize_chrome_description.histograms =
        user_education::MakeTutorialHistograms<
            kCustomizeChromeTutorialMetricPrefix>(
            customize_chrome_description.steps.size());

    tutorial_registry.AddTutorial(kSidePanelCustomizeChromeTutorialId,
                                  std::move(customize_chrome_description));
  }

  {  // Side panel reading list tutorial

    // The Description for kSidePanelReadingListTutorialId
    TutorialDescription side_panel_description;

    // Open side panel
    TutorialDescription::Step open_side_panel_step(
        0, IDS_TUTORIAL_SIDE_PANEL_READING_LIST_OPEN_SIDE_PANEL,
        ui::InteractionSequence::StepType::kShown, kSidePanelButtonElementId,
        std::string(), HelpBubbleArrow::kTopRight);
    side_panel_description.steps.emplace_back(open_side_panel_step);

    // Click "Add current tab"
    TutorialDescription::Step add_current_tab_step(
        0, IDS_TUTORIAL_SIDE_PANEL_READING_LIST_ADD_TAB,
        ui::InteractionSequence::StepType::kShown,
        kAddCurrentTabToReadingListElementId, std::string(),
        HelpBubbleArrow::kRightTop, ui::CustomElementEventType(), absl::nullopt,
        /* transition_only_on_event =*/false,
        user_education::TutorialDescription::NameElementsCallback(),
        TutorialDescription::ContextMode::kAny);
    side_panel_description.steps.emplace_back(add_current_tab_step);

    // When shown, name the element
    TutorialDescription::Step new_reading_list_item_step(
        0, 0, ui::InteractionSequence::StepType::kShown,
        kSidePanelReadingListUnreadElementId, std::string(),
        HelpBubbleArrow::kNone, ui::CustomElementEventType(),
        /* must_remain_visible =*/true,
        /* transition_only_on_event =*/true,
        base::BindRepeating(
            [](ui::InteractionSequence* sequence, ui::TrackedElement* element) {
              sequence->NameElement(
                  element, base::StringPiece(kReadingListItemElementName));
              return true;
            }),
        TutorialDescription::ContextMode::kAny);
    side_panel_description.steps.emplace_back(new_reading_list_item_step);

    // Mark as read
    TutorialDescription::Step mark_as_read_step(
        0, IDS_TUTORIAL_SIDE_PANEL_READING_LIST_MARK_READ,
        ui::InteractionSequence::StepType::kShown, ui::ElementIdentifier(),
        kReadingListItemElementName, HelpBubbleArrow::kRightTop);
    side_panel_description.steps.emplace_back(mark_as_read_step);

    TutorialDescription::Step detect_mark_as_read_step(
        0, 0, ui::InteractionSequence::StepType::kCustomEvent,
        ui::ElementIdentifier(), kReadingListItemElementName,
        HelpBubbleArrow::kNone, kSidePanelReadingMarkedAsReadEventId);
    side_panel_description.steps.emplace_back(detect_mark_as_read_step);

    // Click drop down
    TutorialDescription::Step click_dropdown_step(
        0, IDS_TUTORIAL_SIDE_PANEL_READING_LIST_CLICK_DROPDOWN,
        ui::InteractionSequence::StepType::kShown, kSidePanelComboboxElementId,
        std::string(), HelpBubbleArrow::kTopLeft);
    side_panel_description.steps.emplace_back(click_dropdown_step);

    TutorialDescription::Step detect_click_dropdown_step(
        0, 0, ui::InteractionSequence::StepType::kCustomEvent,
        kSidePanelComboboxElementId, std::string(), HelpBubbleArrow::kNone,
        kSidePanelComboboxChangedCustomEventId);
    side_panel_description.steps.emplace_back(detect_click_dropdown_step);

    // Completion of the tutorial.
    TutorialDescription::Step success_step(
        IDS_TUTORIAL_GENERIC_SUCCESS_TITLE,
        IDS_TUTORIAL_SIDE_PANEL_READING_LIST_SUCCESS_BODY,
        ui::InteractionSequence::StepType::kShown, kTabStripRegionElementId,
        std::string(), HelpBubbleArrow::kNone);
    side_panel_description.steps.emplace_back(success_step);

    side_panel_description.histograms = user_education::MakeTutorialHistograms<
        kSidePanelReadingListTutorialMetricPrefix>(
        side_panel_description.steps.size());
    tutorial_registry.AddTutorial(kSidePanelReadingListTutorialId,
                                  std::move(side_panel_description));
  }

  {
    TutorialDescription side_search_description;

    // 1st bubble appears and prompts users to open side search
    TutorialDescription::Step open_side_search_in_panel_step(
        0, IDS_SIDE_SEARCH_TUTORIAL_OPEN_SIDE_PANEL,
        ui::InteractionSequence::StepType::kShown, kSideSearchButtonElementId,
        std::string(), HelpBubbleArrow::kBottomCenter);
    side_search_description.steps.emplace_back(open_side_search_in_panel_step);

    // 2nd bubble appears and prompts users to open a link
    TutorialDescription::Step see_side_search(
        0, IDS_SIDE_SEARCH_TUTORIAL_OPEN_A_LINK_TO_TAB,
        ui::InteractionSequence::StepType::kShown, kSideSearchWebViewElementId,
        std::string(), HelpBubbleArrow::kLeftCenter);
    side_search_description.steps.emplace_back(see_side_search);

    // Hidden step that detects a link is pressed
    TutorialDescription::Step detect_side_search_result_clicked(
        0, 0, ui::InteractionSequence::StepType::kCustomEvent,
        kSideSearchWebViewElementId, std::string(), HelpBubbleArrow::kNone,
        kSideSearchResultsClickedCustomEventId);
    side_search_description.steps.emplace_back(
        detect_side_search_result_clicked);

    // 3rd bubble appears and prompts users to press close button
    TutorialDescription::Step click_close(
        0, IDS_SIDE_SEARCH_TUTORIAL_CLOSE_SIDE_PANEL,
        ui::InteractionSequence::StepType::kShown,
        kSidePanelCloseButtonElementId, std::string(),
        HelpBubbleArrow::kTopRight);
    side_search_description.steps.emplace_back(click_close);

    // Completion of the tutorial.
    TutorialDescription::Step success_step(
        IDS_TUTORIAL_GENERIC_SUCCESS_TITLE, IDS_SIDE_SEARCH_PROMO,
        ui::InteractionSequence::StepType::kShown, kSideSearchButtonElementId,
        std::string(), HelpBubbleArrow::kTopRight);
    side_search_description.steps.emplace_back(success_step);

    side_search_description.histograms =
        user_education::MakeTutorialHistograms<kSideSearchTutorialMetricPrefix>(
            side_search_description.steps.size());
    side_search_description.can_be_restarted = true;
    tutorial_registry.AddTutorial(kSideSearchTutorialId,
                                  std::move(side_search_description));
  }
}
