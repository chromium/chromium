// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IME_IME_WINDOW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_IME_IME_WINDOW_VIEW_H_

#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "chrome/browser/ui/input_method/ime_native_window.h"
#include "chrome/browser/ui/input_method/ime_window.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_icon_image.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class WebView;
}

namespace ui {

class ImeWindowFrameView;

// The views implementation for the IME window UI.
// This interacts with ImeWindow through the ImeNativeWindow interface.
class ImeWindowView : public ImeNativeWindow,
                      public views::WidgetDelegateView {
 public:
  enum class PointerType { MOUSE, TOUCH };

  ImeWindowView(ImeWindow* ime_window,
                const gfx::Rect& bounds,
                content::WebContents* contents);
  ~ImeWindowView() override;

  void OnCloseButtonClicked();

  // Methods to deal with mouse/touch dragging on the non client view.
  bool OnTitlebarPointerPressed(const gfx::Point& pointer_location,
                                PointerType pointer_type);
  bool OnTitlebarPointerDragged(const gfx::Point& pointer_location,
                                PointerType pointer_type);
  void OnTitlebarPointerReleased(PointerType pointer_type);
  void OnTitlebarPointerCaptureLost();

  // ui::ImeNativeWindow:
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() const override;
  void UpdateWindowIcon() override;
  bool IsVisible() const override;

  // views::WidgetDelegateView:
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool CanActivate() const override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  base::string16 GetWindowTitle() const override;
  gfx::ImageSkia GetWindowIcon() override;
  void DeleteDelegate() override;

  ImeWindowFrameView* GetFrameView() const;
  views::Widget* window() const { return window_; }
  views::WebView* web_view() const { return web_view_; }

 private:
  enum class DragState { NO_DRAG, POSSIBLE_DRAG, ACTIVE_DRAG };
  void EndDragging();

  ImeWindow* ime_window_;

  // Member variables for dragging.
  PointerType dragging_pointer_type_;
  gfx::Point pointer_location_on_press_;
  DragState dragging_state_;
  gfx::Rect bounds_on_drag_start_;

  // The native window.
  views::Widget* window_;

  // The web view control which hosts the web contents.
  views::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(ImeWindowView);
};

}  // namespace ui

#endif  // CHROME_BROWSER_UI_VIEWS_IME_IME_WINDOW_VIEW_H_
