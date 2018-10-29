// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/command_updater_delegate.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/media_router/presentation_receiver_window.h"
#include "chrome/browser/ui/toolbar/chrome_toolbar_model_delegate.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/views/widget/widget_delegate.h"

class ExclusiveAccessBubbleViews;
class PresentationReceiverWindowDelegate;
class PresentationReceiverWindowFrame;
class ToolbarModelImpl;

#if defined(OS_CHROMEOS)
class FullscreenWindowObserver;
#endif

// This class implements the View portion of PresentationReceiverWindow.  It
// contains a WebView for displaying the receiver page and a LocationBarView for
// displaying the URL.
class PresentationReceiverWindowView final
    : public PresentationReceiverWindow,
      public views::WidgetDelegateView,
      public LocationBarView::Delegate,
      public CommandUpdaterDelegate,
      public ChromeToolbarModelDelegate,
      public ExclusiveAccessContext,
      public ExclusiveAccessBubbleViewsContext,
      public ui::AcceleratorProvider {
 public:
  PresentationReceiverWindowView(PresentationReceiverWindowFrame* frame,
                                 PresentationReceiverWindowDelegate* delegate);
  ~PresentationReceiverWindowView() final;

  void Init();

  LocationBarView* location_bar_view() { return location_bar_view_; }

  // PresentationReceiverWindow overrides.
  void Close() final;
  bool IsWindowActive() const final;
  bool IsWindowFullscreen() const final;
  gfx::Rect GetWindowBounds() const final;
  void ShowInactiveFullscreen() final;
  void UpdateWindowTitle() final;
  void UpdateLocationBar() final;

  // LocationBarView::Delegate overrides.
  content::WebContents* GetWebContents() final;
  ToolbarModel* GetToolbarModel() final;
  const ToolbarModel* GetToolbarModel() const final;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      final;

  // CommandUpdaterDelegate overrides.
  void ExecuteCommandWithDisposition(int id,
                                     WindowOpenDisposition disposition) final;

  // ChromeToolbarModelDelegate overrides.
  content::WebContents* GetActiveWebContents() const final;

  // views::WidgetDelegateView overrides.
  bool CanResize() const final;
  bool CanMaximize() const final;
  bool CanMinimize() const final;
  void DeleteDelegate() final;
  base::string16 GetWindowTitle() const final;

  // ui::AcceleratorTarget overrides.
  bool AcceleratorPressed(const ui::Accelerator& accelerator) final;

  // ExclusiveAccessContext overrides.
  Profile* GetProfile() final;
  bool IsFullscreen() const final;
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType bubble_type) final;
  void ExitFullscreen() final;
  void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type,
      ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
      bool force_update) final;
  void OnExclusiveAccessUserInput() final;
  content::WebContents* GetActiveWebContents() final;
  void UnhideDownloadShelf() final;
  void HideDownloadShelf() final;
  bool CanUserExitFullscreen() const final;

  // ExclusiveAccessBubbleViewsContext overrides.
  ExclusiveAccessManager* GetExclusiveAccessManager() final;
  views::Widget* GetBubbleAssociatedWidget() final;
  ui::AcceleratorProvider* GetAcceleratorProvider() final;
  gfx::NativeView GetBubbleParentView() const final;
  gfx::Point GetCursorPointInParent() const final;
  gfx::Rect GetClientAreaBoundsInScreen() const final;
  bool IsImmersiveModeEnabled() const final;
  gfx::Rect GetTopContainerBoundsInScreen() final;
  void DestroyAnyExclusiveAccessBubble() final;
  bool CanTriggerOnMouse() const final;

  // ui::AcceleratorProvider overrides.
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const final;

 private:
  // Updates the UI in response to a change to fullscreen state.
  void OnFullscreenChanged();

  PresentationReceiverWindowFrame* const frame_;
  PresentationReceiverWindowDelegate* const delegate_;
  base::string16 title_;
  const std::unique_ptr<ToolbarModelImpl> toolbar_model_;
  CommandUpdaterImpl command_updater_;
  LocationBarView* location_bar_view_ = nullptr;
  ExclusiveAccessManager exclusive_access_manager_;
  ui::Accelerator fullscreen_accelerator_;
  std::unique_ptr<ExclusiveAccessBubbleViews> exclusive_access_bubble_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<FullscreenWindowObserver> window_observer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PresentationReceiverWindowView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_VIEW_H_
