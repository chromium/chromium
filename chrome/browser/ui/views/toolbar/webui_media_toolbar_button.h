// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_MEDIA_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_MEDIA_TOOLBAR_BUTTON_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_reopen_suppressor.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom-forward.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/bubble/bubble_anchor.h"

class MediaNotificationService;
class MediaToolbarButtonContextualMenu;
class MediaToolbarButtonController;
class WebUIToolbarControlDelegate;

namespace ui {
class SimpleMenuModel;
}

namespace views {
class MenuRunner;
}

// WebUIMediaToolbarButton implements C++-side functionality for the
// WebUI-based implementation of the Global Media Controls toolbar button.
class WebUIMediaToolbarButton : public MediaToolbarButtonControllerDelegate,
                                public MediaToolbarButton {
 public:
  explicit WebUIMediaToolbarButton(WebUIToolbarControlDelegate* delegate);
  WebUIMediaToolbarButton(const WebUIMediaToolbarButton&) = delete;
  WebUIMediaToolbarButton& operator=(const WebUIMediaToolbarButton&) = delete;
  ~WebUIMediaToolbarButton() override;

  void Init();

  void OnClicked(bool is_mouse_interaction);
  void OnMousePressed();
  void HandleContextMenu(const gfx::Rect& screen_rect,
                         ui::mojom::MenuSourceType source);

  // MediaToolbarButtonControllerDelegate implementation.
  void Show() override;
  void Hide() override;
  void Enable() override;
  void Disable() override;
  void MaybeShowLocalMediaCastingPromo() override;
  void MaybeShowStopCastingPromo() override;

  // MediaToolbarButton implementation.
  views::BubbleAnchor GetBubbleAnchor() override;
  MediaToolbarButtonController* GetController() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebUIMediaToolbarButtonInteractiveTest,
                           MediaButtonClickedAndRightClicked);

  void UpdateState();
  void ClosePromoBubble(bool engaged);

  const raw_ptr<WebUIToolbarControlDelegate> delegate_;

  raw_ptr<MediaNotificationService> service_ = nullptr;
  std::unique_ptr<MediaToolbarButtonController> controller_;
  std::unique_ptr<MediaToolbarButtonContextualMenu> context_menu_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Enabled status; false indicates greyed out control.
  bool enabled_ = true;
  bool should_be_shown_ = false;

  // Helper to prevent mouse clicks from immediately reopening a bubble that was
  // just closed.
  WebUIBubbleReopenSuppressor reopen_suppressor_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_MEDIA_TOOLBAR_BUTTON_H_
