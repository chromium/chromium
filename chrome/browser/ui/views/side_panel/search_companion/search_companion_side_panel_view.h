// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_view.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/views/layout/flex_layout_view.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

namespace search_companion {

class SearchCompanionSidePanelView : public views::FlexLayoutView {
 public:
  explicit SearchCompanionSidePanelView(BrowserView* browser_view);
  SearchCompanionSidePanelView(const SearchCompanionSidePanelView&) = delete;
  SearchCompanionSidePanelView& operator=(const SearchCompanionSidePanelView&) =
      delete;
  ~SearchCompanionSidePanelView() override;

  // views::View
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  content::WebContents* GetWebContents();

  void UpdateContent(const std::string& page_url,
                     const std::string& suggest_response,
                     const std::string& content_annotation_response,
                     const std::string& image_content_response);

  base::WeakPtr<SearchCompanionSidePanelView> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<BrowserView> browser_view_;
  raw_ptr<views::WebView> web_view_;

  base::WeakPtrFactory<SearchCompanionSidePanelView> weak_factory_{this};
};

}  // namespace search_companion

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_VIEW_H_
