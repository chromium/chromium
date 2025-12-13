// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TAB_MODAL_DIALOG_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TAB_MODAL_DIALOG_HOST_H_

#include "base/scoped_observation.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_observer.h"

class BrowserView;
class ContentsContainerView;

// Dialog host used specifically for tab modals to help with positioning the tab
// modals relative to its ContentsContainerView.
class TabModalDialogHost : public web_modal::WebContentsModalDialogHost,
                           public views::WidgetObserver,
                           public views::ViewObserver {
 public:
  TabModalDialogHost(BrowserView* browser_view,
                     ContentsContainerView* contents_container_view);

  TabModalDialogHost(const TabModalDialogHost&) = delete;
  TabModalDialogHost& operator=(const TabModalDialogHost&) = delete;

  ~TabModalDialogHost() override;

  // web_modal::ModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& dialog_size) override;
  bool ShouldActivateDialog() const override;
  bool ShouldConstrainDialogBoundsByHost() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // web_modal::WebContentsModalDialogHost:
  gfx::Size GetMaximumDialogSize() override;

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override;
  void OnViewBoundsChanged(views::View* observed_view) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* browser_widget) override;
  void OnWidgetBoundsChanged(views::Widget* browser_widget,
                             const gfx::Rect& new_bounds) override;

 private:
  int GetDialogYCoordinate();
  void NotifyPositionRequiresUpdate();

  const raw_ptr<BrowserView> browser_view_;
  const raw_ptr<ContentsContainerView> contents_container_view_;
  base::ScopedObservation<views::View, views::ViewObserver>
      contents_container_view_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      observer_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TAB_MODAL_DIALOG_HOST_H_
