// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_MINI_TOOLBAR_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_MINI_TOOLBAR_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class ContentsWebView;
struct TabRendererData;

namespace tabs {
enum class TabAlert;
}

namespace ui {
class MenuModel;
}

namespace views {
class ImageButton;
class ImageView;
class Label;
class MenuRunner;
class WebView;
}  // namespace views

// MultiContentsViewMiniToolbar is shown for the inactive side of a split and
// displays the favicon, domain, tab alert state, and a menu button.
class MultiContentsViewMiniToolbar : public views::View,
                                     public TabStripModelObserver {
  METADATA_HEADER(MultiContentsViewMiniToolbar, views::View)

 public:
  MultiContentsViewMiniToolbar(BrowserView* browser_view,
                               ContentsWebView* web_view);
  ~MultiContentsViewMiniToolbar() override;

  void UpdateState(bool is_active);

 private:
  // TabStripModelObserver:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  // View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  void UpdateWebContents(views::WebView* web_view);
  void ClearWebContents(views::WebView*);

  // Returns the bounding path for the MultiContentsViewMiniToolbar. If
  // |border_stroke_only| then only the part of the path for the border stroke
  // will be returned. Otherwise, the entire bounding path will be returned.
  SkPath GetPath(bool border_stroke_only) const;

  void RegisterTabAlertSubscription();
  void OnAlertStatusIndicatorChanged(std::optional<tabs::TabAlert> new_alert);

  std::optional<TabRendererData> GetTabData();
  // Updates the favicon and domain based on the provided |tab_data|.
  void UpdateContents(TabRendererData tab_data);
  void UpdateFavicon(TabRendererData tab_data);
  void OpenSplitViewMenu();

  raw_ptr<views::ImageView> favicon_;
  raw_ptr<views::Label> domain_label_;
  raw_ptr<views::ImageView> alert_state_indicator_;
  raw_ptr<views::ImageButton> menu_button_;
  // Model for the split view menu.
  std::unique_ptr<ui::MenuModel> menu_model_;
  // Runner for the split view menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  raw_ptr<BrowserView> browser_view_;
  raw_ptr<content::WebContents> web_contents_;
  ui::ColorId stroke_color_ = kColorMulitContentsViewInactiveContentOutline;
  base::CallbackListSubscription web_contents_attached_subscription_;
  base::CallbackListSubscription web_contents_detached_subscription_;
  std::optional<base::CallbackListSubscription> tab_alert_status_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_MINI_TOOLBAR_H_
