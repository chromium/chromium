// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_BATTERY_SAVER_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_BATTERY_SAVER_CONTROL_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/performance_controls/battery_saver_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/battery_saver_button_controller.h"
#include "chrome/browser/ui/performance_controls/battery_saver_button_controller_delegate.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "ui/gfx/geometry/rect.h"

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

  void ShowBubble(gfx::Rect anchor_rect);

  bool IsVisible() const { return is_showing_; }

  toolbar_ui_api::mojom::BatterySaverControlStatePtr CreateState() const;

  // BatterySaverButtonControllerDelegate:
  void Show() override;
  void Hide() override;

  // BatterySaverBubbleObserver:
  void OnBubbleShown() override;
  void OnBubbleHidden() override;

 private:
  void UpdateState();
  void CloseFeaturePromo(bool engaged);

  const raw_ptr<WebUIToolbarControlDelegate> delegate_;
  BatterySaverButtonController controller_;
  raw_ptr<views::BubbleDialogModelHost> bubble_ = nullptr;

  // Indicates whether the battery saver button should be shown (i.e. whether
  // Battery Saver Mode is currently active in the browser).
  bool is_showing_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_BATTERY_SAVER_CONTROL_H_
