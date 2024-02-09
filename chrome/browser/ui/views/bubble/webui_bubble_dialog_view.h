// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class WebView;
}  // namespace views

// A Views bubble host for a WebUIContentsWrapper.
// NOTE: The anchor rect takes precedence over the anchor view in this class.
// This is the opposite of the behaviour specified in the
// BubbleDialogDelegateView base class.
class WebUIBubbleDialogView : public views::WidgetObserver,
                              public views::BubbleDialogDelegateView,
                              public WebUIContentsWrapper::Host {
  METADATA_HEADER(WebUIBubbleDialogView, views::BubbleDialogDelegateView)

 public:
  // An optional anchor_rect can be passed to anchor the dialog to a specific
  // point on the screen. The provided anchor_rect will take precedent over the
  // anchor_view.
  WebUIBubbleDialogView(
      views::View* anchor_view,
      // Note that `contents_wrapper` has a lifetime that is unrelated
      // to this View, so it needs to reference via a WeakPtr in case
      // the contents wrapper is destroyed before `this`.
      base::WeakPtr<WebUIContentsWrapper> contents_wrapper,
      const std::optional<gfx::Rect>& anchor_rect = std::nullopt,
      views::BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_RIGHT);
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

  // WebUIContentsWrapper::Host:
  void ShowUI() override;
  void CloseUI() override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  WebUIContentsWrapper* get_contents_wrapper_for_testing() {
    return contents_wrapper_.get();
  }
  void ResetWebUIContentsForTesting();

  // TODO(ffred): This is necessary because the default behaviour of the bubble
  // dialog is that anchor view positioning takes precedent over anchor rect.
  // This will not work because the anchor rect is used to explicitly specify
  // the positioning of the bubble and the anchor view cannot be null.
  //
  // That being said, the base class should reconsider its behaviour so that
  // this type of override is not necessary.
  gfx::Rect GetAnchorRect() const override;

  virtual void Redraw() {}

 private:
  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  base::WeakPtr<WebUIContentsWrapper> contents_wrapper_;
  raw_ptr<views::WebView> web_view_;
  std::optional<gfx::Rect> bubble_anchor_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};

  base::WeakPtrFactory<WebUIBubbleDialogView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_DIALOG_VIEW_H_
