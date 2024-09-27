// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_coordinator.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/toolbar/back_forward_button.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_content_settings_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_navigation_button_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/window_controls_overlay_toggle_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_utils.h"
#include "ui/views/window/hit_test_utils.h"

WebAppFrameToolbarView::WebAppFrameToolbarView(BrowserView* browser_view)
    : browser_view_(browser_view) {
  DCHECK(browser_view_);
  DCHECK(web_app::AppBrowserController::IsWebApp(browser_view_->browser()));
  SetID(VIEW_ID_WEB_APP_FRAME_TOOLBAR);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  {
    // TODO(tluk) fix the need for both LayoutInContainer() and a layout
    // manager for frame layout.
    views::FlexLayout* layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kHorizontal);
    layout->SetMainAxisAlignment(views::LayoutAlignment::kEnd);
    layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  }

  const auto* app_controller = browser_view_->browser()->app_controller();

  if (app_controller->HasMinimalUiButtons()) {
    left_container_ =
        AddChildView(std::make_unique<WebAppNavigationButtonContainer>(
            browser_view_, /*toolbar_button_provider=*/this));
    left_container_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(
            views::LayoutOrientation::kHorizontal,
            views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero)
            .WithOrder(2));
  }

  center_container_ = AddChildView(std::make_unique<views::View>());
  center_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(3));

  right_container_ = AddChildView(
      std::make_unique<WebAppToolbarButtonContainer>(browser_view, this));
  right_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(right_container_->GetFlexRule()).WithOrder(1));

  UpdateStatusIconsVisibility();

  DCHECK(
      !browser_view_->toolbar_button_provider() ||
      views::IsViewClass<WebAppFrameToolbarView>(
          browser_view_->toolbar_button_provider()->GetAsAccessiblePaneView()))
      << "This should be the first ToolbarButtorProvider or a replacement for "
         "an existing instance of this class during a window frame refresh.";
  browser_view_->SetToolbarButtonProvider(this);

  if (browser_view_->IsWindowControlsOverlayEnabled()) {
    OnWindowControlsOverlayEnabledChanged();
  }
  if (browser_view_->AppUsesBorderlessMode()) {
    UpdateBorderlessModeEnabled();
  }
}

WebAppFrameToolbarView::~WebAppFrameToolbarView() = default;

void WebAppFrameToolbarView::UpdateStatusIconsVisibility() {
  right_container_->UpdateStatusIconsVisibility();
}

void WebAppFrameToolbarView::UpdateCaptionColors() {
  // We want to behave differently if this is an update to the color (as opposed
  // to the first time it's being set). Specifically, updates should pulse the
  // origin into view.
  bool color_previously_set = active_background_color_.has_value();
  if (color_previously_set) {
    SkColor old_active_background_color = *active_background_color_;
    SkColor old_active_foreground_color = *active_foreground_color_;
    SkColor old_inactive_background_color = *inactive_background_color_;
    SkColor old_inactive_foreground_color = *inactive_foreground_color_;
    UpdateCachedColors();

    if (old_active_background_color == active_background_color_ &&
        old_active_foreground_color == active_foreground_color_ &&
        old_inactive_background_color == inactive_background_color_ &&
        old_inactive_foreground_color == inactive_foreground_color_) {
      return;
    }
  } else {
    UpdateCachedColors();
  }

  UpdateChildrenColor(/*color_changed=*/color_previously_set);
}

void WebAppFrameToolbarView::SetPaintAsActive(bool active) {
  if (paint_as_active_ == active) {
    return;
  }
  paint_as_active_ = active;
  UpdateChildrenColor(/*color_changed=*/false);
  OnPropertyChanged(&paint_as_active_, views::kPropertyEffectsNone);
}

bool WebAppFrameToolbarView::GetPaintAsActive() const {
  return paint_as_active_;
}

std::pair<int, int> WebAppFrameToolbarView::LayoutInContainer(
    int leading_x,
    int trailing_x,
    int y,
    int available_height) {
  gfx::Rect center_bounds = LayoutInContainer(gfx::Rect(
      leading_x, y, base::ClampSub(trailing_x, leading_x), available_height));
  return std::pair<int, int>(center_bounds.x(), center_bounds.right());
}

gfx::Rect WebAppFrameToolbarView::LayoutInContainer(gfx::Rect available_space) {
  DCHECK(!browser_view_->IsWindowControlsOverlayEnabled());

  SetVisible(!available_space.IsEmpty());
  if (available_space.IsEmpty()) {
    SetSize(gfx::Size());
    return gfx::Rect();
  }

  DCHECK_LE(GetPreferredSize().height(), available_space.height());
  SetBoundsRect(available_space);
  DeprecatedLayoutImmediately();

  if (!center_container_->GetVisible()) {
    return gfx::Rect();
  }

  // Bounds for remaining inner space, in parent container coordinates.
  gfx::Rect center_bounds = center_container_->bounds();
  DCHECK(center_bounds.x() == 0 || left_container_);
  center_bounds.Offset(bounds().OffsetFromOrigin());
  return center_bounds;
}

void WebAppFrameToolbarView::LayoutForWindowControlsOverlay(
    gfx::Rect available_space) {
  DCHECK(!left_container_);
  // The center_container_ might have been laid out by the frame view such that
  // it interferes with hit testing in the ToolbarButtonContainer. Ensure that
  // its bounds are cleared when laying out WCO.
  center_container_->SetBounds(0, 0, 0, 0);

  const int width = std::min(available_space.width(),
                             right_container_->GetPreferredSize().width());
  const int x = available_space.right() - width;
  SetBounds(x, available_space.y(), width, available_space.height());
}

ExtensionsToolbarContainer*
WebAppFrameToolbarView::GetExtensionsToolbarContainer() {
  return right_container_->extensions_container();
}

gfx::Size WebAppFrameToolbarView::GetToolbarButtonSize() const {
  const int size = GetLayoutConstant(WEB_APP_MENU_BUTTON_SIZE);
  return gfx::Size(size, size);
}

views::View* WebAppFrameToolbarView::GetDefaultExtensionDialogAnchorView() {
  ExtensionsToolbarContainer* extensions_container =
      GetExtensionsToolbarContainer();
  if (extensions_container && extensions_container->GetVisible()) {
    return extensions_container->GetExtensionsButton();
  }
  return GetAppMenuButton();
}

PageActionIconView* WebAppFrameToolbarView::GetPageActionIconView(
    PageActionIconType type) {
  return right_container_->page_action_icon_controller()->GetIconView(type);
}

AppMenuButton* WebAppFrameToolbarView::GetAppMenuButton() {
  return right_container_->web_app_menu_button();
}

gfx::Rect WebAppFrameToolbarView::GetFindBarBoundingBox(int contents_bottom) {
  if (!IsDrawn()) {
    return gfx::Rect();
  }

  // If LTR find bar will be right aligned so align to right edge of app menu
  // button. Otherwise it will be left aligned so align to the left edge of the
  // app menu button.
  views::View* anchor_view = GetAnchorView(PageActionIconType::kFind);
  gfx::Rect anchor_bounds =
      anchor_view->ConvertRectToWidget(anchor_view->GetLocalBounds());
  int x_pos = 0;
  int width = anchor_bounds.right();
  if (base::i18n::IsRTL()) {
    x_pos = anchor_bounds.x();
    width = GetWidget()->GetRootView()->width() - anchor_bounds.x();
  }
  return gfx::Rect(x_pos, anchor_bounds.bottom(), width,
                   contents_bottom - anchor_bounds.bottom());
}

void WebAppFrameToolbarView::FocusToolbar() {
  SetPaneFocus(nullptr);
}

views::AccessiblePaneView* WebAppFrameToolbarView::GetAsAccessiblePaneView() {
  return this;
}

views::View* WebAppFrameToolbarView::GetAnchorView(
    std::optional<PageActionIconType> type) {
  views::View* anchor = GetAppMenuButton();
  return anchor ? anchor : this;
}

void WebAppFrameToolbarView::ZoomChangedForActiveTab(bool can_show_bubble) {
  right_container_->page_action_icon_controller()->ZoomChangedForActiveTab(
      can_show_bubble);
}

AvatarToolbarButton* WebAppFrameToolbarView::GetAvatarToolbarButton() {
  return right_container_ ? right_container_->avatar_button() : nullptr;
}

ToolbarButton* WebAppFrameToolbarView::GetBackButton() {
  return left_container_ ? left_container_->back_button() : nullptr;
}

ReloadButton* WebAppFrameToolbarView::GetReloadButton() {
  return left_container_ ? left_container_->reload_button() : nullptr;
}

IntentChipButton* WebAppFrameToolbarView::GetIntentChipButton() {
  return nullptr;
}

DownloadToolbarButtonView* WebAppFrameToolbarView::GetDownloadButton() {
  return right_container_ ? right_container_->download_button() : nullptr;
}

bool WebAppFrameToolbarView::DoesIntersectRect(const View* target,
                                               const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  if (!views::ViewTargeterDelegate::DoesIntersectRect(this, rect)) {
    return false;
  }

  // If the rect is inside the bounds of the center_container, do not claim it.
  // There is no actionable content in the center_container, and it overlaps
  // tabs in tabbed PWA windows.
  gfx::RectF rect_in_center_container_coords_f(rect);
  View::ConvertRectToTarget(this, center_container_,
                            &rect_in_center_container_coords_f);
  gfx::Rect rect_in_client_view_coords =
      gfx::ToEnclosingRect(rect_in_center_container_coords_f);

  return !center_container_->HitTestRect(rect_in_client_view_coords);
}

void WebAppFrameToolbarView::OnWindowControlsOverlayEnabledChanged() {
  if (browser_view_->IsWindowControlsOverlayEnabled()) {
    // The color is not set until the view is added to a widget.
    if (active_background_color_) {
      SetBackground(views::CreateSolidBackground(
          paint_as_active_ ? *active_background_color_
                           : *inactive_background_color_));
    }

    // BrowserView paints to a layer, so this view must do the same to ensure
    // that it paints on top of the BrowserView.
    SetPaintToLayer();
    views::SetHitTestComponent(this, static_cast<int>(HTCAPTION));
  } else {
    SetBackground(nullptr);
    DestroyLayer();
    views::SetHitTestComponent(this, static_cast<int>(HTNOWHERE));
  }
  right_container_->extensions_toolbar_coordinator()
      ->GetExtensionsContainerViewController()
      ->WindowControlsOverlayEnabledChanged(
          browser_view_->IsWindowControlsOverlayEnabled());
}

void WebAppFrameToolbarView::UpdateBorderlessModeEnabled() {
  bool is_borderless_mode_enabled = browser_view_->IsBorderlessModeEnabled();

  // The toolbar is hidden and not set to null, because there are many features
  // that depend on the toolbar and would not work without it. For example all
  // the shortcut commands (e.g. Ctrl+F, zoom) rely on the menu button (child of
  // toolbar) so when these are hidden, the shortcuts will still work.
  SetVisible(!is_borderless_mode_enabled);
}

void WebAppFrameToolbarView::SetWindowControlsOverlayToggleVisible(
    bool visible) {
  right_container_->window_controls_overlay_toggle_button()->SetVisible(
      visible);
}

PageActionIconController*
WebAppFrameToolbarView::GetPageActionIconControllerForTesting() {
  return right_container_->page_action_icon_controller();
}

void WebAppFrameToolbarView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void WebAppFrameToolbarView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();
  UpdateCaptionColors();
}

views::View* WebAppFrameToolbarView::GetContentSettingContainerForTesting() {
  return right_container_->content_settings_container();
}

const std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>&
WebAppFrameToolbarView::GetContentSettingViewsForTesting() const {
  return right_container_->content_settings_container()
      ->get_content_setting_views();
}

void WebAppFrameToolbarView::UpdateCachedColors() {
  const BrowserNonClientFrameView* frame_view =
      browser_view_->frame()->GetFrameView();
  DCHECK(frame_view);

  active_background_color_ =
      frame_view->GetFrameColor(BrowserFrameActiveState::kActive);
  active_foreground_color_ =
      frame_view->GetCaptionColor(BrowserFrameActiveState::kActive);
  inactive_background_color_ =
      frame_view->GetFrameColor(BrowserFrameActiveState::kInactive);
  inactive_foreground_color_ =
      frame_view->GetCaptionColor(BrowserFrameActiveState::kInactive);
}

void WebAppFrameToolbarView::UpdateChildrenColor(bool color_changed) {
  const SkColor foreground_color = paint_as_active_
                                       ? *active_foreground_color_
                                       : *inactive_foreground_color_;
  const SkColor background_color = paint_as_active_
                                       ? *active_background_color_
                                       : *inactive_background_color_;
  right_container_->SetColors(foreground_color, background_color,
                              color_changed);

  if (browser_view_->IsWindowControlsOverlayEnabled()) {
    SetBackground(views::CreateSolidBackground(background_color));
  }
}

BEGIN_METADATA(WebAppFrameToolbarView)
ADD_PROPERTY_METADATA(bool, PaintAsActive)
END_METADATA
