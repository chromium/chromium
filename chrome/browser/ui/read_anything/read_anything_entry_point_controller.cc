// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"

#include <type_traits>

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_filter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "ui/accessibility/accessibility_features.h"

namespace {

static const int kMaxChipIgnoredCount = 5;
const char* const kDenyList[] = {
    "mail.google.com",
    "whatsapp.com",
    "chatgpt.com",
    "docs.google.com",
    "docs.sandbox.google.com",
    "calendar.google.com",
    "drive.google.com",
    "meet.google.com",
    "instagram.com",
    "tiktok.com",
    "youtube.com",
    "photos.google.com",
};

int GetOmniboxChipIgnoredCount(PrefService* prefs) {
  return prefs->GetInteger(
      prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount);
}

bool ShouldShowOmniboxChip(BrowserWindowInterface* bwi) {
  return GetOmniboxChipIgnoredCount(bwi->GetProfile()->GetPrefs()) <=
         kMaxChipIgnoredCount;
}

bool IsTriggeredByOmnibox(const actions::ActionInvocationContext& context) {
  std::underlying_type_t<page_actions::PageActionTrigger> page_action_trigger =
      context.GetProperty(page_actions::kPageActionTriggerKey);
  return (page_action_trigger != page_actions::kInvalidPageActionTrigger) &&
         features::IsReadAnythingOmniboxChipEnabled() &&
         base::FeatureList::IsEnabled(features::kPageActionsMigration);
}

}  // namespace

namespace read_anything {

// static
void ReadAnythingEntryPointController::InvokePageAction(
    BrowserWindowInterface* bwi,
    const actions::ActionInvocationContext& context) {
  if (!bwi) {
    return;
  }
  std::underlying_type_t<SidePanelOpenTrigger> side_panel_trigger =
      context.GetProperty(kSidePanelOpenTriggerKey);

  ReadAnythingOpenTrigger open_trigger;
  if (side_panel_trigger ==
      static_cast<int>(SidePanelOpenTrigger::kPinnedEntryToolbarButton)) {
    open_trigger = ReadAnythingOpenTrigger::kPinnedSidePanelEntryToolbarButton;
  } else if (IsTriggeredByOmnibox(context)) {
    open_trigger = ReadAnythingOpenTrigger::kOmniboxChip;
    // Reset the ignored count for the omnibox entrypoint because it was used.
    bwi->GetProfile()->GetPrefs()->SetInteger(
        prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount, 0);
    auto* const user_ed = BrowserUserEducationInterface::From(bwi);
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHReadingModePageActionLabelFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  } else {
    return;
  }

  ToggleUI(bwi, open_trigger);
}

// static
void ReadAnythingEntryPointController::ShowUI(
    BrowserWindowInterface* bwi,
    ReadAnythingOpenTrigger open_trigger) {
  if (!bwi) {
    return;
  }
  if (!IsUIShowing(bwi)) {
    base::UmaHistogramEnumeration("Accessibility.ReadAnything.ShowTriggered",
                                  open_trigger);
  }

  if (features::IsImmersiveReadAnythingEnabled()) {
    // TODO(crbug.com/471001915): Once IRM flag is enabled by default, change
    // IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE, one of the triggers of this
    // method, to reflect that it's opening Immersive mode instead of Side
    // Panel.
    if (tabs::TabInterface* tab = bwi->GetActiveTabInterface()) {
      auto* controller = ReadAnythingController::From(tab);
      CHECK(controller);
      controller->ShowImmersiveUI(open_trigger);
    }
  } else {
    SidePanelOpenTrigger side_panel_open_trigger =
        read_anything::ReadAnythingToSidePanelOpenTrigger(open_trigger);

    bwi->GetFeatures().side_panel_ui()->Show(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything),
        side_panel_open_trigger);
  }
}

// static
void ReadAnythingEntryPointController::ToggleUI(
    BrowserWindowInterface* bwi,
    ReadAnythingOpenTrigger open_trigger) {
  if (!bwi) {
    return;
  }

  if (!IsUIShowing(bwi)) {
    base::UmaHistogramEnumeration("Accessibility.ReadAnything.ShowTriggered",
                                  open_trigger);
  }

  if (features::IsImmersiveReadAnythingEnabled()) {
    if (tabs::TabInterface* tab = bwi->GetActiveTabInterface()) {
      auto* controller = ReadAnythingController::From(tab);
      CHECK(controller);
      controller->ToggleUI(open_trigger);
    }
  } else {
    SidePanelOpenTrigger side_panel_open_trigger =
        read_anything::ReadAnythingToSidePanelOpenTrigger(open_trigger);

    bwi->GetFeatures().side_panel_ui()->Toggle(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything),
        side_panel_open_trigger);
  }
}

// static
bool ReadAnythingEntryPointController::IsUIShowing(
    BrowserWindowInterface* bwi) {
  if (features::IsImmersiveReadAnythingEnabled()) {
    auto* controller =
        ReadAnythingController::From(bwi->GetActiveTabInterface());
    CHECK(controller);
    auto state = controller->GetPresentationState();
    return state ==
               ReadAnythingController::PresentationState::kInImmersiveOverlay ||
           state == ReadAnythingController::PresentationState::kInSidePanel;
  } else {
    return IsReadAnythingEntryShowing(bwi);
  }
}

// static
void ReadAnythingEntryPointController::UpdatePageActionVisibility(
    bool should_show_page_action,
    BrowserWindowInterface* bwi,
    base::OnceCallback<void(user_education::FeaturePromoResult promo_result)>
        show_promo_callback) {
  if (!base::FeatureList::IsEnabled(features::kPageActionsMigration) ||
      !features::IsReadAnythingOmniboxChipEnabled() || !bwi) {
    return;
  }

  page_actions::PageActionController* page_action_controller =
      bwi->GetActiveTabInterface()->GetTabFeatures()->page_action_controller();
  auto* const user_ed = BrowserUserEducationInterface::From(bwi);
  // No need to show the button if reading mode is already open.
  if (should_show_page_action && !IsUIShowing(bwi)) {
    page_action_controller->Show(kActionSidePanelShowReadAnything);
    if (ShouldShowOmniboxChip(bwi)) {
      page_action_controller->ShowSuggestionChip(
          kActionSidePanelShowReadAnything);
    }
    user_education::FeaturePromoParams params(
        feature_engagement::kIPHReadingModePageActionLabelFeature);
    if (show_promo_callback) {
      params.show_promo_result_callback = std::move(show_promo_callback);
    }
    user_ed->MaybeShowFeaturePromo(std::move(params));
  } else {
    user_ed->AbortFeaturePromo(
        feature_engagement::kIPHReadingModePageActionLabelFeature);
    page_action_controller->Hide(kActionSidePanelShowReadAnything);
  }
}

// static
bool ReadAnythingEntryPointController::CheckIfShouldSuggestReadingModeNaive(
    BrowserWindowInterface* bwi) {
  if (!features::IsReadAnythingOmniboxChipEnabled() || !bwi) {
    return false;
  }

  // Disable the omnibox on app windows, as these windows don't usually have
  // omnibox support.
  Browser* browser = bwi->GetBrowserForMigrationOnly();
  if (browser && (browser->is_type_app() || browser->is_type_app_popup())) {
    return false;
  }

  // Don't show the omnibox entrypoint for non-HTTP(S) URLs. These URLs are
  // not supported by Readability, which is used to check whether the current
  // page is a good candidate for distillation.
  content::WebContents* contents = bwi->GetActiveTabInterface()->GetContents();
  const GURL& url = contents->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Don't show the omnibox entrypoint for sites we know don't distill well.
  for (const char* domain : kDenyList) {
    if (url.DomainIs(domain)) {
      return false;
    }
  }

  return true;
}

// static
void ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
    BrowserWindowInterface* bwi,
    base::OnceCallback<void(bool)> result_callback) {
  if (!features::IsReadAnythingOmniboxChipEnabled() || !bwi) {
    std::move(result_callback).Run(false);
    return;
  }
  // Don't show the omnibox entrypoint if automation is enabled, such as
  // during automated testing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation)) {
    std::move(result_callback).Run(false);
    return;
  }

  if (!CheckIfShouldSuggestReadingModeNaive(bwi)) {
    std::move(result_callback).Run(false);
    return;
  }

  // Readability will callback with whether or not the current contents are a
  // good candidate for distillation.
  content::WebContents* contents = bwi->GetActiveTabInterface()->GetContents();
  RunReadabilityHeuristicsOnWebContents(contents, std::move(result_callback));
}

// static
void ReadAnythingEntryPointController::OnPageActionIgnored(
    BrowserWindowInterface* bwi) {
  if (!base::FeatureList::IsEnabled(features::kPageActionsMigration) ||
      !features::IsReadAnythingOmniboxChipEnabled()) {
    return;
  }

  PrefService* prefs = bwi->GetProfile()->GetPrefs();
  prefs->SetInteger(prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount,
                    GetOmniboxChipIgnoredCount(prefs) + 1);
  if (!ShouldShowOmniboxChip(bwi)) {
    page_actions::PageActionController* page_action_controller =
        bwi->GetActiveTabInterface()
            ->GetTabFeatures()
            ->page_action_controller();
    page_action_controller->HideSuggestionChip(
        kActionSidePanelShowReadAnything);
  }
}

}  // namespace read_anything
