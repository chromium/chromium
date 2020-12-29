// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include "base/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

// Define a custom FlexRule for |tabstrip_scroll_container_|. Equivalent to
// using a (kScaleToMinimum, kPreferred) flex specification on the tabstrip
// itself, bypassing the ScrollView.
// TODO(1132488): Make ScrollView take on TabStrip's preferred size instead.
gfx::Size TabScrollContainerFlexRule(const views::View* tab_strip,
                                     const views::View* view,
                                     const views::SizeBounds& size_bounds) {
  const gfx::Size preferred_size = tab_strip->GetPreferredSize();
  return gfx::Size(size_bounds.width().min_of(preferred_size.width()),
                   preferred_size.height());
}

std::unique_ptr<views::ImageButton> CreateScrollButton(
    views::Button::PressedCallback callback) {
  auto scroll_button =
      std::make_unique<views::ImageButton>(std::move(callback));
  scroll_button->SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  scroll_button->SetHasInkDropActionOnClick(true);
  scroll_button->SetInkDropMode(views::Button::InkDropMode::ON);
  scroll_button->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  return scroll_button;
}

class FrameGrabHandle : public views::View {
 public:
  gfx::Size CalculatePreferredSize() const override {
    // Reserve some space for the frame to be grabbed by, even if the tabstrip
    // is full.
    // TODO(tbergquist): Define this relative to the NTB insets again.
    return gfx::Size(42, 0);
  }
};

}  // namespace

TabStripRegionView::TabStripRegionView(std::unique_ptr<TabStrip> tab_strip) {
  layout_manager_ = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal);

  tab_strip_ = tab_strip.get();
  tab_strip->SetAvailableWidthCallback(
      base::BindRepeating(&TabStripRegionView::CalculateTabStripAvailableWidth,
                          base::Unretained(this)));
  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
    // TODO(https://crbug.com/1132488): ScrollView doesn't propagate changes to
    // the TabStrip's preferred size; observe that manually.
    tab_strip->View::AddObserver(this);

    views::ScrollView* tab_strip_scroll_container =
        AddChildView(std::make_unique<views::ScrollView>(
            views::ScrollView::ScrollWithLayers::kEnabled));
    tab_strip_scroll_container->SetBackgroundColor(base::nullopt);
    tab_strip_scroll_container->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kHiddenButEnabled);
    tab_strip_scroll_container->SetTreatAllScrollEventsAsHorizontal(true);
    tab_strip_container_ = tab_strip_scroll_container;
    tab_strip_scroll_container->SetContents(std::move(tab_strip));
    tab_strip_scroll_container->SetDrawOverflowIndicator(true);
    // This base::Unretained is safe because the callback is called by the
    // layout manager, which is cleaned up before view children like
    // |tab_strip_scroll_container| (which owns |tab_strip_|).
    tab_strip_scroll_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(base::BindRepeating(
            &TabScrollContainerFlexRule, base::Unretained(tab_strip_))));
  } else {
    tab_strip_container_ = AddChildView(std::move(tab_strip));

    // Allow the |tab_strip_container_| to grow into the free space available in
    // the TabStripRegionView.
    const views::FlexSpecification tab_strip_container_flex_spec =
        views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                 views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kPreferred);
    tab_strip_container_->SetProperty(views::kFlexBehaviorKey,
                                      tab_strip_container_flex_spec);
  }

  new_tab_button_ = AddChildView(std::make_unique<NewTabButton>(
      tab_strip_, base::BindRepeating(&TabStrip::NewTabButtonPressed,
                                      base::Unretained(tab_strip_))));
  new_tab_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_NEW_TAB));
  new_tab_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_NEWTAB));
  new_tab_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_BOTTOM);
  new_tab_button_->SetEventTargeter(
      std::make_unique<views::ViewTargeter>(new_tab_button_));

  UpdateNewTabButtonBorder();

  if (base::FeatureList::IsEnabled(features::kScrollableTabStripButtons)) {
    leading_scroll_button_ = AddChildView(CreateScrollButton(
        base::BindRepeating(&TabStripRegionView::ScrollTowardsLeadingTab,
                            base::Unretained(this))));
    trailing_scroll_button_ = AddChildView(CreateScrollButton(
        base::BindRepeating(&TabStripRegionView::ScrollTowardsTrailingTab,
                            base::Unretained(this))));
  }

  reserved_grab_handle_space_ =
      AddChildView(std::make_unique<FrameGrabHandle>());
  reserved_grab_handle_space_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));

  const Browser* browser = tab_strip_->controller()->GetBrowser();
  if (base::FeatureList::IsEnabled(features::kTabSearch) && browser &&
      browser->is_type_normal()) {
    auto tab_search_button = std::make_unique<TabSearchButton>(tab_strip_);
    tab_search_button->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_SEARCH));
    tab_search_button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_SEARCH));
    tab_search_button->SetProperty(views::kCrossAxisAlignmentKey,
                                   views::LayoutAlignment::kCenter);
    tab_search_button_ = AddChildView(std::move(tab_search_button));

    // Add the margin necessary to ensure correct spacing between right-aligned
    // controls and the end of the TabStripRegionView.
    layout_manager_->SetInteriorMargin(gfx::Insets(
        0, 0, 0, GetLayoutConstant(TABSTRIP_REGION_VIEW_CONTROL_PADDING)));
  }
}

TabStripRegionView::~TabStripRegionView() = default;

bool TabStripRegionView::IsRectInWindowCaption(const gfx::Rect& rect) {
  const auto get_target_rect = [&](views::View* target) {
    gfx::RectF rect_in_target_coords_f(rect);
    View::ConvertRectToTarget(this, target, &rect_in_target_coords_f);
    return gfx::ToEnclosingRect(rect_in_target_coords_f);
  };

  // Perform a hit test against the |tab_strip_container_| to ensure that the
  // rect is within the visible portion of the |tab_strip_| before calling the
  // tab strip's |IsRectInWindowCaption()|.
  // TODO(tluk): Address edge case where |rect| might partially intersect with
  // the |tab_strip_container_| and the |tab_strip_| but not over the same
  // pixels. This could lead to this returning false when it should be returning
  // true.
  if (tab_strip_container_->HitTestRect(get_target_rect(tab_strip_container_)))
    return tab_strip_->IsRectInWindowCaption(get_target_rect(tab_strip_));

  // The child could have a non-rectangular shape, so if the rect is not in the
  // visual portions of the child view we treat it as a click to the caption.
  for (View* const child : children()) {
    if (child != tab_strip_container_ && child != reserved_grab_handle_space_ &&
        child->GetLocalBounds().Intersects(get_target_rect(child))) {
      return !child->HitTestRect(get_target_rect(child));
    }
  }

  return true;
}

bool TabStripRegionView::IsPositionInWindowCaption(const gfx::Point& point) {
  return IsRectInWindowCaption(gfx::Rect(point, gfx::Size(1, 1)));
}

void TabStripRegionView::FrameColorsChanged() {
  new_tab_button_->FrameColorsChanged();
  if (tab_search_button_)
    tab_search_button_->FrameColorsChanged();
  if (base::FeatureList::IsEnabled(features::kScrollableTabStripButtons)) {
    const SkColor background_color = tab_strip_->GetTabBackgroundColor(
        TabActive::kInactive, BrowserFrameActiveState::kUseCurrent);
    SkColor foreground_color = tab_strip_->GetTabForegroundColor(
        TabActive::kInactive, background_color);
    views::SetImageFromVectorIconWithColor(leading_scroll_button_,
                                           kScrollingTabstripLeadingIcon,
                                           foreground_color);
    views::SetImageFromVectorIconWithColor(trailing_scroll_button_,
                                           kScrollingTabstripTrailingIcon,
                                           foreground_color);
  }
  tab_strip_->FrameColorsChanged();
  SchedulePaint();
}

const char* TabStripRegionView::GetClassName() const {
  return "TabStripRegionView";
}

void TabStripRegionView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

gfx::Size TabStripRegionView::GetMinimumSize() const {
  gfx::Size tab_strip_min_size = tab_strip_->GetMinimumSize();
  // Cap the tabstrip minimum width to a reasonable value so browser windows
  // aren't forced to grow arbitrarily wide.
  const int max_min_width = 520;
  tab_strip_min_size.set_width(
      std::min(max_min_width, tab_strip_min_size.width()));
  return tab_strip_min_size;
}

void TabStripRegionView::OnThemeChanged() {
  View::OnThemeChanged();
  FrameColorsChanged();
}

views::View* TabStripRegionView::GetDefaultFocusableChild() {
  auto* focusable_child = tab_strip_->GetDefaultFocusableChild();
  return focusable_child ? focusable_child
                         : AccessiblePaneView::GetDefaultFocusableChild();
}

void TabStripRegionView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTabList;
}

void TabStripRegionView::OnViewPreferredSizeChanged(View* view) {
  DCHECK_EQ(view, tab_strip_);

  // The |tab_strip_|'s preferred size changing can change our own preferred
  // size; however, with scrolling enabled, the ScrollView does not propagate
  // ChildPreferredSizeChanged up the view hierarchy, instead assuming that its
  // own preferred size is independent of its childrens'.
  // TODO(https://crbug.com/1132488): Make ScrollView not be like that.
  PreferredSizeChanged();
}

int TabStripRegionView::CalculateTabStripAvailableWidth() {
  // The tab strip can occupy the space not currently taken by its fixed-width
  // sibling views.
  int reserved_width = 0;
  for (View* const child : children()) {
    if (child != tab_strip_container_)
      reserved_width += child->GetMinimumSize().width();
  }

  return size().width() - reserved_width -
         layout_manager_->interior_margin().width();
}

void TabStripRegionView::ScrollTowardsLeadingTab() {
  views::ScrollView* scroll_view_container =
      static_cast<views::ScrollView*>(tab_strip_container_);
  gfx::Rect visible_content = scroll_view_container->GetVisibleRect();
  gfx::Rect scroll(visible_content.x() - visible_content.width(),
                   visible_content.y(), visible_content.width(),
                   visible_content.height());
  scroll_view_container->contents()->ScrollRectToVisible(scroll);
}

void TabStripRegionView::ScrollTowardsTrailingTab() {
  views::ScrollView* scroll_view_container =
      static_cast<views::ScrollView*>(tab_strip_container_);
  gfx::Rect visible_content = scroll_view_container->GetVisibleRect();
  gfx::Rect scroll(visible_content.x() + visible_content.width(),
                   visible_content.y(), visible_content.width(),
                   visible_content.height());
  scroll_view_container->contents()->ScrollRectToVisible(scroll);
}

void TabStripRegionView::UpdateNewTabButtonBorder() {
  const int extra_vertical_space = GetLayoutConstant(TAB_HEIGHT) -
                                   GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) -
                                   NewTabButton::kButtonSize.height();
  constexpr int kHorizontalInset = 8;
  // The new tab button is placed vertically exactly in the center of the
  // tabstrip. Extend the border of the button such that it extends to the top
  // of the tabstrip bounds. This is essential to ensure it is targetable on the
  // edge of the screen when in fullscreen mode and ensures the button abides
  // by the correct Fitt's Law behavior (https://crbug.com/1136557).
  // TODO(crbug.com/1142016): The left border is 0 in order to abut the NTB
  // directly with the tabstrip. That's the best immediately available
  // approximation to the prior behavior of aligning the NTB relative to the
  // trailing separator (instead of the right bound of the trailing tab). This
  // still isn't quite what we ideally want in the non-scrolling case, and
  // definitely isn't what we want in the scrolling case, so this naive approach
  // should be improved, likely by taking the scroll state of the tabstrip into
  // account.
  new_tab_button_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(extra_vertical_space / 2, 0, 0, kHorizontalInset)));
}
