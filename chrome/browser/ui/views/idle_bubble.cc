// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/idle_bubble.h"

#include <string>
#include <utility>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kIdleBubbleElementId);

namespace {

// Wrapper around IdleBubbleDialogDelegate that focuses the X button on
// activation. This lets the user focus the dialog with Alt+Shift+A or F6.
class IdleBubbleDialogDelegate : public views::BubbleDialogModelHost {
 public:
  IdleBubbleDialogDelegate(std::unique_ptr<ui::DialogModel> model,
                           views::View* anchor_view,
                           views::BubbleBorder::Arrow arrow)
      : views::BubbleDialogModelHost(std::move(model), anchor_view, arrow) {}

  // views::WidgetDelegate:
  void OnWidgetInitialized() override {
    views::BubbleDialogModelHost::OnWidgetInitialized();
    if (views::BubbleFrameView* frame = GetBubbleFrameView()) {
      frame->SetProperty(views::kElementIdentifierKey, kIdleBubbleElementId);
    }
  }

  // views::BubbleDialogDelegate:
  views::View* GetInitiallyFocusedView() override {
    views::BubbleFrameView* frame = GetBubbleFrameView();
    return frame ? frame->close_button() : nullptr;
  }
};

}  // namespace

void ShowIdleBubble(Browser* browser,
                    base::TimeDelta idle_threshold,
                    IdleDialog::ActionSet actions,
                    base::OnceClosure on_close) {
  if (!browser || !browser->tab_strip_model()->GetActiveWebContents() ||
      GetIdleBubble(browser)) {
    return;
  }

  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAppMenuButton();

  int bubble_title_id =
      actions.close ? IDS_IDLE_BUBBLE_TITLE_CLOSE : IDS_IDLE_BUBBLE_TITLE_CLEAR;
  int bubble_message_id;
  if (actions.clear && actions.close) {
    bubble_message_id = IDS_IDLE_BUBBLE_BODY_CLOSE_AND_CLEAR;
  } else if (actions.clear) {
    bubble_message_id = IDS_IDLE_BUBBLE_BODY_CLEAR;
  } else {
    bubble_message_id = IDS_IDLE_BUBBLE_BODY_CLOSE;
  }

  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetTitle(l10n_util::GetStringUTF16(bubble_title_id))
      .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringFUTF16(
          bubble_message_id,
          ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                 ui::TimeFormat::LENGTH_LONG, idle_threshold))))
      .SetIsAlertDialog()
      .SetCloseActionCallback(std::move(on_close));

  auto bubble = std::make_unique<IdleBubbleDialogDelegate>(
      dialog_builder.Build(), anchor_view, views::BubbleBorder::TOP_RIGHT);
  bubble->set_close_on_deactivate(false);

  views::BubbleDialogDelegate::CreateBubble(std::move(bubble))->ShowInactive();
}

views::BubbleFrameView* GetIdleBubble(Browser* browser) {
  ui::ElementContext context = browser->window()->GetElementContext();
  return views::ElementTrackerViews::GetInstance()
      ->GetFirstMatchingViewAs<views::BubbleFrameView>(kIdleBubbleElementId,
                                                       context);
}
