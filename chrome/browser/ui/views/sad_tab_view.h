// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/sad_tab_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace views {
class FlexLayoutView;
class Label;
class MdTextButton;
class WebView;
}  // namespace views

namespace test {
class SadTabViewTestApi;
}  // namespace test

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

class SadTabController;

///////////////////////////////////////////////////////////////////////////////
//
// SadTabView
//
//  A views::View subclass used to render the presentation of the crashed
//  "sad tab" in the browser window when a renderer is destroyed unnaturally.
//
///////////////////////////////////////////////////////////////////////////////
class SadTabView : public views::View {
  METADATA_HEADER(SadTabView, views::View)

 public:
  SadTabView(SadTabController* controller,
             content::WebContents* web_contents,
             SadTabKind kind,
             int title_id,
             int message_id,
             std::vector<int> sub_message_ids,
             int error_code_format_id,
             int error_code,
             int button_title_id,
             int help_link_title_id);

  SadTabView(const SadTabView&) = delete;
  SadTabView& operator=(const SadTabView&) = delete;

  ~SadTabView() override;

  gfx::RoundedCornersF GetBackgroundRadii() const;
  void SetBackgroundRadii(const gfx::RoundedCornersF& radii);

  void ReinstallInWebView();

  // Overridden from views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 protected:
  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void RemovedFromWidget() override;

 private:
  friend class test::SadTabViewTestApi;

  // Set this View as the crashed overlay view for the WebView associated
  // with this object's WebContents.
  void AttachToWebView();

  // Enable help link if needed.
  void EnableHelpLink(views::FlexLayoutView* actions_container,
                      int help_link_title_id);

  const raw_ptr<SadTabController> controller_;
  const raw_ptr<content::WebContents> web_contents_;
  const SadTabKind kind_;
  bool painted_ = false;
  raw_ptr<views::Label> message_;
  std::vector<raw_ptr<views::Label, VectorExperimental>> bullet_labels_;
  raw_ptr<views::MdTextButton> action_button_;
  raw_ptr<views::Label> title_;
  raw_ptr<views::WebView> owner_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_
