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
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kIdleBubbleLabelElementId);

void ShowIdleBubble(Browser* browser,
                    base::TimeDelta idle_threshold,
                    IdleDialog::ActionSet actions) {
  if (!browser || !browser->tab_strip_model()->GetActiveWebContents()) {
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
                                               ui::TimeFormat::LENGTH_LONG,
                                               idle_threshold))),
                    std::u16string(), kIdleBubbleLabelElementId);

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_builder.Build(), anchor_view, views::BubbleBorder::TOP_RIGHT);
  bubble->set_close_on_deactivate(true);

  views::BubbleDialogDelegate::CreateBubble(std::move(bubble))->Show();
}

bool IsIdleBubbleOpenForTesting(Browser* browser) {
  ui::ElementContext context = browser->window()->GetElementContext();
  return nullptr !=
         ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
             kIdleBubbleLabelElementId, context);
}
