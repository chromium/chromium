// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_WEB_UI_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_WEB_UI_VIEW_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

namespace ui {
class MenuModel;
}  // namespace ui

namespace views {
class MenuRunner;
}  // namespace views

// SidePanelWebUIView holds generic behavior for side panel entry views hosting
// WebUI. This includes keyboard event handling, context menu triggering, and
// handling visibility updates.
class SidePanelWebUIView : public views::WebView,
                           public BubbleContentsWrapper::Host {
 public:
  SidePanelWebUIView(base::RepeatingClosure on_show_cb,
                     base::RepeatingClosure close_cb,
                     BubbleContentsWrapper* contents_wrapper);
  SidePanelWebUIView(const SidePanelWebUIView&) = delete;
  SidePanelWebUIView& operator=(const SidePanelWebUIView&) = delete;
  ~SidePanelWebUIView() override;

  // views::WebView:
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

 private:
  base::RepeatingClosure on_show_cb_;
  base::RepeatingClosure close_cb_;
  raw_ptr<BubbleContentsWrapper, DanglingUntriaged> contents_wrapper_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::unique_ptr<ui::MenuModel> context_menu_model_;
  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  base::WeakPtrFactory<SidePanelWebUIView> weak_factory_{this};
};

template <class T>
class SidePanelWebUIViewT : public SidePanelWebUIView {
 public:
  SidePanelWebUIViewT(
      base::RepeatingClosure on_show_cb,
      base::RepeatingClosure close_cb,
      std::unique_ptr<BubbleContentsWrapperT<T>> contents_wrapper)
      : SidePanelWebUIView(std::move(on_show_cb),
                           std::move(close_cb),
                           contents_wrapper.get()),
        contents_wrapper_(std::move(contents_wrapper)) {
    contents_wrapper_->ReloadWebContents();
  }
  SidePanelWebUIViewT(const SidePanelWebUIViewT&) = delete;
  SidePanelWebUIViewT& operator=(const SidePanelWebUIViewT&) = delete;
  ~SidePanelWebUIViewT() override = default;

  BubbleContentsWrapperT<T>* contents_wrapper() {
    return contents_wrapper_.get();
  }

 private:
  std::unique_ptr<BubbleContentsWrapperT<T>> contents_wrapper_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_WEB_UI_VIEW_H_
