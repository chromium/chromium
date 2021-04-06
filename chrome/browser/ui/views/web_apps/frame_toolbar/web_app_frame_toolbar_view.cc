// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"

#include <memory>

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/toolbar/back_forward_button.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_content_settings_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_navigation_button_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_utils.h"

WebAppFrameToolbarView::WebAppFrameToolbarView(views::Widget* widget,
                                               BrowserView* browser_view)
    : browser_view_(browser_view) {
  DCHECK(browser_view_);
  DCHECK(web_app::AppBrowserController::IsWebApp(browser_view_->browser()));
  SetID(VIEW_ID_WEB_APP_FRAME_TOOLBAR);

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
    left_container_ = AddChildView(
        std::make_unique<WebAppNavigationButtonContainer>(browser_view_));
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

  right_container_ =
      AddChildView(std::make_unique<WebAppToolbarButtonContainer>(
          widget, browser_view, this));
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
}

WebAppFrameToolbarView::~WebAppFrameToolbarView() = default;

void WebAppFrameToolbarView::UpdateStatusIconsVisibility() {
  right_container_->UpdateStatusIconsVisibility();
}

void WebAppFrameToolbarView::UpdateCaptionColors() {
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
  UpdateChildrenColor();
}

void WebAppFrameToolbarView::SetPaintAsActive(bool active) {
  if (paint_as_active_ == active)
    return;
  paint_as_active_ = active;
  UpdateChildrenColor();
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
  SetVisible(available_height > 0);

  if (available_height == 0) {
    SetSize(gfx::Size());
    return std::pair<int, int>(0, 0);
  }

  gfx::Size preferred_size = GetPreferredSize();
  const int width = std::max(trailing_x - leading_x, 0);
  const int height = preferred_size.height();
  DCHECK_LE(height, available_height);
  SetBounds(leading_x, y + (available_height - height) / 2, width, height);
  Layout();

  if (!center_container_->GetVisible())
    return std::pair<int, int>(0, 0);

  // Bounds for remaining inner space, in parent container coordinates.
  gfx::Rect center_bounds = center_container_->bounds();
  DCHECK(center_bounds.x() == 0 || left_container_);
  center_bounds.Offset(bounds().OffsetFromOrigin());

  return std::pair<int, int>(center_bounds.x(), center_bounds.right());
}

ExtensionsToolbarContainer*
WebAppFrameToolbarView::GetExtensionsToolbarContainer() {
  return right_container_->extensions_container();
}

gfx::Size WebAppFrameToolbarView::GetToolbarButtonSize() const {
  constexpr int kFocusModeButtonSize = 34;
  int size = browser_view_->browser()->is_focus_mode()
                 ? kFocusModeButtonSize
                 : GetLayoutConstant(WEB_APP_MENU_BUTTON_SIZE);
  return gfx::Size(size, size);
}

views::View* WebAppFrameToolbarView::GetDefaultExtensionDialogAnchorView() {
  return right_container_->extensions_container()->extensions_button();
}

PageActionIconView* WebAppFrameToolbarView::GetPageActionIconView(
    PageActionIconType type) {
  return right_container_->page_action_icon_controller()->GetIconView(type);
}

AppMenuButton* WebAppFrameToolbarView::GetAppMenuButton() {
  return right_container_->web_app_menu_button();
}

gfx::Rect WebAppFrameToolbarView::GetFindBarBoundingBox(int contents_bottom) {
  if (!IsDrawn())
    return gfx::Rect();

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

views::View* WebAppFrameToolbarView::GetAnchorView(PageActionIconType type) {
  views::View* anchor = GetAppMenuButton();
  return anchor ? anchor : this;
}

void WebAppFrameToolbarView::ZoomChangedForActiveTab(bool can_show_bubble) {
  right_container_->page_action_icon_controller()->ZoomChangedForActiveTab(
      can_show_bubble);
}

AvatarToolbarButton* WebAppFrameToolbarView::GetAvatarToolbarButton() {
  return nullptr;
}

ToolbarButton* WebAppFrameToolbarView::GetBackButton() {
  return left_container_ ? left_container_->back_button() : nullptr;
}

ReloadButton* WebAppFrameToolbarView::GetReloadButton() {
  return left_container_ ? left_container_->reload_button() : nullptr;
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

const std::vector<ContentSettingImageView*>&
WebAppFrameToolbarView::GetContentSettingViewsForTesting() const {
  return right_container_->content_settings_container()
      ->get_content_setting_views();
}

void WebAppFrameToolbarView::UpdateChildrenColor() {
  const SkColor foreground_color =
      paint_as_active_ ? active_foreground_color_ : inactive_foreground_color_;
  if (left_container_)
    left_container_->SetIconColor(foreground_color);
  right_container_->SetColors(
      foreground_color,
      paint_as_active_ ? active_background_color_ : inactive_background_color_);
}

BEGIN_METADATA(WebAppFrameToolbarView, views::AccessiblePaneView)
ADD_PROPERTY_METADATA(bool, PaintAsActive)
END_METADATA
