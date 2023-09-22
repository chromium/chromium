// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/frame/window_frame_util.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_scroll_container.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/border.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/metrics/histogram_functions.h"
#endif

namespace {

// TODO (1451400): This const should replace
// TABSTRIP_REGION_VIEW_CONTROL_PADDING once ChromeRefresh launched.
constexpr int kCRtabstripRegionViewControlPadding = 6;

class FrameGrabHandle : public views::View {
 public:
  METADATA_HEADER(FrameGrabHandle);
  gfx::Size CalculatePreferredSize() const override {
    // Reserve some space for the frame to be grabbed by, even if the tabstrip
    // is full.
    // TODO(tbergquist): Define this relative to the NTB insets again.
    return gfx::Size(42, 0);
  }
};

BEGIN_METADATA(FrameGrabHandle, views::View)
END_METADATA

}  // namespace

TabStripRegionView::TabStripRegionView(std::unique_ptr<TabStrip> tab_strip)
    : render_tab_search_before_tab_strip_(
          TabSearchBubbleHost::ShouldTabSearchRenderBeforeTabStrip()),
      render_new_tab_button_over_tab_strip_(features::IsChromeRefresh2023()) {
  views::SetCascadingColorProviderColor(
      this, views::kCascadingBackgroundColor,
      kColorTabBackgroundInactiveFrameInactive);

  layout_manager_ = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal);

  tab_strip_ = tab_strip.get();
  const Browser* browser = tab_strip_->GetBrowser();

  // Add and configure the TabSearchButton.
  std::unique_ptr<TabSearchButton> tab_search_button;
  if (browser && browser->is_type_normal()) {
    tab_search_button = std::make_unique<TabSearchButton>(tab_strip_);
    tab_search_button->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_SEARCH));
    tab_search_button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_SEARCH));
    tab_search_button->SetProperty(views::kCrossAxisAlignmentKey,
                                   views::LayoutAlignment::kCenter);
  }

  if (tab_search_button && render_tab_search_before_tab_strip_) {
    tab_search_button->SetPaintToLayer();
    tab_search_button->layer()->SetFillsBoundsOpaquely(false);

    tab_search_button_ = AddChildView(std::move(tab_search_button));
    // Inset between the tabsearch and tabstrip should be reduced to account for
    // extra spacing.
    layout_manager_->SetChildViewIgnoredByLayout(tab_search_button_, true);
  }

  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
    std::unique_ptr<TabStripScrollContainer> scroll_container =
        std::make_unique<TabStripScrollContainer>(std::move(tab_strip));
    tab_strip_scroll_container_ = scroll_container.get();
    tab_strip_container_ = AddChildView(std::move(scroll_container));
    // Allow the |tab_strip_container_| to grow into the free space available in
    // the TabStripRegionView.
    const views::FlexSpecification tab_strip_container_flex_spec =
        views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                 views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred);
    tab_strip_container_->SetProperty(views::kFlexBehaviorKey,
                                      tab_strip_container_flex_spec);

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

  reserved_grab_handle_space_ =
      AddChildView(std::make_unique<FrameGrabHandle>());
  reserved_grab_handle_space_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(3));

  SetProperty(views::kElementIdentifierKey, kTabStripRegionElementId);

#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kChromeOSTabSearchCaptionButton))
    return;
#endif

  if (browser && tab_search_button &&
      !WindowFrameUtil::IsWin10TabSearchCaptionButtonEnabled(browser) &&
      !render_tab_search_before_tab_strip_) {
    tab_search_button_ = AddChildView(std::move(tab_search_button));
    if (features::IsChromeRefresh2023()) {
      tab_search_button_->SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(0, 0, kCRtabstripRegionViewControlPadding,
                            kCRtabstripRegionViewControlPadding));
    } else {
      const auto control_padding = gfx::Insets::TLBR(
          0, 0, 0, GetLayoutConstant(TABSTRIP_REGION_VIEW_CONTROL_PADDING));

      tab_search_button_->SetProperty(views::kMarginsKey, control_padding);
    }
  }

  //  If the new tab button or tab search button are positioned over the
  //  tabstrip, then buttons are rendered to a layer, and the margins are set to
  //  take up the rest of the space under the buttons.
  absl::optional<int> tab_strip_right_margin;
  if (render_new_tab_button_over_tab_strip_) {
    new_tab_button_->SetPaintToLayer();
    new_tab_button_->layer()->SetFillsBoundsOpaquely(false);
    // Inset between the tabstrip and new tab button should be reduced to
    // account for extra spacing.
    layout_manager_->SetChildViewIgnoredByLayout(new_tab_button_, true);

    tab_strip_right_margin = new_tab_button_->GetPreferredSize().width() +
                             kCRtabstripRegionViewControlPadding;
  } else {
    UpdateNewTabButtonBorder();
  }

  absl::optional<int> tab_strip_left_margin;
  if (tab_search_button_ && render_tab_search_before_tab_strip_) {
    // The `tab_search_button_` is being laid out manually.
    CHECK(layout_manager_->IsChildViewIgnoredByLayout(tab_search_button_));

    // Add a margin to the tab_strip_container_ to leave the correct amount of
    // space for the `tab_search_button_`.
    gfx::Size tab_search_button_size = tab_search_button_->GetPreferredSize();

    tab_strip_left_margin =
        tab_search_button_size.width() + kCRtabstripRegionViewControlPadding;
  }

  if (tab_strip_left_margin.has_value() || tab_strip_right_margin.has_value()) {
    tab_strip_container_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, tab_strip_left_margin.value_or(0), 0,
                          tab_strip_right_margin.value_or(0)));
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
  // tab strip's |IsRectInWindowCaption()| for scrolling disabled. Defer to
  // scroll container if scrolling is enabled.
  // TODO(tluk): Address edge case where |rect| might partially intersect with
  // the |tab_strip_container_| and the |tab_strip_| but not over the same
  // pixels. This could lead to this returning false when it should be returning
  // true.

  if (tab_strip_container_->HitTestRect(
          get_target_rect(tab_strip_container_))) {
    if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
      TabStripScrollContainer* scroll_container =
          views::AsViewClass<TabStripScrollContainer>(tab_strip_container_);

      return scroll_container->IsRectInWindowCaption(
          get_target_rect(scroll_container));

    } else {
      return tab_strip_->IsRectInWindowCaption(get_target_rect(tab_strip_));
    }
  }

  // The child could have a non-rectangular shape, so if the rect is not in the
  // visual portions of the child view we treat it as a click to the caption.
  for (View* const child : children()) {
    if (child != tab_strip_container_ && child != reserved_grab_handle_space_ &&
        child->GetLocalBounds().Intersects(get_target_rect(child))) {
      return !child->HitTestRect(get_target_rect(child));
    }
  }

#if BUILDFLAG(IS_WIN)
  bool rect_in_reserved_space =
      reserved_grab_handle_space_->GetLocalBounds().Intersects(
          get_target_rect(reserved_grab_handle_space_));
  ReportCaptionHitTestInReservedGrabHandleSpace(rect_in_reserved_space);
#endif

  return true;
}

bool TabStripRegionView::IsPositionInWindowCaption(const gfx::Point& point) {
  return IsRectInWindowCaption(gfx::Rect(point, gfx::Size(1, 1)));
}

views::View::Views TabStripRegionView::GetChildrenInZOrder() {
  views::View::Views children;

  if (tab_strip_container_) {
    children.emplace_back(tab_strip_container_);
  }

  if (new_tab_button_) {
    children.emplace_back(new_tab_button_);
  }

  if (tab_search_button_) {
    children.emplace_back(tab_search_button_);
  }

  if (reserved_grab_handle_space_) {
    children.emplace_back(reserved_grab_handle_space_);
  }

  return children;
}

// The TabSearchButton need bounds that overlap the TabStripContainer, which
// FlexLayout doesn't currently support. Because of this the TSB bounds are
// manually calculated.
void TabStripRegionView::Layout() {
  views::AccessiblePaneView::Layout();

  if (tab_search_button_ && render_tab_search_before_tab_strip_) {
    const gfx::Size tab_search_button_size =
        tab_search_button_->GetPreferredSize();

    const int x = tab_strip_container_->x() - tab_search_button_size.width() +
                  kCRtabstripRegionViewControlPadding;

    // The y position is measured from the bottom of the tabstrip, and then
    // pading and button height are removed.
    const int y = tab_strip_container_->y() + tab_strip_container_->height() -
                  kCRtabstripRegionViewControlPadding -
                  tab_search_button_size.height();

    const gfx::Rect tab_search_new_bounds =
        gfx::Rect(gfx::Point(x, y), tab_search_button_size);

    tab_search_button_->SetBoundsRect(tab_search_new_bounds);
  }

  if (render_new_tab_button_over_tab_strip_) {
    gfx::Size new_tab_button_size = new_tab_button_->GetPreferredSize();

    gfx::Point new_tab_button_new_position = gfx::Point(
        tab_strip_container_->x() + tab_strip_container_->width() -
            TabStyle::Get()->GetBottomCornerRadius() +
            kCRtabstripRegionViewControlPadding,
        tab_strip_container_->y() + tab_strip_container_->height() -
            kCRtabstripRegionViewControlPadding - new_tab_button_size.height());

    gfx::Rect new_tab_button_new_bounds =
        gfx::Rect(new_tab_button_new_position, new_tab_button_size);

    // If the tabsearch button is before the tabstrip container, then manually
    // set the margins to the correct position.
    new_tab_button_->SetBoundsRect(new_tab_button_new_bounds);
  }
}

bool TabStripRegionView::CanDrop(const OSExchangeData& data) {
  return TabDragController::IsSystemDragAndDropSessionRunning() &&
         data.HasCustomFormat(
             ui::ClipboardFormatType::GetType(ui::kMimeTypeWindowDrag));
}

bool TabStripRegionView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  format_types->insert(
      ui::ClipboardFormatType::GetType(ui::kMimeTypeWindowDrag));
  return true;
}

void TabStripRegionView::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(TabDragController::IsSystemDragAndDropSessionRunning());
  TabDragController::OnSystemDragAndDropUpdated(event);
}

int TabStripRegionView::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(TabDragController::IsSystemDragAndDropSessionRunning());
  TabDragController::OnSystemDragAndDropUpdated(event);
  return ui::DragDropTypes::DRAG_MOVE;
}

void TabStripRegionView::OnDragExited() {
  DCHECK(TabDragController::IsSystemDragAndDropSessionRunning());
  TabDragController::OnSystemDragAndDropExited();
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

views::View* TabStripRegionView::GetDefaultFocusableChild() {
  auto* focusable_child = tab_strip_->GetDefaultFocusableChild();
  return focusable_child ? focusable_child
                         : AccessiblePaneView::GetDefaultFocusableChild();
}

// static
void TabStripRegionView::ReportCaptionHitTestInReservedGrabHandleSpace(
    bool in_reserved_grab_handle_space) {
#if BUILDFLAG(IS_WIN)
  static bool button_down_previously = false;
  int primary_mouse_button =
      ::GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
  bool button_down_now =
      (::GetAsyncKeyState(primary_mouse_button) & 0x8000) != 0;
  if (button_down_now && !button_down_previously) {
    base::UmaHistogramBoolean(
        "Chrome.Frame.MouseDownCaptionHitTestInReservedGrabHandleSpace",
        in_reserved_grab_handle_space);
  }
  button_down_previously = button_down_now;
#endif
}

void TabStripRegionView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTabList;
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
      gfx::Insets::TLBR(extra_vertical_space / 2, 0, 0, kHorizontalInset)));
}

BEGIN_METADATA(TabStripRegionView, views::AccessiblePaneView)
END_METADATA
