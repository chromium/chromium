// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/command_updater_delegate.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/media_router/presentation_receiver_window.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_delegate.h"

class ExclusiveAccessBubbleViews;
class PresentationReceiverWindowDelegate;
class PresentationReceiverWindowFrame;
class LocationBarModelImpl;

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
      public ChromeLocationBarModelDelegate,
      public ExclusiveAccessContext,
      public ExclusiveAccessBubbleViewsContext,
      public ui::AcceleratorProvider {
  METADATA_HEADER(PresentationReceiverWindowView, views::WidgetDelegateView)

 public:
  PresentationReceiverWindowView(PresentationReceiverWindowFrame* frame,
                                 PresentationReceiverWindowDelegate* delegate);
  PresentationReceiverWindowView(const PresentationReceiverWindowView&) =
      delete;
  PresentationReceiverWindowView& operator=(
      const PresentationReceiverWindowView&) = delete;
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
  LocationBarModel* GetLocationBarModel() final;
  const LocationBarModel* GetLocationBarModel() const final;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      final;

  // CommandUpdaterDelegate overrides.
  void ExecuteCommandWithDisposition(int id,
                                     WindowOpenDisposition disposition) final;

  // ChromeLocationBarModelDelegate overrides.
  content::WebContents* GetActiveWebContents() const final;

  // views::WidgetDelegateView overrides.
  std::u16string GetWindowTitle() const final;

  // ui::AcceleratorTarget overrides.
  bool AcceleratorPressed(const ui::Accelerator& accelerator) final;

  // ExclusiveAccessContext overrides.
  Profile* GetProfile() final;
  bool IsFullscreen() const final;
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType bubble_type,
                       const int64_t display_id) final;
  void ExitFullscreen() final;
  void UpdateExclusiveAccessBubble(
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback) override;
  bool IsExclusiveAccessBubbleDisplayed() const final;
  void OnExclusiveAccessUserInput() final;
  content::WebContents* GetWebContentsForExclusiveAccess() final;
  bool CanUserExitFullscreen() const final;

  // ExclusiveAccessBubbleViewsContext overrides.
  ExclusiveAccessManager* GetExclusiveAccessManager() final;
  ui::AcceleratorProvider* GetAcceleratorProvider() final;
  gfx::NativeView GetBubbleParentView() const final;
  gfx::Rect GetClientAreaBoundsInScreen() const final;
  bool IsImmersiveModeEnabled() const final;
  gfx::Rect GetTopContainerBoundsInScreen() final;
  void DestroyAnyExclusiveAccessBubble() final;

  // ui::AcceleratorProvider overrides.
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const final;

 private:
  // Updates the UI in response to a change to fullscreen state.
  void OnFullscreenChanged();

  const raw_ptr<PresentationReceiverWindowFrame> frame_;
  const raw_ptr<PresentationReceiverWindowDelegate> delegate_;
  std::u16string title_;
  const std::unique_ptr<LocationBarModelImpl> location_bar_model_;
  CommandUpdaterImpl command_updater_;
  raw_ptr<LocationBarView> location_bar_view_ = nullptr;
  ExclusiveAccessManager exclusive_access_manager_;
  ui::Accelerator fullscreen_accelerator_;
  std::unique_ptr<ExclusiveAccessBubbleViews> exclusive_access_bubble_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<FullscreenWindowObserver> window_observer_;
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_VIEW_H_
