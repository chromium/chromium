// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_VIEW_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "chrome/browser/ui/views/frame/tab_modal_dialog_host.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class BrowserView;
class ContentsContainerOutline;
class ContentsWebView;
class MultiContentsViewMiniToolbar;
class ScrimView;
class ActorOverlayWebView;

namespace gfx {
class Rect;
}  // namespace gfx

namespace glic {
class ContextSharingBorderView;
}  // namespace glic

namespace new_tab_footer {
class NewTabFooterWebView;
}  // namespace new_tab_footer

namespace views {
class WebView;
class Widget;
}  // namespace views

namespace enterprise_watermark {
class WatermarkView;
}  // namespace enterprise_watermark

// ContentsContainerView holds the ContentsWebView and the outlines and
// minitoolbar when in split view.
class ContentsContainerView : public views::View,
                              public views::LayoutDelegate,
                              public views::ViewObserver {
  METADATA_HEADER(ContentsContainerView, views::View)
 public:
  // Enumerates where the devtools are docked relative to the main web contents.
  enum class DevToolsDockedPlacement {
    kLeft,
    kRight,
    kBottom,
    // Devtools are not docked.
    kNone,
    kUnknown
  };

  explicit ContentsContainerView(BrowserView* browser_view);
  ContentsContainerView(ContentsContainerView&) = delete;
  ContentsContainerView& operator=(const ContentsContainerView&) = delete;
  ~ContentsContainerView() override;

  // Returns accessible panes to be used in BrowserView to create the order of
  // pane traversal.
  std::vector<views::View*> GetAccessiblePanes();

  ContentsWebView* contents_view() { return contents_view_; }
  MultiContentsViewMiniToolbar* mini_toolbar() { return mini_toolbar_; }
  ScrimView* contents_scrim_view() { return contents_scrim_view_; }
  views::WebView* devtools_web_view() { return devtools_web_view_; }
  ScrimView* devtools_scrim_view() { return devtools_scrim_view_; }
  DevToolsDockedPlacement devtools_docked_placement() {
    return current_devtools_docked_placement_;
  }
  ActorOverlayWebView* actor_overlay_web_view() {
    return actor_overlay_web_view_;
  }
  views::View* immersive_read_anything_overlay_view() {
    return immersive_read_anything_overlay_view_;
  }
  glic::ContextSharingBorderView* glic_border_view() { return glic_border_; }
  new_tab_footer::NewTabFooterWebView* new_tab_footer_view() {
    return new_tab_footer_view_;
  }
  views::Widget* capture_contents_border_widget() {
    return capture_contents_border_widget_.get();
  }
  enterprise_watermark::WatermarkView* watermark_view() {
    return watermark_view_;
  }
  const ContentsContainerOutline* contents_outline_view() const {
    return container_outline_;
  }
  TabModalDialogHost* web_contents_modal_dialog_host() {
    return &web_contents_modal_dialog_host_;
  }

  // Sets the contents resizing strategy.
  void SetContentsResizingStrategy(
      const DevToolsContentsResizingStrategy& strategy);
  DevToolsContentsResizingStrategy& contents_resizing_strategy() {
    return strategy_;
  }

  void ApplyWatermarkSettings(const std::string& watermark_text,
                              SkColor fill_color,
                              SkColor outline_color,
                              int font_size);

  void UpdateBorderAndOverlay(bool is_in_split,
                              bool is_active,
                              bool is_highlighted);

  void ShowCaptureContentsBorder();
  void HideCaptureContentsBorder();
  void SetCaptureContentsBorderLocation(
      std::optional<gfx::Rect> border_location);

  // Returns the contents_view bounds including ntp footer.
  gfx::Rect GetContentsViewBounds() const;

 private:
  void CreateCaptureContentsBorder();
  void UpdateCaptureContentsBorderLocation();

  // Updates the DevTools docked placement. It infers the docked placement from
  // the bounds of contents_webview relative to the local bounds of the
  // container that holds both contents_webview and devtools_webview.
  void UpdateDevToolsDockedPlacement();

  void UpdateBorderRoundedCorners();
  void ClearBorderRoundedCorners();

  // views::View:
  void ChildVisibilityChanged(View* child) override;
  void Layout(PassKey) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(View* observed_view) override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  bool is_in_split_ = false;

  raw_ptr<BrowserView> browser_view_ = nullptr;
  raw_ptr<ContentsWebView> contents_view_ = nullptr;

  TabModalDialogHost web_contents_modal_dialog_host_;

  // The view that contains devtools window for the WebContents.
  raw_ptr<views::WebView> devtools_web_view_ = nullptr;
  // The scrim view that covers the devtools area when a tab-modal dialog is
  // open.
  raw_ptr<ScrimView> devtools_scrim_view_ = nullptr;
  DevToolsDockedPlacement current_devtools_docked_placement_ =
      DevToolsDockedPlacement::kNone;

  // The view that contains the Immersive Reading Mode. This view is an overlay
  // on top of the ContentsWebView.
  raw_ptr<views::View> immersive_read_anything_overlay_view_ = nullptr;

  // The view that shows a footer at the bottom of the contents
  // container on new tab pages.
  raw_ptr<new_tab_footer::NewTabFooterWebView> new_tab_footer_view_ = nullptr;
  // Separator between the web contents and the Footer.
  raw_ptr<views::View> new_tab_footer_view_separator_ = nullptr;

  // The view that overlays a watermark on the contents container.
  raw_ptr<enterprise_watermark::WatermarkView> watermark_view_ = nullptr;

  // The scrim view that covers the content area when a tab-modal dialog is
  // open.
  raw_ptr<ScrimView> contents_scrim_view_ = nullptr;

  // The view that contains the Glic Actor Overlay. The Actor Overlay is a UI
  // overlay that is shown on top of the web contents.
  raw_ptr<ActorOverlayWebView> actor_overlay_web_view_ = nullptr;

  // The glic browser view that renders around the web contents area.
  raw_ptr<glic::ContextSharingBorderView> glic_border_ = nullptr;

  raw_ptr<MultiContentsViewMiniToolbar> mini_toolbar_ = nullptr;

  raw_ptr<ContentsContainerOutline> container_outline_ = nullptr;

  std::unique_ptr<views::Widget> capture_contents_border_widget_;
  std::optional<gfx::Rect> dynamic_capture_content_border_bounds_;

  DevToolsContentsResizingStrategy strategy_;
  base::ScopedObservation<View, ViewObserver> view_bounds_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_VIEW_H_
