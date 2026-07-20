// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/command_updater_delegate.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/media_router/presentation_receiver_window.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/ui/views/exclusive_access/exclusive_access_bubble_views_context.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

class ExclusiveAccessBubbleViews;
class PresentationReceiverWindowDelegate;
class PresentationReceiverWindowFrame;
class LocationBarModelImpl;

namespace views {
class WebView;
}

#if BUILDFLAG(IS_CHROMEOS)
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
      public ui::AcceleratorProvider,
      public views::WidgetObserver,
      public web_modal::WebContentsModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
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
  views::WebView* web_view_for_testing() const { return web_view_; }

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
  void HandleCommandWithDisposition(int id,
                                    WindowOpenDisposition disposition,
                                    base::TimeTicks time_stamp) final;

  // ChromeLocationBarModelDelegate overrides.
  content::WebContents* GetActiveWebContents() const final;

  // views::WidgetDelegateView overrides.
  std::u16string GetWindowTitle() const final;

  // views::View overrides.
  void OnBoundsChanged(const gfx::Rect& previous_bounds) final;
  void AddedToWidget() final;
  void RemovedFromWidget() final;

  // views::WidgetObserver overrides.
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) final;
  void OnWidgetDestroying(views::Widget* widget) final;

  // web_modal::WebContentsModalDialogManagerDelegate overrides.
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost(
      content::WebContents* web_contents) final;
  bool IsWebContentsVisible(content::WebContents* web_contents) final;

  // web_modal::WebContentsModalDialogHost overrides.
  gfx::NativeView GetHostView() const final;
  gfx::Point GetDialogPosition(const gfx::Size& size) final;
  gfx::Size GetMaximumDialogSize() final;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) final;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) final;
  void NotifyPositionRequiresUpdate() final;

  // ui::AcceleratorTarget overrides.
  bool AcceleratorPressed(const ui::Accelerator& accelerator) final;

  // ExclusiveAccessContext overrides.
  Profile* GetProfile() final;
  bool IsFullscreen() const final;
  void EnterFullscreen(const url::Origin& origin,
                       ExclusiveAccessBubbleType bubble_type,
                       FullscreenTabParams fullscreen_tab_params) final;
  void ExitFullscreen() final;
  void UpdateExclusiveAccessBubble(
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback) override;
  bool IsExclusiveAccessBubbleDisplayed() const final;
  void OnExclusiveAccessUserInput() final;
  content::WebContents* GetWebContentsForExclusiveAccess() final;
  bool CanUserEnterFullscreen() const final;
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
  raw_ptr<views::WebView> web_view_ = nullptr;
  base::ObserverList<web_modal::ModalDialogHostObserver> observer_list_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  ExclusiveAccessManager exclusive_access_manager_;
  ui::Accelerator fullscreen_accelerator_;
  std::unique_ptr<ExclusiveAccessBubbleViews> exclusive_access_bubble_;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<FullscreenWindowObserver> window_observer_;
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_VIEW_H_
