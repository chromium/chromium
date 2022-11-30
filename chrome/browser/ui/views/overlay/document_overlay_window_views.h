// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_DOCUMENT_OVERLAY_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_DOCUMENT_OVERLAY_WINDOW_VIEWS_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/overlay/overlay_window_views.h"

class BackToTabImageButton;
class CloseImageButton;
class Profile;
class ResizeHandleButton;

class OverlayLocationBarViewProxy {
 public:
  OverlayLocationBarViewProxy() = default;
  OverlayLocationBarViewProxy(const OverlayLocationBarViewProxy&) = delete;
  OverlayLocationBarViewProxy& operator=(const OverlayLocationBarViewProxy&) =
      delete;
  virtual ~OverlayLocationBarViewProxy();
  virtual void Init() = 0;
  virtual std::unique_ptr<views::View> ReleaseView() = 0;
};

class DocumentOverlayWindowViews : public OverlayWindowViews,
                                   public content::DocumentOverlayWindow {
 public:
  // Constructs and initializes an instance. Since it includes a location bar
  // view which is rather testing hostile due to its many dependencies, the
  // optional argument supports supplying a fake implementation.
  static std::unique_ptr<DocumentOverlayWindowViews> Create(
      content::DocumentPictureInPictureWindowController* controller,
      std::unique_ptr<OverlayLocationBarViewProxy>
          location_bar_view_proxy_for_testing = nullptr);

  DocumentOverlayWindowViews(const DocumentOverlayWindowViews&) = delete;
  DocumentOverlayWindowViews& operator=(const DocumentOverlayWindowViews&) =
      delete;

  ~DocumentOverlayWindowViews() override;

  // OverlayWindow:
  bool IsActive() override;
  void Close() override;
  void ShowInactive() override;
  void Hide() override;
  bool IsVisible() override;
  bool IsAlwaysOnTop() override;
  gfx::Rect GetBounds() override;
  void UpdateNaturalSize(const gfx::Size& natural_size) override;

  // views::Widget:
  bool IsActive() const override;
  bool IsVisible() const override;
  void OnNativeWidgetMove() override;
  void OnNativeWidgetDestroyed() override;
  const ui::ThemeProvider* GetThemeProvider() const override;

  // OverlayWindowViews
  bool ControlsHitTestContainsPoint(const gfx::Point& point) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  int GetResizeHTComponent() const override;
  gfx::Rect GetResizeHandleControlsBounds() override;
  void UpdateResizeHandleBounds(WindowQuadrant quadrant) override;
#endif
  void OnUpdateControlsBounds() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void SetUpViews() override;
  void OnRootViewReady() override;
  void UpdateLayerBoundsWithLetterboxing(gfx::Size window_size) override;
  content::PictureInPictureWindowController* GetController() const override;
  views::View* GetWindowBackgroundView() const override;
  views::View* GetControlsContainerView() const override;

  // Gets the bounds of the controls.
  gfx::Rect GetBackToTabControlsBounds();
  gfx::Rect GetCloseControlsBounds();

  // Unit test support.
  CloseImageButton* close_button_for_testing() const;
  ui::Layer* document_layer_for_testing() const;

  void set_location_bar_view_proxy(
      std::unique_ptr<OverlayLocationBarViewProxy> proxy);

 private:
  explicit DocumentOverlayWindowViews(
      content::DocumentPictureInPictureWindowController* controller);

  // Calculate and set the bounds of the controls.
  gfx::Rect CalculateControlsBounds(int x, const gfx::Size& size);

  // Not owned; |controller_| owns |this|.
  raw_ptr<content::DocumentPictureInPictureWindowController> controller_ =
      nullptr;

  std::unique_ptr<OverlayLocationBarViewProxy> location_bar_view_proxy_;

  raw_ptr<Profile> profile_for_theme_ = nullptr;

  // Temporary storage for child Views. Used during the time between
  // construction and initialization, when the views::View pointer members must
  // already be initialized, but there is no root view to add them to yet.
  std::vector<std::unique_ptr<views::View>> view_holder_;

  // Views to be shown. The views are first temporarily owned by view_holder_,
  // then passed to this widget's ContentsView which takes ownership.
  raw_ptr<views::View> window_background_view_ = nullptr;
  raw_ptr<views::View> web_view_ = nullptr;
  raw_ptr<views::View> controls_container_view_ = nullptr;
  raw_ptr<views::View> location_bar_view_ = nullptr;
  raw_ptr<CloseImageButton> close_controls_view_ = nullptr;
  raw_ptr<BackToTabImageButton> back_to_tab_image_button_ = nullptr;
  raw_ptr<ResizeHandleButton> resize_handle_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_DOCUMENT_OVERLAY_WINDOW_VIEWS_H_
