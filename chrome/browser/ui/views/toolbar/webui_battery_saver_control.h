// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_BATTERY_SAVER_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_BATTERY_SAVER_CONTROL_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/performance_controls/battery_saver_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/battery_saver_button_controller.h"
#include "chrome/browser/ui/performance_controls/battery_saver_button_controller_delegate.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class TrackedElement;
}  // namespace ui

class WebUIToolbarControlDelegate;

namespace views {
class BubbleDialogModelHost;
}

class WebUIBatterySaverControl : public BatterySaverButtonControllerDelegate,
                                 public BatterySaverBubbleObserver {
 public:
  explicit WebUIBatterySaverControl(WebUIToolbarControlDelegate* delegate);
  WebUIBatterySaverControl(const WebUIBatterySaverControl&) = delete;
  WebUIBatterySaverControl& operator=(const WebUIBatterySaverControl&) = delete;
  ~WebUIBatterySaverControl() override;

  void Init();

  // Attempts to show the battery saver bubble when the button, or its overflow
  // element, has been clicked on.
  //
  // * If a bubble is already visible, closes it.
  // * If the button is currently hidden (according to the ElementTracker),
  // forces it to be visible, and sets `pending_show_bubble_`, to show a bubble
  // once it's visible.
  // * If the button is visible, shows a bubble anchored to it. Preferentially
  // uses `anchor_rect` if it's set.
  void ShowBubble(std::optional<gfx::Rect> anchor_rect = std::nullopt);

  bool IsVisible() const { return is_showing_; }

  toolbar_ui_api::mojom::BatterySaverControlStatePtr CreateState() const;

  // BatterySaverButtonControllerDelegate:
  void Show() override;
  void Hide() override;

  // BatterySaverBubbleObserver:
  void OnBubbleShown() override;
  void OnBubbleHidden() override;

  views::BubbleDialogModelHost* bubble_for_testing() { return bubble_; }

 private:
  void UpdateState();
  void CloseFeaturePromo(bool engaged);

  // Invoked when the battery saver button's TrackedElement has become visible
  // while `pending_show_bubble_` is true, which is the point at which the
  // bubble can finally be anchored to it.
  void OnButtonShownWithPendingShowBubble(ui::TrackedElement* element);

  // Abandons a pending attempt to show the bubble, allowing the button to
  // overflow again.
  void CancelPendingShowBubble();

  const raw_ptr<WebUIToolbarControlDelegate> delegate_;
  BatterySaverButtonController controller_;
  raw_ptr<views::BubbleDialogModelHost> bubble_ = nullptr;

  // Indicates whether the battery saver button should be shown (i.e. whether
  // Battery Saver Mode is currently active in the browser).
  bool is_showing_;

  // True while waiting for the WebUI to display the button so that the bubble
  // can be anchored to it. Note that this deliberately has no timeout: if the
  // renderer never displays the button (e.g. because it crashed), the state
  // pushed to its replacement will still have `prevent_overflow` set, so the
  // new renderer will display the button and the bubble will be shown then.
  bool pending_show_bubble_ = false;

  // Subscription used to wait for the button to become visible. Only held while
  // `pending_show_bubble_` is true.
  base::CallbackListSubscription button_shown_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_BATTERY_SAVER_CONTROL_H_
