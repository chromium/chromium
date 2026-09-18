// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_ROUNDED_OMNIBOX_RESULTS_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_ROUNDED_OMNIBOX_RESULTS_FRAME_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

class LocationBar;
class OmniboxPopupWebUIBaseContent;

// A class that wraps a Widget's content view to provide a custom results frame.
class RoundedOmniboxResultsFrame : public views::View {
  METADATA_HEADER(RoundedOmniboxResultsFrame, views::View)

 public:
  RoundedOmniboxResultsFrame(views::View* contents,
                             LocationBar* location_bar,
                             bool forward_mouse_events);
  RoundedOmniboxResultsFrame(const RoundedOmniboxResultsFrame&) = delete;
  RoundedOmniboxResultsFrame& operator=(const RoundedOmniboxResultsFrame&) =
      delete;
  ~RoundedOmniboxResultsFrame() override;

  // Hook to customize Widget initialization.
  static void OnBeforeWidgetInit(views::Widget::InitParams* params,
                                 views::Widget* widget);

  // The height of the location bar view part of the omnibox popup.
  static int GetNonResultSectionHeight(bool include_cutout = true);

  // How the Widget is aligned relative to the location bar.
  static gfx::Insets GetLocationBarAlignmentInsets();

  // Returns the blur region taken up by the Omnibox popup shadows.
  static gfx::Insets GetShadowInsets();

  // Removes the `contents_` view and returns ownership to the caller.
  std::unique_ptr<views::View> ExtractContents();

  // Returns the `contents_` view.
  views::View* GetContents();

  // Returns the nested `OmniboxPopupWebUIBaseContent` if the contents of the
  // frame contains one.
  OmniboxPopupWebUIBaseContent* GetOmniboxPopupWebUIBaseContent();

  void SetCutoutVisibility(bool visible);

  static constexpr int kDefaultElevation = 16;

  // Updates whether mouse events should be forwarded to the underlying
  // location bar.
  void set_forward_mouse_events(bool forward) {
    forward_mouse_events_ = forward;
  }

  bool forward_mouse_events() const { return forward_mouse_events_; }

  // views::View:
  void Layout(PassKey) override;
  void AddedToWidget() override;
#if !defined(USE_AURA)
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
#endif  // !USE_AURA

 private:
  void SetElevation(int elevation);

  gfx::Insets GetContentInsets();

  raw_ptr<views::View> top_background_ = nullptr;
  raw_ptr<views::View> contents_host_ = nullptr;
  raw_ptr<views::View> contents_;

  // Only used on platforms that support Aura (non-Mac).
  [[maybe_unused]] bool forward_mouse_events_;

  // True when the WebUI paints the popup's background, rounded corners and
  // drop shadow instead of this frame. Determined at construction.
  bool draw_shadow_in_webui_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_ROUNDED_OMNIBOX_RESULTS_FRAME_H_
