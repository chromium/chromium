// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper_service.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper_service_factory.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
#include "ui/base/interaction/element_identifier.h"
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

  bool ShowBubble(
      const absl::optional<gfx::Rect>& anchor = absl::nullopt,
      views::BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_RIGHT,
      ui::ElementIdentifier identifier = ui::ElementIdentifier());
  void CloseBubble();
  views::Widget* GetBubbleWidget() const;
  bool bubble_using_cached_web_contents() const {
    return bubble_using_cached_web_contents_;
  }

  // Creates the persistent renderer process if the feature is enabled.
  virtual void MaybeInitPersistentRenderer() = 0;
  virtual base::WeakPtr<WebUIBubbleDialogView> CreateWebUIBubbleDialog(
      const absl::optional<gfx::Rect>& anchor,
      views::BubbleBorder::Arrow arrow) = 0;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  base::WeakPtr<WebUIBubbleDialogView> bubble_view_for_testing() {
    return bubble_view_;
  }
  void ResetContentsWrapperForTesting();
  void DisableCloseBubbleHelperForTesting();

 protected:
  BubbleContentsWrapper* cached_contents_wrapper() {
    return cached_contents_wrapper_.get();
  }
  void set_cached_contents_wrapper(
      std::unique_ptr<BubbleContentsWrapper> cached_contents_wrapper) {
    cached_contents_wrapper_ = std::move(cached_contents_wrapper);
  }
  void set_bubble_using_cached_web_contents(bool is_cached) {
    bubble_using_cached_web_contents_ = is_cached;
  }

 private:
  void ResetContentsWrapper();

  base::WeakPtr<WebUIBubbleDialogView> bubble_view_;

  // Stores a cached BubbleContentsWrapper for reuse in the WebUIBubbleDialog.
  std::unique_ptr<BubbleContentsWrapper> cached_contents_wrapper_;

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

  // Controls whether `close_bubble_helper_` is set when ShowBubble() is called.
  bool disable_close_bubble_helper_ = false;
};

template <typename T>
class WebUIBubbleManagerT : public WebUIBubbleManager {
 public:
  WebUIBubbleManagerT(views::View* anchor_view,
                      Profile* profile,
                      const GURL& webui_url,
                      int task_manager_string_id)
      : anchor_view_(anchor_view),
        profile_(profile),
        webui_url_(webui_url),
        task_manager_string_id_(task_manager_string_id) {}
  ~WebUIBubbleManagerT() override = default;

  void MaybeInitPersistentRenderer() override {
    if (base::FeatureList::IsEnabled(
            features::kWebUIBubblePerProfilePersistence)) {
      auto* service =
          BubbleContentsWrapperServiceFactory::GetForProfile(profile_, true);
      if (service && !service->GetBubbleContentsWrapperFromURL(webui_url_)) {
        service->template InitBubbleContentsWrapper<T>(webui_url_,
                                                       task_manager_string_id_);
      }
    }
  }

  base::WeakPtr<WebUIBubbleDialogView> CreateWebUIBubbleDialog(
      const absl::optional<gfx::Rect>& anchor,
      views::BubbleBorder::Arrow arrow) override {
    BubbleContentsWrapper* contents_wrapper = nullptr;

    // Only use per profile peristence if the flag is set and if a
    // BubbleContentsWrapperService exists for the current profile. The service
    // may not exist for off the record profiles.
    auto* service =
        BubbleContentsWrapperServiceFactory::GetForProfile(profile_, true);
    if (service && base::FeatureList::IsEnabled(
                       features::kWebUIBubblePerProfilePersistence)) {
      set_bubble_using_cached_web_contents(true);

      // If using per-profile WebContents persistence get the associated
      // BubbleContentsWrapper from the BubbleContentsWrapperService.
      MaybeInitPersistentRenderer();
      contents_wrapper = service->GetBubbleContentsWrapperFromURL(webui_url_);
      DCHECK(contents_wrapper);

      // If there is a host currently associated to this contents wrapper ensure
      // the host has closed and the association has been removed.
      if (contents_wrapper->GetHost())
        contents_wrapper->CloseUI();
      DCHECK(!contents_wrapper->GetHost());

      // If the wrapped WebContents has crashed ensure we reload it here before
      // passing it over to the dialog host.
      if (contents_wrapper->web_contents()->IsCrashed())
        contents_wrapper->ReloadWebContents();
    } else {
      set_bubble_using_cached_web_contents(!!cached_contents_wrapper());

      if (!cached_contents_wrapper()) {
        set_cached_contents_wrapper(std::make_unique<BubbleContentsWrapperT<T>>(
            webui_url_, profile_, task_manager_string_id_));
        cached_contents_wrapper()->ReloadWebContents();
      }

      contents_wrapper = cached_contents_wrapper();
    }

    auto bubble_view = std::make_unique<WebUIBubbleDialogView>(
        anchor_view_, contents_wrapper, anchor, arrow);
    auto weak_ptr = bubble_view->GetWeakPtr();
    views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
    return weak_ptr;
  }

 private:
  const raw_ptr<views::View> anchor_view_;
  const raw_ptr<Profile, DanglingUntriaged> profile_;
  const GURL webui_url_;
  const int task_manager_string_id_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_
