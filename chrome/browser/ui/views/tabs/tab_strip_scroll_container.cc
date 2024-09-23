// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_scroll_container.h"
#include <memory>

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_shader.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_scrolling_overflow_indicator_strategy.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace {
SkColor4f GetCurrentFrameColor(TabStrip* tab_strip) {
  return SkColor4f::FromColor(tab_strip->controller()->GetFrameColor(
      BrowserFrameActiveState::kUseCurrent));
}

SkColor4f GetShadowColor(TabStrip* tab_strip) {
  return SkColor4f::FromColor(
      tab_strip->GetColorProvider()->GetColor(ui::kColorShadowBase));
}

// Define a custom FlexRule for |scroll_view_|. Equivalent to using a
// (kScaleToMinimum, kPreferred) flex specification on the tabstrip itself,
// bypassing the ScrollView.
// TODO(crbug.com/40721975): Make ScrollView take on TabStrip's preferred size
// instead.
gfx::Size TabScrollContainerFlexRule(const views::View* tab_strip,
                                     const views::View* view,
                                     const views::SizeBounds& size_bounds) {
  const gfx::Size preferred_size = tab_strip->GetPreferredSize(size_bounds);
  const int minimum_width = tab_strip->GetMinimumSize().width();
  const int width = std::max(
      minimum_width, size_bounds.width().min_of(preferred_size.width()));
  return gfx::Size(width, preferred_size.height());
}

std::unique_ptr<views::ImageButton> CreateScrollButton(
    views::Button::PressedCallback callback) {
  // TODO(tbergquist): These have a lot in common with the NTB and the tab
  // search buttons. Could probably extract a base class.
  auto scroll_button =
      std::make_unique<views::ImageButton>(std::move(callback));
  scroll_button->SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  scroll_button->SetImageHorizontalAlignment(
      views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
  scroll_button->SetHasInkDropActionOnClick(true);
  views::InkDrop::Get(scroll_button.get())
      ->SetMode(views::InkDropHost::InkDropMode::ON);
  scroll_button->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  scroll_button->SetPreferredSize(gfx::Size(28, 28));
  views::HighlightPathGenerator::Install(
      scroll_button.get(),
      std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets()));

  const views::FlexSpecification button_flex_spec =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred);
  scroll_button->SetProperty(views::kFlexBehaviorKey, button_flex_spec);
  return scroll_button;
}

// Must be kept the same as kTabScrollingButtonPositionVariations values
enum ScrollButtonPositionType {
  kJoinedButtonsRight = 0,
  kJoinedButtonsLeft = 1,
  kSplitButtons = 2
};

}  // namespace

TabStripScrollContainer::TabStripScrollContainer(
    std::unique_ptr<TabStrip> tab_strip) {
  SetLayoutManager(std::make_unique<views::FillLayout>())
      ->SetMinimumSizeEnabled(true);

  // TODO(crbug.com/40721975): ScrollView doesn't propagate changes to
  // the TabStrip's preferred size; observe that manually.
  tab_strip_observation_.Observe(tab_strip.get());
  tab_strip->SetAvailableWidthCallback(
      base::BindRepeating(&TabStripScrollContainer::GetTabStripAvailableWidth,
                          base::Unretained(this)));

  std::unique_ptr<views::ScrollView> scroll_view =
      std::make_unique<views::ScrollView>(
          views::ScrollView::ScrollWithLayers::kEnabled);
  scroll_view_ = scroll_view.get();
  scroll_view->SetBackgroundColor(std::nullopt);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view->SetTreatAllScrollEventsAsHorizontal(true);
  scroll_view->SetContents(std::move(tab_strip));

  overflow_indicator_strategy_ =
      TabStripScrollingOverflowIndicatorStrategy::CreateFromFeatureFlag(
          scroll_view_,
          base::BindRepeating(&GetCurrentFrameColor, this->tab_strip()),
          base::BindRepeating(&GetShadowColor, this->tab_strip()));
  overflow_indicator_strategy_->Init();
  // This base::Unretained is safe because the callback is called by the
  // layout manager, which is cleaned up before view children like
  // |scroll_view| (which owns |tab_strip|).
  scroll_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(base::BindRepeating(
          &TabScrollContainerFlexRule, base::Unretained(this->tab_strip()))));

  on_contents_scrolled_subscription_ = scroll_view->AddContentsScrolledCallback(
      base::BindRepeating(&TabStripScrollContainer::OnContentsScrolledCallback,
                          base::Unretained(this)));

  if (!base::FeatureList::IsEnabled(features::kTabScrollingButtonPosition)) {
    leading_scroll_button_ = nullptr;
    trailing_scroll_button_ = nullptr;
    overflow_view_ = AddChildView(
        std::make_unique<OverflowView>(std::move(scroll_view), nullptr));
    return;
  }

  int scroll_button_strategy = base::GetFieldTrialParamByFeatureAsInt(
      features::kTabScrollingButtonPosition,
      features::kTabScrollingButtonPositionParameterName, 0);

  std::unique_ptr<views::ImageButton> leading_scroll_button =
      CreateScrollButton(
          base::BindRepeating(&TabStripScrollContainer::ScrollTowardsLeadingTab,
                              base::Unretained(this)));
  leading_scroll_button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_SCROLL_LEADING));

  std::unique_ptr<views::ImageButton> trailing_scroll_button =
      CreateScrollButton(base::BindRepeating(
          &TabStripScrollContainer::ScrollTowardsTrailingTab,
          base::Unretained(this)));
  trailing_scroll_button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_SCROLL_TRAILING));

  // The space in dips between the scroll buttons and the NTB.
  constexpr int kScrollButtonsTrailingMargin = 8;
  trailing_scroll_button->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0, 0, kScrollButtonsTrailingMargin));

  leading_scroll_button_ = leading_scroll_button.get();
  trailing_scroll_button_ = trailing_scroll_button.get();

  switch (scroll_button_strategy) {
    case ScrollButtonPositionType::kJoinedButtonsLeft:
    case ScrollButtonPositionType::kJoinedButtonsRight: {
      std::unique_ptr<views::View> scroll_button_container =
          std::make_unique<views::View>();
      views::FlexLayout* scroll_button_layout =
          scroll_button_container->SetLayoutManager(
              std::make_unique<views::FlexLayout>());
      scroll_button_layout->SetOrientation(
          views::LayoutOrientation::kHorizontal);
      scroll_button_container->AddChildView(std::move(leading_scroll_button));
      scroll_button_container->AddChildView(std::move(trailing_scroll_button));
      overflow_view_ = AddChildView(std::make_unique<OverflowView>(
          std::move(scroll_view),
          scroll_button_strategy == ScrollButtonPositionType::kJoinedButtonsLeft
              ? std::move(scroll_button_container)
              : nullptr,
          scroll_button_strategy ==
                  ScrollButtonPositionType::kJoinedButtonsRight
              ? std::move(scroll_button_container)
              : nullptr));
    } break;
    case ScrollButtonPositionType::kSplitButtons:
      overflow_view_ = AddChildView(std::make_unique<OverflowView>(
          std::move(scroll_view), std::move(leading_scroll_button),
          std::move(trailing_scroll_button)));
      break;
  }
}

TabStripScrollContainer::~TabStripScrollContainer() = default;

void TabStripScrollContainer::OnViewPreferredSizeChanged(views::View* view) {
  DCHECK_EQ(tab_strip(), view);

  PreferredSizeChanged();
}

void TabStripScrollContainer::OnContentsScrolledCallback() {
  views::Widget* root_widget = tab_strip()->GetWidget();
  std::set<raw_ptr<views::Widget, SetExperimental>> children_widgets;
  views::Widget::GetAllOwnedWidgets(root_widget->GetNativeView(),
                                    &children_widgets);

  for (views::Widget* child_widget : children_widgets) {
    views::BubbleDialogDelegate* bdd =
        child_widget->widget_delegate()->AsBubbleDialogDelegate();
    if (bdd) {
      views::View* anchor_view = bdd->GetAnchorView();
      if (this->Contains(anchor_view)) {
        child_widget->Hide();
      }
    }
  }

  // disable the scroll buttons if fully scrolled and re-enable them otherwise
  MaybeUpdateScrollButtonState();
}

int TabStripScrollContainer::GetTabStripAvailableWidth() const {
  return overflow_view_->GetAvailableSize(scroll_view_).width().value();
}

void TabStripScrollContainer::ScrollTowardsLeadingTab() {
  gfx::Rect visible_content = scroll_view_->GetVisibleRect();
  tab_strip()->ScrollTowardsLeadingTabs(visible_content.width());
}

void TabStripScrollContainer::ScrollTowardsTrailingTab() {
  gfx::Rect visible_content = scroll_view_->GetVisibleRect();
  tab_strip()->ScrollTowardsTrailingTabs(visible_content.width());
}

void TabStripScrollContainer::FrameColorsChanged() {
  SkColor foreground_enabled_color =
      tab_strip()->GetTabForegroundColor(TabActive::kInactive);
  // TODO(crbug.com/40879445): Get a disabled color that is lighter
  // and changes with the frame background color
  SkColor foreground_disabled_color =
      GetColorProvider()->GetColor(kColorTabForegroundInactiveFrameInactive);

  /* When the buttons are fully scrolled in a direction the corresponding button
     is disabled. They are hidden when there are not enough tabs to be in tab
     scrolling mode. */
  if (leading_scroll_button_) {
    views::SetImageFromVectorIconWithColor(
        leading_scroll_button_, kLeadingScrollIcon, foreground_enabled_color,
        foreground_disabled_color);
  }
  if (trailing_scroll_button_) {
    views::SetImageFromVectorIconWithColor(
        trailing_scroll_button_, kTrailingScrollIcon, foreground_enabled_color,
        foreground_disabled_color);
  }
  overflow_indicator_strategy_->FrameColorsChanged();
}

void TabStripScrollContainer::MaybeUpdateScrollButtonState() {
  if (trailing_scroll_button_) {
    if (scroll_view_->GetVisibleRect().right() ==
        scroll_view_->contents()->GetLocalBounds().right()) {
      trailing_scroll_button_->SetEnabled(false);
    } else {
      trailing_scroll_button_->SetEnabled(true);
    }
  }

  if (leading_scroll_button_) {
    if (scroll_view_->GetVisibleRect().x() ==
        scroll_view_->contents()->GetLocalBounds().x()) {
      leading_scroll_button_->SetEnabled(false);
    } else {
      leading_scroll_button_->SetEnabled(true);
    }
  }
}

bool TabStripScrollContainer::IsRectInWindowCaption(const gfx::Rect& rect) {
  const auto get_target_rect = [&](views::View* target) {
    gfx::RectF rect_in_target_coords_f(rect);
    View::ConvertRectToTarget(this, target, &rect_in_target_coords_f);
    return gfx::ToEnclosingRect(rect_in_target_coords_f);
  };

  if (leading_scroll_button_ &&
      leading_scroll_button_->GetLocalBounds().Intersects(
          get_target_rect(leading_scroll_button_))) {
    return !leading_scroll_button_->HitTestRect(
        get_target_rect(leading_scroll_button_));
  }

  if (trailing_scroll_button_ &&
      trailing_scroll_button_->GetLocalBounds().Intersects(
          get_target_rect(trailing_scroll_button_))) {
    return !trailing_scroll_button_->HitTestRect(
        get_target_rect(trailing_scroll_button_));
  }

  if (scroll_view_->GetLocalBounds().Intersects(
          get_target_rect(scroll_view_))) {
    return tab_strip()->IsRectInWindowCaption(get_target_rect(tab_strip()));
  }

  return true;
}

void TabStripScrollContainer::OnThemeChanged() {
  View::OnThemeChanged();
  FrameColorsChanged();
}

void TabStripScrollContainer::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(
          base::BindRepeating(&TabStripScrollContainer::FrameColorsChanged,
                              base::Unretained(this)));
}

void TabStripScrollContainer::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

BEGIN_METADATA(TabStripScrollContainer)
ADD_READONLY_PROPERTY_METADATA(int, TabStripAvailableWidth)
END_METADATA
