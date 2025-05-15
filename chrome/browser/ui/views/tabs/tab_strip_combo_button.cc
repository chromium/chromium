// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_combo_button.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
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

constexpr int kButtonGapNoBackground = 14;
constexpr base::TimeDelta kAccidentalClickThreshold = base::Seconds(1);
constexpr char kNewTabButtonAccidentalClickName[] =
    "Tabs.NewTabButton.AccidentalClicks";
constexpr char kTabSearchAccidentalClickName[] =
    "Tabs.TabSearch.AccidentalClicks";
}  // namespace

TabStripComboButton::TabStripComboButton(BrowserWindowInterface* browser,
                                         TabStrip* tab_strip) {
  Edge new_tab_button_flat_edge = Edge::kNone;
  if (features::HasTabstripComboButtonWithBackground()) {
    if (features::HasTabstripComboButtonWithReverseButtonOrder()) {
      new_tab_button_flat_edge =
          base::i18n::IsRTL() ? Edge::kRight : Edge::kLeft;
    } else {
      new_tab_button_flat_edge =
          base::i18n::IsRTL() ? Edge::kLeft : Edge::kRight;
    }
  }
  std::unique_ptr<TabStripControlButton> new_tab_button =
      std::make_unique<TabStripControlButton>(
          tab_strip->controller(),
          base::BindRepeating(&TabStrip::NewTabButtonPressed,
                              base::Unretained(tab_strip)),
          vector_icons::kAddIcon, new_tab_button_flat_edge);
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
  } else {
    // Add a gap between the new tab button and tab search button.
    gfx::Insets button_margins;
    if (features::HasTabstripComboButtonWithReverseButtonOrder()) {
      button_margins = gfx::Insets::TLBR(0, kButtonGapNoBackground, 0, 0);
    } else {
      button_margins = gfx::Insets::TLBR(0, 0, 0, kButtonGapNoBackground);
    }
    new_tab_button->SetProperty(views::kMarginsKey, button_margins);
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
  separator->SetBorderRadius(TabStyle::Get()->GetSeparatorCornerRadius());
  separator->SetPreferredSize(TabStyle::Get()->GetSeparatorSize());
  subscriptions_.push_back(browser->RegisterDidBecomeActive(base::BindRepeating(
      &TabStripComboButton::DidBecomeActive, base::Unretained(this))));
  subscriptions_.push_back(
      browser->RegisterDidBecomeInactive(base::BindRepeating(
          &TabStripComboButton::DidBecomeInactive, base::Unretained(this))));

  Edge tab_search_button_flat_edge = Edge::kNone;
  if (features::HasTabstripComboButtonWithBackground()) {
    if (features::HasTabstripComboButtonWithReverseButtonOrder()) {
      tab_search_button_flat_edge =
          base::i18n::IsRTL() ? Edge::kLeft : Edge::kRight;
    } else {
      tab_search_button_flat_edge =
          base::i18n::IsRTL() ? Edge::kRight : Edge::kLeft;
    }
  }
  std::unique_ptr<TabSearchButton> tab_search_button =
      std::make_unique<TabSearchButton>(tab_strip->controller(), browser,
                                        tab_search_button_flat_edge,
                                        Edge::kNone, tab_strip);
  tab_search_button->SetFlatEdgeFactor(1);
  tab_search_button->SetProperty(views::kCrossAxisAlignmentKey,
                                 views::LayoutAlignment::kCenter);
  subscriptions_.push_back(tab_search_button->AddStateChangedCallback(
      base::BindRepeating(&TabStripComboButton::OnTabSearchButtonStateChanged,
                          base::Unretained(this))));

  auto* button_container = AddChildView(std::make_unique<views::View>());
  auto* separator_container = AddChildView(std::make_unique<views::View>());
  button_container->SetLayoutManager(std::make_unique<views::FlexLayout>());
  separator_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  separator_container->SetCanProcessEventsWithinSubtree(false);

  if (features::HasTabstripComboButtonWithReverseButtonOrder()) {
    tab_search_button_ =
        button_container->AddChildView(std::move(tab_search_button));
    new_tab_button_ = button_container->AddChildView(std::move(new_tab_button));
  } else {
    new_tab_button_ = button_container->AddChildView(std::move(new_tab_button));
    tab_search_button_ =
        button_container->AddChildView(std::move(tab_search_button));
  }
  separator_ = separator_container->AddChildView(std::move(separator));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetNotifyEnterExitOnChild(true);
}

TabStripComboButton::~TabStripComboButton() = default;

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
  if (tab_search_button_->GetState() == views::Button::STATE_PRESSED) {
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

void TabStripComboButton::DidBecomeActive(BrowserWindowInterface* browser) {
  if (features::HasTabstripComboButtonWithBackground()) {
    if (using_custom_theme_) {
      separator_->SetColorId(ui::kColorFrameActive);
    } else {
      separator_->SetColorId(kColorTabStripComboButtonSeparator);
    }
  } else {
    separator_->SetColorId(kColorTabDividerFrameActive);
  }
}

void TabStripComboButton::DidBecomeInactive(BrowserWindowInterface* browser) {
  if (using_custom_theme_) {
    separator_->SetColorId(ui::kColorFrameInactive);
  } else {
    separator_->SetColorId(kColorTabDividerFrameInactive);
  }
}

void TabStripComboButton::OnThemeChanged() {
  views::View::OnThemeChanged();
  using_custom_theme_ = GetThemeProvider()->HasCustomImage(IDR_THEME_FRAME);

  ui::ColorId foreground_active_color;
  ui::ColorId foreground_inactive_color;
  ui::ColorId background_active_color;
  ui::ColorId background_inactive_color;
  if (using_custom_theme_ || features::HasTabstripComboButtonWithBackground()) {
    foreground_active_color = kColorNewTabButtonForegroundFrameActive;
    foreground_inactive_color = kColorNewTabButtonForegroundFrameInactive;
    background_active_color = kColorNewTabButtonCRBackgroundFrameActive;
    background_inactive_color = kColorNewTabButtonCRBackgroundFrameInactive;
  } else {
    foreground_active_color = kColorTabForegroundInactiveFrameActive;
    foreground_inactive_color = kColorNewTabButtonCRForegroundFrameInactive;
    background_active_color = kColorNewTabButtonBackgroundFrameActive;
    background_inactive_color = kColorNewTabButtonBackgroundFrameInactive;
  }
  new_tab_button_->SetForegroundFrameActiveColorId(foreground_active_color);
  new_tab_button_->SetForegroundFrameInactiveColorId(foreground_inactive_color);
  new_tab_button_->SetBackgroundFrameActiveColorId(background_active_color);
  new_tab_button_->SetBackgroundFrameInactiveColorId(background_inactive_color);
  tab_search_button_->SetForegroundFrameActiveColorId(foreground_active_color);
  tab_search_button_->SetForegroundFrameInactiveColorId(
      foreground_inactive_color);
  tab_search_button_->SetBackgroundFrameActiveColorId(background_active_color);
  tab_search_button_->SetBackgroundFrameInactiveColorId(
      background_inactive_color);
}

void TabStripComboButton::UpdateSeparatorVisibility() {
  const views::Button::ButtonState new_tab_button_state =
      new_tab_button_->GetState();
  const views::Button::ButtonState tab_search_button_state =
      tab_search_button_->GetState();
  const bool is_visible =
      features::HasTabstripComboButtonWithBackground()
          ? new_tab_button_state != views::Button::STATE_HOVERED &&
                new_tab_button_state != views::Button::STATE_PRESSED &&
                tab_search_button_state != views::Button::STATE_HOVERED &&
                tab_search_button_state != views::Button::STATE_PRESSED
          : true;
  separator_->SetVisible(is_visible);
}

BEGIN_METADATA(TabStripComboButton)
END_METADATA
