// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NATIVE_WIDGET_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NATIVE_WIDGET_ASH_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/frame/browser_native_widget.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWidget;
class BrowserView;

// BrowserNativeWidgetAsh provides the frame for Chrome browser windows on
// Chrome OS under classic ash.
class BrowserNativeWidgetAsh : public views::NativeWidgetAura,
                               public BrowserNativeWidget,
                               public views::WidgetObserver {
 public:
  BrowserNativeWidgetAsh(BrowserWidget* browser_widget,
                         BrowserView* browser_view);

  BrowserNativeWidgetAsh(const BrowserNativeWidgetAsh&) = delete;
  BrowserNativeWidgetAsh& operator=(const BrowserNativeWidgetAsh&) = delete;

 protected:
  ~BrowserNativeWidgetAsh() override;

  // Overridden from views::NativeWidgetAura:
  void OnWidgetInitDone() override;
  void OnWindowTargetVisibilityChanged(bool visible) override;

  // Overridden from BrowserNativeWidget:
  views::Widget::InitParams GetWidgetParams(
      views::Widget::InitParams::Ownership ownership) override;
  bool UseCustomFrame() const override;
  bool UsesNativeSystemMenu() const override;
  bool ShouldSaveWindowPlacement() const override;
  void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  bool ShouldRestorePreviousBrowserWidgetState() const override;
  bool ShouldUseInitialVisibleOnAllWorkspaces() const override;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

 private:
  // Set the window into the auto managed mode.
  void SetWindowAutoManaged();

  // The BrowserView is our ClientView. This is a pointer to it.
  raw_ptr<BrowserView> browser_view_;

  // Set true when dragging a tab to create a browser window.
  bool created_from_drag_ = false;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NATIVE_WIDGET_ASH_H_
