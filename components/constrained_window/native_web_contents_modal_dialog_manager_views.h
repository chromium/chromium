// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSTRAINED_WINDOW_NATIVE_WEB_CONTENTS_MODAL_DIALOG_MANAGER_VIEWS_H_
#define COMPONENTS_CONSTRAINED_WINDOW_NATIVE_WEB_CONTENTS_MODAL_DIALOG_MANAGER_VIEWS_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/single_web_contents_dialog_manager.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

namespace constrained_window {

// Class for parenting a tab-modal views dialog off of a views browser.
class NativeWebContentsModalDialogManagerViews
    : public web_modal::SingleWebContentsDialogManager,
      public web_modal::ModalDialogHostObserver,
      public views::WidgetObserver {
 public:
  NativeWebContentsModalDialogManagerViews(
      gfx::NativeWindow dialog,
      web_modal::SingleWebContentsDialogManagerDelegate* native_delegate);

  NativeWebContentsModalDialogManagerViews(
      const NativeWebContentsModalDialogManagerViews&) = delete;
  NativeWebContentsModalDialogManagerViews& operator=(
      const NativeWebContentsModalDialogManagerViews&) = delete;

  ~NativeWebContentsModalDialogManagerViews() override;

  // Sets up this object to manage the |dialog_|. Registers for closing events
  // in order to notify the delegate.
  void ManageDialog();

  // web_modal::SingleWebContentsDialogManager:
  void Show() override;
  void Hide() override;
  void Close() override;
  void Focus() override;
  void Pulse() override;
  bool IsActive() const override;

  // web_modal::ModalDialogHostObserver:
  void OnPositionRequiresUpdate() override;
  void OnHostDestroying() override;

  // views::WidgetObserver:

  // NOTE(wittman): OnWidgetClosing is overriden to ensure that, when the widget
  // is explicitly closed, the destruction occurs within the same call
  // stack. This avoids event races that lead to non-deterministic destruction
  // ordering in e.g. the print preview dialog. OnWidgetDestroying is overridden
  // because OnWidgetClosing is *only* invoked on explicit close, not when the
  // widget is implicitly destroyed due to its parent being closed. This
  // situation occurs with app windows.  WidgetClosing removes the observer, so
  // only one of these two functions is ever invoked for a given widget.
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void HostChanged(web_modal::WebContentsModalDialogHost* new_host) override;
  gfx::NativeWindow dialog() override;

 protected:
  web_modal::SingleWebContentsDialogManagerDelegate* native_delegate() {
    return native_delegate_;
  }

  static views::Widget* GetWidget(gfx::NativeWindow dialog);

 private:
  void WidgetClosing(views::Widget* widget);

  raw_ptr<web_modal::SingleWebContentsDialogManagerDelegate> native_delegate_;
  gfx::NativeWindow dialog_;
  raw_ptr<web_modal::WebContentsModalDialogHost> host_ = nullptr;
  bool host_destroying_ = false;
  std::set<raw_ptr<views::Widget, SetExperimental>> observed_widgets_;
  std::set<raw_ptr<views::Widget, SetExperimental>> shown_widgets_;
};

}  // namespace constrained_window

#endif  // COMPONENTS_CONSTRAINED_WINDOW_NATIVE_WEB_CONTENTS_MODAL_DIALOG_MANAGER_VIEWS_H_
