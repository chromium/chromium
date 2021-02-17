// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_DIALOG_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class WebView;
}  // namespace views

// A Views bubble host for a BubbleContentsWrapper.
class WebUIBubbleDialogView : public views::WidgetObserver,
                              public views::BubbleDialogDelegateView,
                              public BubbleContentsWrapper::Host {
 public:
  METADATA_HEADER(WebUIBubbleDialogView);

  WebUIBubbleDialogView(views::View* anchor_view,
                        BubbleContentsWrapper* contents_wrapper);
  WebUIBubbleDialogView(const WebUIBubbleDialogView&) = delete;
  WebUIBubbleDialogView& operator=(const WebUIBubbleDialogView&) = delete;
  ~WebUIBubbleDialogView() override;

  views::WebView* web_view() { return web_view_; }
  void ClearContentsWrapper();
  base::WeakPtr<WebUIBubbleDialogView> GetWeakPtr();

  // views::WidgetObserver
  void OnWidgetClosing(views::Widget* widget) override;

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  // BubbleContentsWrapper::Host:
  void ShowUI() override;
  void CloseUI() override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  BubbleContentsWrapper* get_contents_wrapper_for_testing() {
    return contents_wrapper_;
  }
  void ResetWebUIContentsForTesting();

 private:
  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  BubbleContentsWrapper* contents_wrapper_;
  views::WebView* web_view_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};

  base::WeakPtrFactory<WebUIBubbleDialogView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_DIALOG_VIEW_H_
