// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_scroll_container.h"
#include <memory>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_shader.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
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

namespace {
// Define a custom FlexRule for |scroll_view_|. Equivalent to using a
// (kScaleToMinimum, kPreferred) flex specification on the tabstrip itself,
// bypassing the ScrollView.
// TODO(1132488): Make ScrollView take on TabStrip's preferred size instead.
gfx::Size TabScrollContainerFlexRule(const views::View* tab_strip,
                                     const views::View* view,
                                     const views::SizeBounds& size_bounds) {
  const gfx::Size preferred_size = tab_strip->GetPreferredSize();
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

// A customized overflow indicator that paints a shadow-like gradient over the
// tabstrip.
class TabStripContainerOverflowIndicator : public views::View {
 public:
  METADATA_HEADER(TabStripContainerOverflowIndicator);
  TabStripContainerOverflowIndicator(TabStrip* tab_strip,
                                     views::OverflowIndicatorAlignment side)
      : tab_strip_(tab_strip), side_(side) {
    DCHECK(side_ == views::OverflowIndicatorAlignment::kLeft ||
           side_ == views::OverflowIndicatorAlignment::kRight);
  }

  // Making this smaller than the margin provided by the leftmost/rightmost
  // tab's tail (TabStyle::kTabOverlap / 2) makes the transition in and out of
  // the scroll state smoother.
  static constexpr int kOpaqueWidth = 8;
  // The width of the full opacity part of the shadow.
  static constexpr int kShadowSpread = 1;
  // The width of the soft edge of the shadow.
  static constexpr int kShadowBlur = 3;
  static constexpr int kTotalWidth = kOpaqueWidth + kShadowSpread + kShadowBlur;

  // views::View overrides:
  void OnPaint(gfx::Canvas* canvas) override {
    // TODO(tbergquist): Handle themes with titlebar background images.
    // TODO(crbug/1308932): Remove FromColor and make all SkColor4f.
    SkColor4f frame_color =
        SkColor4f::FromColor(tab_strip_->controller()->GetFrameColor(
            BrowserFrameActiveState::kUseCurrent));
    SkColor4f shadow_color = SkColor4f::FromColor(
        GetColorProvider()->GetColor(ui::kColorShadowBase));

    // Mirror how the indicator is painted for the right vs left sides.
    SkPoint points[2];
    if (side_ == views::OverflowIndicatorAlignment::kLeft) {
      points[0].iset(GetContentsBounds().origin().x(), GetContentsBounds().y());
      points[1].iset(GetContentsBounds().right(), GetContentsBounds().y());
    } else {
      points[0].iset(GetContentsBounds().right(), GetContentsBounds().y());
      points[1].iset(GetContentsBounds().origin().x(), GetContentsBounds().y());
    }

    SkColor4f colors[5];
    SkScalar color_positions[5];
    // Paint an opaque region on the outside.
    colors[0] = frame_color;
    colors[1] = frame_color;
    color_positions[0] = 0;
    color_positions[1] = static_cast<float>(kOpaqueWidth) / kTotalWidth;

    // Paint a shadow-like gradient on the inside.
    colors[2] = shadow_color;
    colors[3] = shadow_color;
    colors[4] = shadow_color;
    colors[4].fA = 0.0f;
    color_positions[2] = static_cast<float>(kOpaqueWidth) / kTotalWidth;
    color_positions[3] =
        static_cast<float>(kOpaqueWidth + kShadowSpread) / kTotalWidth;
    color_positions[4] = 1;

    cc::PaintFlags flags;
    flags.setShader(cc::PaintShader::MakeLinearGradient(
        points, colors, color_positions, 5, SkTileMode::kClamp));
    canvas->DrawRect(GetContentsBounds(), flags);
  }

 private:
  raw_ptr<TabStrip> tab_strip_;
  views::OverflowIndicatorAlignment side_;
};

BEGIN_METADATA(TabStripContainerOverflowIndicator, views::View)
END_METADATA

// Must be kept the same as kTabScrollingButtonPositionVariations values
enum ScrollButtonPositionType {
  kJoinedButtonsRight = 0,
  kJoinedButtonsLeft = 1,
  kSplitButtons = 2
};

}  // namespace

TabStripScrollContainer::TabStripScrollContainer(
    std::unique_ptr<TabStrip> tab_strip)
    : tab_strip_(tab_strip.get()) {
  SetLayoutManager(std::make_unique<views::FillLayout>())
      ->SetMinimumSizeEnabled(true);

  // TODO(https://crbug.com/1132488): ScrollView doesn't propagate changes to
  // the TabStrip's preferred size; observe that manually.
  tab_strip->View::AddObserver(this);
  tab_strip->SetAvailableWidthCallback(
      base::BindRepeating(&TabStripScrollContainer::GetTabStripAvailableWidth,
                          base::Unretained(this)));

  std::unique_ptr<views::ScrollView> scroll_view =
      std::make_unique<views::ScrollView>(
          views::ScrollView::ScrollWithLayers::kEnabled);
  scroll_view_ = scroll_view.get();
  scroll_view->SetBackgroundColor(absl::nullopt);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view->SetTreatAllScrollEventsAsHorizontal(true);
  scroll_view->SetContents(std::move(tab_strip));

  scroll_view->SetDrawOverflowIndicator(true);
  left_overflow_indicator_ = scroll_view->SetCustomOverflowIndicator(
      views::OverflowIndicatorAlignment::kLeft,
      std::make_unique<TabStripContainerOverflowIndicator>(
          tab_strip_, views::OverflowIndicatorAlignment::kLeft),
      TabStripContainerOverflowIndicator::kTotalWidth, false);
  right_overflow_indicator_ = scroll_view->SetCustomOverflowIndicator(
      views::OverflowIndicatorAlignment::kRight,
      std::make_unique<TabStripContainerOverflowIndicator>(
          tab_strip_, views::OverflowIndicatorAlignment::kRight),
      TabStripContainerOverflowIndicator::kTotalWidth, false);

  // This base::Unretained is safe because the callback is called by the
  // layout manager, which is cleaned up before view children like
  // |scroll_view| (which owns |tab_strip_|).
  scroll_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(base::BindRepeating(
          &TabScrollContainerFlexRule, base::Unretained(tab_strip_))));

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
  leading_scroll_button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_SCROLL_LEADING));

  std::unique_ptr<views::ImageButton> trailing_scroll_button =
      CreateScrollButton(base::BindRepeating(
          &TabStripScrollContainer::ScrollTowardsTrailingTab,
          base::Unretained(this)));
  trailing_scroll_button->SetAccessibleName(
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
  DCHECK_EQ(tab_strip_, view);

  PreferredSizeChanged();
}

void TabStripScrollContainer::OnContentsScrolledCallback() {
  views::Widget* root_widget = tab_strip_->GetWidget();
  std::set<views::Widget*> children_widgets;
  views::Widget::GetAllOwnedWidgets(root_widget->GetNativeView(),
                                    &children_widgets);

  for (auto* child_widget : children_widgets) {
    views::BubbleDialogDelegate* bdd =
        child_widget->widget_delegate()->AsBubbleDialogDelegate();
    if (bdd) {
      views::View* anchor_view = bdd->GetAnchorView();
      if (this->Contains(anchor_view)) {
        child_widget->Hide();
      }
    }
  }
}

int TabStripScrollContainer::GetTabStripAvailableWidth() const {
  return overflow_view_->GetAvailableSize(scroll_view_).width().value();
}

void TabStripScrollContainer::ScrollTowardsLeadingTab() {
  gfx::Rect visible_content = scroll_view_->GetVisibleRect();
  tab_strip_->ScrollTowardsLeadingTabs(visible_content.width());
}

void TabStripScrollContainer::ScrollTowardsTrailingTab() {
  gfx::Rect visible_content = scroll_view_->GetVisibleRect();
  tab_strip_->ScrollTowardsTrailingTabs(visible_content.width());
}

void TabStripScrollContainer::FrameColorsChanged() {
  SkColor foreground_color =
      tab_strip_->GetTabForegroundColor(TabActive::kInactive);
  /* Use placeholder color for disabled state because these buttons should
     never be disabled (they are hidden when the tab strip is not full) */
  if (leading_scroll_button_) {
    views::SetImageFromVectorIconWithColor(leading_scroll_button_,
                                           kLeadingScrollIcon, foreground_color,
                                           gfx::kPlaceholderColor);
  }
  if (trailing_scroll_button_) {
    views::SetImageFromVectorIconWithColor(
        trailing_scroll_button_, kTrailingScrollIcon, foreground_color,
        gfx::kPlaceholderColor);
  }
  left_overflow_indicator_->SchedulePaint();
  right_overflow_indicator_->SchedulePaint();
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
    return tab_strip_->IsRectInWindowCaption(get_target_rect(tab_strip_));
  }

  return true;
}

void TabStripScrollContainer::OnThemeChanged() {
  View::OnThemeChanged();
  FrameColorsChanged();
}

BEGIN_METADATA(TabStripScrollContainer, views::View)
ADD_READONLY_PROPERTY_METADATA(int, TabStripAvailableWidth)
END_METADATA
