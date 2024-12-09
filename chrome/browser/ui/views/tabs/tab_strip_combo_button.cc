// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_combo_button.h"

#include "base/time/time.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

// LINT.IfChange(AccidentalClickType)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AccidentalClickType {
  kClick = 0,
  kAccidentalClick = 1,
  kMaxValue = kAccidentalClick,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:AccidentalClickType)

constexpr int kSeparatorBorderRadius = 2;
constexpr int kSeparatorWidth = 2;
constexpr int kSeparatorWidthNoBackground = 1;
constexpr int kSeparatorHeight = 16;
constexpr base::TimeDelta kAccidentalClickThreshold = base::Seconds(1);
constexpr char kNewTabButtonAccidentalClickName[] =
    "Tabs.NewTabButton.AccidentalClicks";
constexpr char kTabSearchAccidentalClickName[] =
    "Tabs.TabSearch.AccidentalClicks";
}  // namespace

TabStripComboButton::TabStripComboButton(BrowserWindowInterface* browser,
                                         TabStrip* tab_strip) {
  std::unique_ptr<TabStripControlButton> new_tab_button =
      std::make_unique<TabStripControlButton>(
          tab_strip->controller(),
          base::BindRepeating(&TabStrip::NewTabButtonPressed,
                              base::Unretained(tab_strip)),
          vector_icons::kAddIcon,
          base::i18n::IsRTL() ? Edge::kLeft : Edge::kRight);
  new_tab_button->SetProperty(views::kElementIdentifierKey,
                              kNewTabButtonElementId);

  if (features::HasTabstripComboButtonWithBackground()) {
    new_tab_button->SetForegroundFrameActiveColorId(
        kColorNewTabButtonForegroundFrameActive);
    new_tab_button->SetForegroundFrameInactiveColorId(
        kColorNewTabButtonForegroundFrameInactive);
    new_tab_button->SetBackgroundFrameActiveColorId(
        kColorNewTabButtonCRBackgroundFrameActive);
    new_tab_button->SetBackgroundFrameInactiveColorId(
        kColorNewTabButtonCRBackgroundFrameInactive);
  }

  new_tab_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_NEW_TAB));
  new_tab_button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_NEWTAB));
  subscriptions_.push_back(new_tab_button->AddStateChangedCallback(
      base::BindRepeating(&TabStripComboButton::OnNewTabButtonStateChanged,
                          base::Unretained(this))));

#if BUILDFLAG(IS_LINUX)
  // The New Tab Button can be middle-clicked on Linux.
  new_tab_button->SetTriggerableEventFlags(
      new_tab_button->GetTriggerableEventFlags() | ui::EF_MIDDLE_MOUSE_BUTTON);
#endif

  std::unique_ptr<views::Separator> separator =
      std::make_unique<views::Separator>();
  const int color_id = features::HasTabstripComboButtonWithBackground()
                           ? kColorTabStripComboButtonSeparator
                           : kColorTabStripComboButtonSeparatorOnHeader;
  separator->SetColorId(color_id);
  separator->SetBorderRadius(kSeparatorBorderRadius);
  const int separator_width = features::HasTabstripComboButtonWithBackground()
                                  ? kSeparatorWidth
                                  : kSeparatorWidthNoBackground;
  separator->SetPreferredSize(gfx::Size(separator_width, kSeparatorHeight));

  std::unique_ptr<TabSearchContainer> tab_search_container =
      std::make_unique<TabSearchContainer>(
          tab_strip->controller(), browser->GetTabStripModel(), true, this,
          browser, browser->GetFeatures().tab_declutter_controller(), this,
          tab_strip);
  tab_search_container->SetProperty(views::kCrossAxisAlignmentKey,
                                    views::LayoutAlignment::kCenter);
  subscriptions_.push_back(
      tab_search_container->tab_search_button()->AddStateChangedCallback(
          base::BindRepeating(
              &TabStripComboButton::OnTabSearchButtonStateChanged,
              base::Unretained(this))));

  auto* button_container = AddChildView(std::make_unique<views::View>());
  auto* separator_container = AddChildView(std::make_unique<views::View>());
  button_container->SetLayoutManager(std::make_unique<views::FlexLayout>());
  separator_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  separator_container->SetCanProcessEventsWithinSubtree(false);

  new_tab_button_ = button_container->AddChildView(std::move(new_tab_button));
  tab_search_container_ =
      button_container->AddChildView(std::move(tab_search_container));
  separator_ = separator_container->AddChildView(std::move(separator));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetNotifyEnterExitOnChild(true);
}

TabStripComboButton::~TabStripComboButton() {}

void TabStripComboButton::OnNewTabButtonStateChanged() {
  if (new_tab_button_->GetState() == views::Button::STATE_PRESSED) {
    new_tab_button_last_pressed_ = base::TimeTicks::Now();
    base::UmaHistogramEnumeration(kNewTabButtonAccidentalClickName,
                                  AccidentalClickType::kClick);
    if (!tab_search_button_last_pressed_.is_null() &&
        (new_tab_button_last_pressed_ - tab_search_button_last_pressed_) <
            kAccidentalClickThreshold) {
      base::UmaHistogramEnumeration(kTabSearchAccidentalClickName,
                                    AccidentalClickType::kAccidentalClick);
    }
  }

  UpdateSeparatorVisibility();
}

void TabStripComboButton::OnTabSearchButtonStateChanged() {
  if (tab_search_container_->tab_search_button()->GetState() ==
      views::Button::STATE_PRESSED) {
    tab_search_button_last_pressed_ = base::TimeTicks::Now();
    base::UmaHistogramEnumeration(kTabSearchAccidentalClickName,
                                  AccidentalClickType::kClick);
    if (!new_tab_button_last_pressed_.is_null() &&
        (tab_search_button_last_pressed_ - new_tab_button_last_pressed_) <
            kAccidentalClickThreshold) {
      base::UmaHistogramEnumeration(kNewTabButtonAccidentalClickName,
                                    AccidentalClickType::kAccidentalClick);
    }
  }

  UpdateSeparatorVisibility();
}

void TabStripComboButton::UpdateSeparatorVisibility() {
  const views::Button::ButtonState new_tab_button_state =
      new_tab_button_->GetState();
  const views::Button::ButtonState tab_search_button_state =
      tab_search_container_->tab_search_button()->GetState();
  separator_->SetVisible(
      new_tab_button_state != views::Button::STATE_HOVERED &&
      new_tab_button_state != views::Button::STATE_PRESSED &&
      tab_search_button_state != views::Button::STATE_HOVERED &&
      tab_search_button_state != views::Button::STATE_PRESSED);
}

BEGIN_METADATA(TabStripComboButton)
END_METADATA
