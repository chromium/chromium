// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_

#include <memory>
#include <utility>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class GURL;
class Profile;
class WebUIBubbleDialogView;

// WebUIBubbleManager handles the creation / destruction of the WebUI bubble.
// This is needed to deal with the asynchronous presentation of WebUI.
class WebUIBubbleManager : public views::WidgetObserver {
 public:
  WebUIBubbleManager();
  WebUIBubbleManager(const WebUIBubbleManager&) = delete;
  const WebUIBubbleManager& operator=(const WebUIBubbleManager&) = delete;
  ~WebUIBubbleManager() override;

  bool ShowBubble();
  void CloseBubble();
  views::Widget* GetBubbleWidget() const;
  bool bubble_using_cached_web_contents() const {
    return bubble_using_cached_web_contents_;
  }
  virtual base::WeakPtr<WebUIBubbleDialogView> CreateWebUIBubbleDialog() = 0;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  base::WeakPtr<WebUIBubbleDialogView> bubble_view_for_testing() {
    return bubble_view_;
  }

  void ResetContentsWrapperForTesting();

 protected:
  BubbleContentsWrapper* contents_wrapper() { return contents_wrapper_.get(); }
  void set_contents_wrapper(
      std::unique_ptr<BubbleContentsWrapper> contents_wrapper) {
    contents_wrapper_ = std::move(contents_wrapper);
  }
  void set_bubble_using_cached_web_contents(bool is_cached) {
    bubble_using_cached_web_contents_ = is_cached;
  }

 private:
  void ResetContentsWrapper();

  base::WeakPtr<WebUIBubbleDialogView> bubble_view_;

  // Stores a cached BubbleContentsWrapper for reuse in the WebUIBubbleDialog.
  std::unique_ptr<BubbleContentsWrapper> contents_wrapper_;

  // Tracks whether the current bubble was created by reusing a preloaded web
  // contents.
  bool bubble_using_cached_web_contents_ = false;

  // A timer controlling how long the |cached_web_view_| is cached for.
  std::unique_ptr<base::RetainingOneShotTimer> cache_timer_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};

  // This is necessary to prevent a bug closing the active tab in the bubble.
  // See https://crbug.com/1139028.
  std::unique_ptr<CloseBubbleOnTabActivationHelper> close_bubble_helper_;
};

template <typename T>
class WebUIBubbleManagerT : public WebUIBubbleManager {
 public:
  WebUIBubbleManagerT(views::View* anchor_view,
                      Profile* profile,
                      const GURL& webui_url,
                      int task_manager_string_id,
                      bool enable_extension_apis = false)
      : anchor_view_(anchor_view),
        profile_(profile),
        webui_url_(webui_url),
        task_manager_string_id_(task_manager_string_id),
        enable_extension_apis_(enable_extension_apis) {}
  ~WebUIBubbleManagerT() override = default;

  base::WeakPtr<WebUIBubbleDialogView> CreateWebUIBubbleDialog() override {
    set_bubble_using_cached_web_contents(!!contents_wrapper());
    if (!contents_wrapper()) {
      set_contents_wrapper(std::make_unique<BubbleContentsWrapperT<T>>(
          webui_url_, profile_, task_manager_string_id_,
          enable_extension_apis_));
    }

    DCHECK(!contents_wrapper()->GetHost());
    auto bubble_view = std::make_unique<WebUIBubbleDialogView>(
        anchor_view_, contents_wrapper());
    auto weak_ptr = bubble_view->GetWeakPtr();
    views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
    return weak_ptr;
  }

 private:
  views::View* const anchor_view_;
  Profile* const profile_;
  const GURL webui_url_;
  const int task_manager_string_id_;
  const bool enable_extension_apis_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_
