// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_modal_dialog_host.h"

#include "base/feature_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {
// The number of pixels the constrained window should overlap the bottom
// of the omnibox.
const int kConstrainedWindowOverlap = 3;
}  // namespace

TabModalDialogHost::TabModalDialogHost(
    BrowserView* browser_view,
    ContentsContainerView* contents_container_view)
    : browser_view_(browser_view),
      contents_container_view_(contents_container_view) {
  contents_container_view_observation_.Observe(contents_container_view_);
}

TabModalDialogHost::~TabModalDialogHost() {
  observer_list_.Notify(&web_modal::ModalDialogHostObserver::OnHostDestroying);
}

gfx::NativeView TabModalDialogHost::GetHostView() const {
  views::Widget* const host_widget = views::Widget::GetWidgetForNativeView(
      browser_view_->GetWidgetForAnchoring()->GetNativeView());
  return host_widget ? host_widget->GetNativeView() : gfx::NativeView();
}

gfx::Point TabModalDialogHost::GetDialogPosition(const gfx::Size& dialog_size) {
  gfx::Rect contents_container_view_coordinates_in_browser =
      contents_container_view_->ConvertRectToWidget(
          contents_container_view_->GetLocalBounds());

  const int leading_x = contents_container_view_coordinates_in_browser.x();
  const int middle_x = leading_x + contents_container_view_->width() / 2;
  const int dialog_starting_x = std::max(middle_x - dialog_size.width() / 2, 0);
  return gfx::Point(
      std::min(dialog_starting_x, browser_view_->width() - dialog_size.width()),
      GetDialogYCoordinate());
}

bool TabModalDialogHost::ShouldActivateDialog() const {
  // The browser Widget may be inactive if showing a bubble so instead check
  // against the last active browser window when determining whether to
  // activate the dialog.
  return GetLastActiveBrowserWindowInterfaceWithAnyProfile() ==
         browser_view_->browser();
}

bool TabModalDialogHost::ShouldConstrainDialogBoundsByHost() {
  return !base::FeatureList::IsEnabled(features::kTabModalUsesDesktopWidget);
}

void TabModalDialogHost::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TabModalDialogHost::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

gfx::Size TabModalDialogHost::GetMaximumDialogSize() {
  // Modals use NativeWidget and cannot be rendered beyond the browser
  // window boundaries. Restricting them to the browser window bottom
  // boundary and let the dialog to figure out a good layout.
  // WARNING: previous attempts to allow dialog to extend beyond the browser
  // boundaries have caused regressions in a number of dialogs. See
  // crbug.com/364463378, crbug.com/369739216, crbug.com/363205507.
  // TODO(crbug.com/334413759, crbug.com/346974105): use desktop widgets
  // universally.
  gfx::Rect content_area = contents_container_view_->ConvertRectToWidget(
      contents_container_view_->GetLocalBounds());
  // Use the browser view's entire contents container width as the maximum
  // dialog size instead of the content_area's width to prevent the dialogs from
  // clipping when the content_area becomes too small for the dialog. This will
  // cause the dialog to extend beyond its corresponding content_area but remain
  // the bounds of the browser view contents container.
  return gfx::Size(browser_view_->contents_container()->width(),
                   content_area.bottom() - GetDialogYCoordinate());
}

void TabModalDialogHost::OnViewAddedToWidget(views::View* observed_view) {
  widget_observation_.Observe(contents_container_view_->GetWidget());
}

void TabModalDialogHost::OnViewBoundsChanged(views::View* observed_view) {
  NotifyPositionRequiresUpdate();
}

void TabModalDialogHost::OnWidgetDestroying(views::Widget* browser_widget) {
  widget_observation_.Reset();
}

void TabModalDialogHost::OnWidgetBoundsChanged(views::Widget* browser_widget,
                                               const gfx::Rect& new_bounds) {
  // Update the modal dialogs' position when the browser window bounds change.
  // This is used to adjust the modal dialog's position when the browser
  // window is being dragged across screen boundaries. We avoid having the
  // modal dialog partially visible as it may display security-sensitive
  // information.
  NotifyPositionRequiresUpdate();
}

int TabModalDialogHost::GetDialogYCoordinate() {
  gfx::Rect toolbar_coordinates_in_browser =
      browser_view_->toolbar()->ConvertRectToWidget(
          browser_view_->toolbar()->GetLocalBounds());
  return toolbar_coordinates_in_browser.bottom() - kConstrainedWindowOverlap;
}

void TabModalDialogHost::NotifyPositionRequiresUpdate() {
  observer_list_.Notify(
      &web_modal::ModalDialogHostObserver::OnPositionRequiresUpdate);
}
