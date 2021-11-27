// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_LATER_SIDE_PANEL_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_LATER_SIDE_PANEL_WEB_VIEW_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/webui/read_later/read_later_ui.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

class Browser;

namespace ui {
class MenuModel;
}  // namespace ui

namespace views {
class MenuRunner;
}  // namespace views

class ReadLaterSidePanelWebView : public views::WebView,
                                  public TabStripModelObserver,
                                  public BubbleContentsWrapper::Host {
 public:
  ReadLaterSidePanelWebView(Browser* browser, base::RepeatingClosure close_cb);
  ReadLaterSidePanelWebView(const ReadLaterSidePanelWebView&) = delete;
  ReadLaterSidePanelWebView& operator=(const ReadLaterSidePanelWebView&) =
      delete;
  ~ReadLaterSidePanelWebView() override;

  void SetVisible(bool visible) override;

  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  // BubbleContentsWrapper::Host:
  void ShowUI() override;
  void CloseUI() override;
  void ShowCustomContextMenu(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model) override;
  void HideCustomContextMenu() override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

 private:
  void UpdateActiveURL(content::WebContents* contents);

  const raw_ptr<Browser> browser_;
  base::RepeatingClosure close_cb_;
  std::unique_ptr<BubbleContentsWrapperT<ReadLaterUI>> contents_wrapper_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::unique_ptr<ui::MenuModel> context_menu_model_;
  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  base::WeakPtrFactory<ReadLaterSidePanelWebView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_LATER_SIDE_PANEL_WEB_VIEW_H_
