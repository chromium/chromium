// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"

#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/user_education/feature_promo_registry.h"
#include "chrome/browser/ui/user_education/feature_promo_specification.h"
#include "chrome/browser/ui/user_education/help_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/help_bubble_params.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/help_bubble_factory_views.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/user_education/help_bubble_factory_mac.h"
#endif

namespace {

const char kTabGroupTutorialMetricPrefix[] = "TabGroup";
constexpr char kTabGroupHeaderElementName[] = "TabGroupHeader";

class BrowserHelpBubbleAcceleratorDelegate
    : public HelpBubbleAcceleratorDelegate {
 public:
  BrowserHelpBubbleAcceleratorDelegate() = default;
  ~BrowserHelpBubbleAcceleratorDelegate() override = default;

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
};

}  // namespace

const char kTabGroupTutorialId[] = "Tab Group Tutorial";

void RegisterChromeHelpBubbleFactories(HelpBubbleFactoryRegistry& registry) {
  static base::NoDestructor<BrowserHelpBubbleAcceleratorDelegate> delegate;
  registry.MaybeRegister<HelpBubbleFactoryViews>(delegate.get());
#if BUILDFLAG(IS_MAC)
  registry.MaybeRegister<HelpBubbleFactoryMac>();
#endif
}

void MaybeRegisterChromeFeaturePromos(FeaturePromoRegistry& registry) {
  // Verify that we haven't already registered the expected features.
  // TODO(dfried): figure out if we should do something more sophisticated here.
  if (registry.IsFeatureRegistered(
          feature_engagement::kIPHDesktopPwaInstallFeature))
    return;

  // kIPHAutofillVirtualCardSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillVirtualCardSuggestionFeature,
          kAutofillCreditCardSuggestionEntryElementId,
          IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_IPH_BUBBLE_LABEL,
          IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_IPH_BUBBLE_LABEL,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)));

  // kIPHDesktopPwaInstallFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHDesktopPwaInstallFeature, kInstallPwaElementId,
      IDS_DESKTOP_PWA_INSTALL_PROMO));

  // kIPHUpdatedConnectionSecurityIndicatorsFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHUpdatedConnectionSecurityIndicatorsFeature,
          kLocationIconElementId,
          IDS_UPDATED_CONNECTION_SECURITY_INDICATORS_PROMO)
          .SetBubbleArrow(HelpBubbleArrow::kTopLeft)));

  // kIPHDesktopTabGroupsNewGroupFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForTutorialPromo(
                    feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
                    kTabStripRegionElementId, IDS_TAB_GROUPS_NEW_GROUP_PROMO,
                    kTabGroupTutorialId)
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
      kReadLaterButtonElementId, IDS_READING_LIST_DISCOVERY_PROMO));

  // kIPHReadingListEntryPointFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
      feature_engagement::kIPHReadingListEntryPointFeature,
      kBookmarkStarViewElementId, IDS_READING_LIST_ENTRY_POINT_PROMO));

  // kIPHIntentChipFeature
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHIntentChipFeature, kIntentChipElementId,
      IDS_INTENT_CHIP_IPH, IDS_INTENT_CHIP_IPH,
      FeaturePromoSpecification::AcceleratorInfo()));

  // kIPHReadingListInSidePanelFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHReadingListInSidePanelFeature,
      kReadLaterButtonElementId, IDS_READING_LIST_IN_SIDE_PANEL_PROMO));

  // kIPHReopenTabFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHReopenTabFeature, kAppMenuButtonElementId,
      IDS_REOPEN_TAB_PROMO, IDS_REOPEN_TAB_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo(IDC_RESTORE_TAB)));

  // kIPHSideSearchFeature:
#if BUILDFLAG(ENABLE_SIDE_SEARCH)
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &feature_engagement::kIPHSideSearchFeature,
                    kSideSearchButtonElementId, IDS_SIDE_SEARCH_PROMO)
                    .SetBubbleArrow(HelpBubbleArrow::kTopLeft)));
#endif

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
}

void MaybeRegisterChromeTutorials(TutorialRegistry& tutorial_registry) {
  // TODO (dfried): we might want to do something more sophisticated in the
  // future.
  if (tutorial_registry.IsTutorialRegistered(kTabGroupTutorialId))
    return;

  {  // Tab Group Tutorial
    TutorialDescription description;

    // The initial step.
    TutorialDescription::Step create_tabgroup_step(
        0, IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
        ui::InteractionSequence::StepType::kShown, kTabStripRegionElementId,
        std::string(), HelpBubbleArrow::kNone);
    description.steps.emplace_back(create_tabgroup_step);

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
    description.steps.emplace_back(std::move(new_tab_group_step));

    // The menu step.
    TutorialDescription::Step bubble_menu_edit_step(
        0, IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE,
        ui::InteractionSequence::StepType::kShown, kTabGroupEditorBubbleId,
        std::string(), HelpBubbleArrow::kLeftCenter,
        ui::CustomElementEventType(),
        /*must_remain_visible =*/false);
    description.steps.emplace_back(std::move(bubble_menu_edit_step));

    TutorialDescription::Step bubble_menu_edit_ended_step(
        0, 0, ui::InteractionSequence::StepType::kHidden,
        kTabGroupEditorBubbleId, std::string(), HelpBubbleArrow::kNone,
        ui::CustomElementEventType(),
        /*must_remain_visible =*/false);
    description.steps.emplace_back(std::move(bubble_menu_edit_ended_step));

    // Drag tab into the group.
    TutorialDescription::Step drag_tab_into_group_step(
        0, IDS_TUTORIAL_TAB_GROUP_DRAG_TAB,
        ui::InteractionSequence::StepType::kShown, kTabStripRegionElementId,
        std::string(), HelpBubbleArrow::kNone);
    description.steps.emplace_back(std::move(drag_tab_into_group_step));

    TutorialDescription::Step successfully_drag_tab_into_group_step(
        0, 0, ui::InteractionSequence::StepType::kCustomEvent,
        ui::ElementIdentifier(), std::string(), HelpBubbleArrow::kTopCenter,
        kTabGroupedCustomEventId, /*must_remain_visible =*/true);
    description.steps.emplace_back(
        std::move(successfully_drag_tab_into_group_step));

    // Click to collapse the tab group.
    TutorialDescription::Step collapse_step(
        0, IDS_TUTORIAL_TAB_GROUP_COLLAPSE,
        ui::InteractionSequence::StepType::kShown, ui::ElementIdentifier(),
        kTabGroupHeaderElementName, HelpBubbleArrow::kTopCenter);
    description.steps.emplace_back(std::move(collapse_step));

    TutorialDescription::Step detect_collapse_step(
        0, 0, ui::InteractionSequence::StepType::kActivated,
        kTabGroupHeaderElementId, std::string(), HelpBubbleArrow::kNone);
    description.steps.emplace_back(std::move(detect_collapse_step));

    // Completion of the tutorial.
    TutorialDescription::Step success_step(
        IDS_TUTORIAL_TAB_GROUP_SUCCESS_TITLE,
        IDS_TUTORIAL_TAB_GROUP_SUCCESS_DESCRIPTION,
        ui::InteractionSequence::StepType::kShown, kTabStripRegionElementId,
        std::string(), HelpBubbleArrow::kNone);
    description.steps.emplace_back(std::move(success_step));

    description.histograms =
        MakeTutorialHistograms<kTabGroupTutorialMetricPrefix>(
            description.steps.size());
    tutorial_registry.AddTutorial(kTabGroupTutorialId, std::move(description));
  }
}
