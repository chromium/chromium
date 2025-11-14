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

class LocationBarView;

// A class that wraps a Widget's content view to provide a custom results frame.
class RoundedOmniboxResultsFrame : public views::View {
  METADATA_HEADER(RoundedOmniboxResultsFrame, views::View)

 public:
  RoundedOmniboxResultsFrame(views::View* contents,
                             LocationBarView* location_bar,
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

  void SetCutoutVisibility(bool visible);

  // views::View:
  void Layout(PassKey) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
#if !defined(USE_AURA)
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
#endif  // !USE_AURA

 private:
  gfx::Insets GetContentInsets();

  raw_ptr<views::View> top_background_ = nullptr;
  raw_ptr<views::View> contents_host_ = nullptr;
  raw_ptr<views::View> contents_;

  // Only used on platforms that support Aura (non-Mac).
  [[maybe_unused]] const bool forward_mouse_events_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_ROUNDED_OMNIBOX_RESULTS_FRAME_H_
