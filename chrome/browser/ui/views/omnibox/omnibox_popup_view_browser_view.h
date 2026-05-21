// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_BROWSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_BROWSER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "ui/views/view_tracker.h"

class LocationBarView;
class Browser;
class BrowserView;
class EmbeddedWebView;
class RoundedOmniboxResultsFrame;

// Implements `OmniboxPopupView` as a WebView embedded directly in
// `BrowserView`. This is used for the next-gen Omnibox experience where the
// full WebUI (input and suggestions) is rendered inside `BrowserView` to avoid
// overlapping and window management issues associated with separate widgets.
// Gated behind `omnibox::kWebUIOmniboxFullPopupV2` with
// `Omnibox_UseBrowserView` param.
class OmniboxPopupViewBrowserView : public OmniboxPopupView,
                                    public OmniboxEditModel::Observer {
 public:
  OmniboxPopupViewBrowserView(LocationBarView* location_bar_view,
                              Browser* browser);
  OmniboxPopupViewBrowserView(const OmniboxPopupViewBrowserView&) = delete;
  OmniboxPopupViewBrowserView& operator=(const OmniboxPopupViewBrowserView&) =
      delete;
  ~OmniboxPopupViewBrowserView() override;

  // OmniboxPopupView:
  bool IsOpen() const override;
  void InvalidateLine(size_t line) override;
  void UpdatePopupAppearance() override;
  void ProvideButtonFocusHint(size_t line) override;
  void OnDragCanceled() override;
  void GetPopupAccessibleNodeData(ui::AXNodeData* node_data) const override;
  bool IsSelectionPopupControlled() const override;
  OmniboxPopupViewBrowserView* AsOmniboxPopupViewBrowserView() override;

  // Updates the layout of the embedded WebView. Should be called when
  // BrowserView layouts or when popup appearance changes.
  void UpdateLayout();

  void SetBrowserView(BrowserView* browser_view) {
    browser_view_ = browser_view;
  }

  RoundedOmniboxResultsFrame* popup_frame();

  // OmniboxEditModel::Observer:
  void OnContentsChanged() override;
  void OnSelectionChanged(OmniboxPopupSelection old_selection,
                          OmniboxPopupSelection selection) override {}
  void OnMatchIconUpdated(size_t index) override {}
  void OnKeywordStateChanged(bool is_keyword_selected) override {}
  void OnCharTyped(base::TimeTicks timestamp) override {}

 private:
  void OnWebViewResize(const gfx::Size& new_size);

  const raw_ptr<LocationBarView> location_bar_view_;
  const raw_ptr<Browser> browser_;
  raw_ptr<BrowserView> browser_view_ = nullptr;
  raw_ptr<EmbeddedWebView> web_view_ = nullptr;
  // Tracks the popup frame which is owned by `browser_view_` and may be
  // destroyed before this `OmniboxPopupViewBrowserView` instance during
  // shutdown, leading to dangling pointers. `ViewTracker` automatically nulls
  // out the pointer when the view is destroyed.
  views::ViewTracker popup_frame_tracker_;
  int content_height_ = 0;

  base::ScopedObservation<OmniboxEditModel, OmniboxEditModel::Observer>
      edit_model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_BROWSER_VIEW_H_
