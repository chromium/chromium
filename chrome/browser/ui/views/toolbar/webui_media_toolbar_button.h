// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_MEDIA_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_MEDIA_TOOLBAR_BUTTON_H_

#include <memory>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom-forward.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/widget/widget_observer.h"

class MediaNotificationService;
class MediaToolbarButtonContextualMenu;
class MediaToolbarButtonController;
class WebUIToolbarControlDelegate;

namespace ui {
class SimpleMenuModel;
class TrackedElement;
}  // namespace ui

namespace views {
class MenuRunner;
class Widget;
}  // namespace views

// WebUIMediaToolbarButton implements C++-side functionality for the
// WebUI-based implementation of the Global Media Controls toolbar button.
class WebUIMediaToolbarButton : public MediaToolbarButtonControllerDelegate,
                                public MediaToolbarButton,
                                public views::WidgetObserver {
 public:
  explicit WebUIMediaToolbarButton(WebUIToolbarControlDelegate* delegate);
  WebUIMediaToolbarButton(const WebUIMediaToolbarButton&) = delete;
  WebUIMediaToolbarButton& operator=(const WebUIMediaToolbarButton&) = delete;
  ~WebUIMediaToolbarButton() override;

  void Init();

  // Should be invoked when either the button or its overflow menu item is
  // clicked. Toggles the media dialog. If the button is currently hidden (e.g.,
  // because it's overflowed), forces it to be displayed, and shows the dialog
  // once it is. The renderer is responsible for not invoking this for mouse
  // clicks that just closed the dialog, so that it isn't immediately reopened.
  void OnClicked();
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

  // views::WidgetObserver implementation.
  void OnWidgetDestroying(views::Widget* widget) override;

  bool IsButtonShowing() const { return should_be_shown_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(WebUIMediaToolbarButtonInteractiveTest,
                           MediaButtonClickedAndRightClicked);

  // Shows the media dialog anchored to the button. If the button is currently
  // hidden, instead forces it to be displayed, and sets `pending_show_bubble_`,
  // to show the dialog once it's visible. Unlike OnClicked(), never closes the
  // dialog.
  void ShowBubble();

  // Invoked when the media button's TrackedElement has become visible while
  // `pending_show_bubble_` is true, which is the point at which the dialog can
  // finally be anchored to it.
  void OnButtonShownWithPendingShowBubble(ui::TrackedElement* element);

  // Abandons a pending attempt to show the dialog, allowing the button to
  // overflow again.
  void CancelPendingShowBubble();

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

  // True while waiting for the WebUI to display the button so that the dialog
  // can be anchored to it. Deliberately has no timeout: if the renderer never
  // displays the button (e.g. because it crashed), the state pushed to its
  // replacement will still have `prevent_overflow` set, so the new renderer
  // will display the button and the dialog will be shown then.
  bool pending_show_bubble_ = false;

  // Subscription used to wait for the button to become visible. Only held while
  // `pending_show_bubble_` is true.
  base::CallbackListSubscription button_shown_subscription_;

  // Observes the widget of the media dialog most recently shown by this
  // button, for as long as it's alive, so the button can be kept displayed
  // while the dialog is anchored to it. Note that MediaDialogView::IsShowing()
  // can't be used for this, since there is only one media dialog per process,
  // so it may belong to another browser window.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      dialog_widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_MEDIA_TOOLBAR_BUTTON_H_
