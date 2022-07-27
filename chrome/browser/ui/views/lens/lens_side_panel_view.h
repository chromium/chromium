// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/layout/flex_layout_view.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace views {
class ImageButton;
class ImageView;
class Separator;
class WebView;
}  // namespace views

namespace lens {

class LensSidePanelView : public views::FlexLayoutView {
 public:
  LensSidePanelView(content::BrowserContext* browser_context,
                    base::RepeatingClosure close_callback,
                    base::RepeatingClosure launch_callback);
  LensSidePanelView(const LensSidePanelView&) = delete;
  LensSidePanelView& operator=(const LensSidePanelView&) = delete;
  ~LensSidePanelView() override;

  content::WebContents* GetWebContents();

  // views::FlexLayoutView:
  void OnThemeChanged() override;

  void SetContentVisible(bool visible);

 private:
  void CreateAndInstallHeader(base::RepeatingClosure close_callback,
                              base::RepeatingClosure launch_callback);

  // Validates side panel URL and updates enabled/disabled
  // state of the launch button.
  void UpdateLaunchButtonState();

  raw_ptr<views::ImageView> branding_;
  raw_ptr<views::Separator> separator_;
  raw_ptr<views::WebView> loading_indicator_web_view_;
  raw_ptr<views::WebView> web_view_;
  raw_ptr<views::ImageButton> close_button_;
  raw_ptr<views::ImageButton> launch_button_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_VIEW_H_
