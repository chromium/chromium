// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_glic_container.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#endif  // BUILDFLAG(ENABLE_GLIC)
namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TriggerOutcome {
  kAccepted = 0,
  kDismissed = 1,
  kTimedOut = 2,
  kMaxValue = kTimedOut,
};

constexpr char kDeclutterTriggerOutcomeName[] =
    "Tab.Organization.Declutter.Trigger.Outcome";
constexpr char kDeclutterTriggerBucketedCTRName[] =
    "Tab.Organization.Declutter.Trigger.BucketedCTR";
constexpr int kLargeSpaceBetweenButtons = 4;

}  // namespace
TabGlicContainer::TabGlicContainer(
    TabStripController* tab_strip_controller,
    tabs::TabDeclutterController* tab_declutter_controller)
    : tab_declutter_controller_(tab_declutter_controller) {
  // `tab_declutter_controller_` will be null for some profile types and if
  // feature is not enabled.
  if (tab_declutter_controller_) {
    tab_declutter_button_ =
        AddChildView(CreateTabDeclutterButton(tab_strip_controller));

    SetupButtonProperties(tab_declutter_button_);

    tab_declutter_observation_.Observe(tab_declutter_controller_);
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (GlicEnabling::IsEnabledByFlags()) {
    std::unique_ptr<glic::GlicButton> glic_button =
        std::make_unique<glic::GlicButton>(tab_strip_controller);
    glic_button->SetProperty(views::kCrossAxisAlignmentKey,
                             views::LayoutAlignment::kCenter);
    glic_button->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 0, 0, GetLayoutConstant(TAB_STRIP_PADDING)));

    glic_button_ = AddChildView(std::move(glic_button));
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  SetLayoutManager(std::make_unique<views::FlexLayout>());
}

TabGlicContainer::~TabGlicContainer() = default;

std::unique_ptr<TabStripNudgeButton> TabGlicContainer::CreateTabDeclutterButton(
    TabStripController* tab_strip_controller) {
  auto button = std::make_unique<TabStripNudgeButton>(
      tab_strip_controller,
      base::BindRepeating(&TabGlicContainer::OnTabDeclutterButtonClicked,
                          base::Unretained(this)),
      base::BindRepeating(&TabGlicContainer::OnTabDeclutterButtonDismissed,
                          base::Unretained(this)),
      features::IsTabstripDedupeEnabled()
          ? l10n_util::GetStringUTF16(IDS_TAB_DECLUTTER)
          : l10n_util::GetStringUTF16(IDS_TAB_DECLUTTER_NO_DEDUPE),
      kTabDeclutterButtonElementId, Edge::kNone);

  button->SetTooltipText(
      features::IsTabstripDedupeEnabled()
          ? l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_DECLUTTER)
          : l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_DECLUTTER_NO_DEDUPE));
  button->GetViewAccessibility().SetName(
      features::IsTabstripDedupeEnabled()
          ? l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_DECLUTTER)
          : l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_DECLUTTER_NO_DEDUPE));

  button->SetProperty(views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kCenter);
  return button;
}

void TabGlicContainer::OnTabDeclutterButtonClicked() {
  tabs::TabDeclutterController::EmitEntryPointHistogram(
      tab_search::mojom::TabDeclutterEntryPoint::kNudge);
  base::UmaHistogramEnumeration(kDeclutterTriggerOutcomeName,
                                TriggerOutcome::kAccepted);
  LogDeclutterTriggerBucket(true);

  ExecuteHideTabStripNudge(tab_declutter_button_);
}

void TabGlicContainer::OnTabDeclutterButtonDismissed() {
  base::UmaHistogramEnumeration(kDeclutterTriggerOutcomeName,
                                TriggerOutcome::kDismissed);
  tab_declutter_controller_->OnActionUIDismissed(
      base::PassKey<TabGlicContainer>());

  ExecuteHideTabStripNudge(tab_declutter_button_);
}

void TabGlicContainer::OnTriggerDeclutterUIVisibility() {
  CHECK(tab_declutter_controller_);

  ShowTabStripNudge(tab_declutter_button_);
}

DeclutterTriggerCTRBucket TabGlicContainer::GetDeclutterTriggerBucket(
    bool clicked) {
  const auto total_tab_count =
      tab_declutter_controller_->tab_strip_model()->GetTabCount();
  const auto stale_tab_count = tab_declutter_controller_->GetStaleTabs().size();

  if (total_tab_count < 15) {
    return clicked ? DeclutterTriggerCTRBucket::kClickedUnder15Tabs
                   : DeclutterTriggerCTRBucket::kShownUnder15Tabs;
  } else if (total_tab_count < 20) {
    if (stale_tab_count < 2) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked15To19TabsUnder2Stale
                     : DeclutterTriggerCTRBucket::kShown15To19TabsUnder2Stale;
    } else if (stale_tab_count < 5) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked15To19Tabs2To4Stale
                     : DeclutterTriggerCTRBucket::kShown15To19Tabs2To4Stale;
    } else if (stale_tab_count < 8) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked15To19Tabs5To7Stale
                     : DeclutterTriggerCTRBucket::kShown15To19Tabs5To7Stale;
    } else {
      return clicked ? DeclutterTriggerCTRBucket::kClicked15To19TabsOver7Stale
                     : DeclutterTriggerCTRBucket::kShown15To19TabsOver7Stale;
    }
  } else if (total_tab_count < 25) {
    if (stale_tab_count < 2) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked20To24TabsUnder2Stale
                     : DeclutterTriggerCTRBucket::kShown20To24TabsUnder2Stale;
    } else if (stale_tab_count < 5) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked20To24Tabs2To4Stale
                     : DeclutterTriggerCTRBucket::kShown20To24Tabs2To4Stale;
    } else if (stale_tab_count < 8) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked20To24Tabs5To7Stale
                     : DeclutterTriggerCTRBucket::kShown20To24Tabs5To7Stale;
    } else {
      return clicked ? DeclutterTriggerCTRBucket::kClicked20To24TabsOver7Stale
                     : DeclutterTriggerCTRBucket::kShown20To24TabsOver7Stale;
    }
  } else {
    return clicked ? DeclutterTriggerCTRBucket::kClickedOver24Tabs
                   : DeclutterTriggerCTRBucket::kShownOver24Tabs;
  }
}

void TabGlicContainer::LogDeclutterTriggerBucket(bool clicked) {
  const DeclutterTriggerCTRBucket bucket = GetDeclutterTriggerBucket(clicked);
  base::UmaHistogramEnumeration(kDeclutterTriggerBucketedCTRName, bucket);
}

void TabGlicContainer::ShowTabStripNudge(TabStripNudgeButton* button) {
  // TODO(crbug.com/384099721) add mouse support
  ExecuteShowTabStripNudge(button);
}

void TabGlicContainer::HideTabStripNudge(TabStripNudgeButton* button) {
  // TODO(crbug.com/384099721) add mouse support
  ExecuteHideTabStripNudge(button);
}

void TabGlicContainer::ExecuteShowTabStripNudge(TabStripNudgeButton* button) {
  return;
  // TODO(crbug.com/384554420) add modal support.
  // TODO(crbug.com/384565280) add animation support.
}

void TabGlicContainer::ExecuteHideTabStripNudge(TabStripNudgeButton* button) {
  // TODO(crbug.com/384565280) add animation support.
}

void TabGlicContainer::SetupButtonProperties(TabStripNudgeButton* button) {
  // Set the margins for the button
  const int space_between_buttons = kLargeSpaceBetweenButtons;
  gfx::Insets margin;
  margin.set_right(space_between_buttons);
  button->SetProperty(views::kMarginsKey, margin);

  // Set opacity for the button
  button->SetOpacity(0);
}

BEGIN_METADATA(TabGlicContainer)
END_METADATA
