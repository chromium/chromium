// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_ASH_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget_observer.h"

class BrowserFrame;
class BrowserView;

// BrowserFrameAsh provides the frame for Chrome browser windows on Chrome OS
// under classic ash.
class BrowserFrameAsh : public views::NativeWidgetAura,
                        public NativeBrowserFrame,
                        public views::WidgetObserver {
 public:
  BrowserFrameAsh(BrowserFrame* browser_frame, BrowserView* browser_view);

  BrowserFrameAsh(const BrowserFrameAsh&) = delete;
  BrowserFrameAsh& operator=(const BrowserFrameAsh&) = delete;

 protected:
  ~BrowserFrameAsh() override;

  // Overridden from views::NativeWidgetAura:
  void OnWidgetInitDone() override;
  void OnWindowTargetVisibilityChanged(bool visible) override;

  // Overridden from NativeBrowserFrame:
  views::Widget::InitParams GetWidgetParams(
      views::Widget::InitParams::Ownership ownership) override;
  bool UseCustomFrame() const override;
  bool UsesNativeSystemMenu() const override;
  int GetMinimizeButtonOffset() const override;
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

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_ASH_H_
