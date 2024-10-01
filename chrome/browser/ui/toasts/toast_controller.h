// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_TOAST_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOASTS_TOAST_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;
class ToastRegistry;
class ToastSpecification;
enum class ToastId;

namespace content {
class Page;
}

namespace toasts {
enum class ToastCloseReason;
class ToastView;
}

namespace views {
class Widget;
}

struct ToastParams {
  explicit ToastParams(ToastId id);
  ToastParams(ToastParams&& other) noexcept;
  ToastParams& operator=(ToastParams&& other) noexcept;
  ~ToastParams();

  ToastId toast_id_;
  std::vector<std::u16string> body_string_replacement_params_;
  std::vector<std::u16string> action_button_string_replacement_params_;
};

class ToastController : public views::WidgetObserver,
                        public FullscreenObserver,
                        public OmniboxTabHelper::Observer,
                        public content::WebContentsObserver {
 public:
  explicit ToastController(BrowserWindowInterface* browser_window_interface,
                           const ToastRegistry* toast_registry);
  ~ToastController() override;

  void Init();
  bool IsShowingToast() const;
  bool CanShowToast(ToastId id) const;
  std::optional<ToastId> GetCurrentToastId() const;

  // Attempts to show the toast and returns true if the toast was successfully
  // shown, otherwise return false. Callers that show a persistent toast must
  // eventually call ClosePersistentToast() to ensure their toast closes.
  bool MaybeShowToast(ToastParams params);

  // Closes the currently showing persistent toast that must correspond to `id`.
  void ClosePersistentToast(ToastId id);

  // views::WidgetObserver:
#if BUILDFLAG(IS_MAC)
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
#endif
  void OnWidgetDestroyed(views::Widget* widget) override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // OmniboxTabHelper::Observer:
  void OnOmniboxInputStateChanged() override {}
  void OnOmniboxInputInProgress(bool in_progress) override;
  void OnOmniboxFocusChanged(OmniboxFocusState state,
                             OmniboxFocusChangeReason reason) override;
  void OnOmniboxPopupVisibilityChanged(bool popup_is_open) override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  views::Widget* GetToastWidgetForTesting() { return toast_widget_; }

  toasts::ToastView* GetToastViewForTesting() { return toast_view_; }

  base::OneShotTimer* GetToastCloseTimerForTesting();

 private:
  void OnActiveTabChanged(BrowserWindowInterface* browser_interface);
  void QueueToast(ToastParams params);
  void ShowToast(ToastParams params);
  virtual void CreateToast(const ToastParams& params,
                           const ToastSpecification* spec);
  virtual void CloseToast(toasts::ToastCloseReason reason);
  std::u16string FormatString(int string_id,
                              std::vector<std::u16string> replacement);
  void ClearTabScopedToasts();
  void UpdateToastWidgetVisibility(bool show_toast_widget);
  bool ShouldRenderToastOverWebContents();

  // FullscreenObserver:
  void OnFullscreenStateChanged() override;

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  const raw_ptr<const ToastRegistry> toast_registry_;
  std::optional<ToastParams> current_ephemeral_params_;
  std::optional<ToastParams> next_ephemeral_params_;
  std::optional<ToastParams> persistent_params_;
  std::optional<ToastId> currently_showing_toast_id_;
  base::OneShotTimer toast_close_timer_;
  bool is_omnibox_popup_showing_ = false;

  // Observer to check for browser window entering fullscreen.
  base::ScopedObservation<FullscreenController, FullscreenObserver>
      fullscreen_observation_{this};

  // Observer to check when the toast is destroyed.
  base::ScopedObservation<views::Widget, views::WidgetObserver> toast_observer_{
      this};
  base::ScopedObservation<OmniboxTabHelper, OmniboxTabHelper::Observer>
      omnibox_helper_observer_{this};

  raw_ptr<toasts::ToastView> toast_view_;
  raw_ptr<views::Widget> toast_widget_;

  std::vector<base::CallbackListSubscription> browser_subscriptions_;
};

#endif  // CHROME_BROWSER_UI_TOASTS_TOAST_CONTROLLER_H_
