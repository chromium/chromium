// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"

namespace send_tab_to_self {

// static
SendTabToSelfToolbarBubbleView* SendTabToSelfToolbarBubbleView::CreateBubble(
    Profile* profile,
    SendTabToSelfToolbarIconView* parent,
    const SendTabToSelfEntry& entry,
    base::OnceCallback<void(NavigateParams*)> navigate_callback) {
  SendTabToSelfToolbarBubbleView* bubble_view =
      new SendTabToSelfToolbarBubbleView(profile, parent, entry,
                                         std::move(navigate_callback));
  // The widget is owned by the views system.
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble_view);
  widget->Show();
  return bubble_view;
}

SendTabToSelfToolbarBubbleView::~SendTabToSelfToolbarBubbleView() = default;

SendTabToSelfToolbarBubbleView::SendTabToSelfToolbarBubbleView(
    Profile* profile,
    SendTabToSelfToolbarIconView* parent,
    const SendTabToSelfEntry& entry,
    base::OnceCallback<void(NavigateParams*)> navigate_callback)
    : views::BubbleDialogDelegateView(parent, views::BubbleBorder::TOP_RIGHT),
      toolbar_button_(parent),
      navigate_callback_(std::move(navigate_callback)),
      profile_(profile),
      title_(entry.GetTitle()),
      url_(entry.GetURL()),
      device_name_(entry.GetDeviceName()),
      guid_(entry.GetGUID()) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetShowCloseButton(true);
  SetTitle(
      l10n_util::GetStringUTF16(IDS_TOOLBAR_BUTTON_SEND_TAB_TO_SELF_TITLE));
  SetCloseCallback(base::BindOnce(&SendTabToSelfToolbarBubbleView::Hide,
                                  base::Unretained(this)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_close_on_deactivate(false);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  // TODO(crbug/1206381): metrics.
  auto margin = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUTTON_HORIZONTAL_PADDING);

  // Page title.
  auto title = std::make_unique<views::Label>(base::UTF8ToUTF16(title_));
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title->SetTextStyle(views::style::STYLE_PRIMARY);
  title->SetElideBehavior(gfx::ELIDE_TAIL);
  title->SetMaximumWidthSingleLine(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
      margin * 2);
  AddChildView(std::move(title));

  // Page URL.
  auto url = std::make_unique<views::Label>(
      url_formatter::FormatUrlForSecurityDisplay(url_));
  url->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  url->SetTextStyle(views::style::STYLE_SECONDARY);
  url->SetElideBehavior(gfx::ELIDE_TAIL);
  AddChildView(std::move(url));

  // Device name.
  auto device = std::make_unique<views::Label>(l10n_util::GetStringFUTF16(
      IDS_TOOLBAR_BUTTON_SEND_TAB_TO_SELF_FROM_DEVICE,
      base::UTF8ToUTF16(device_name_)));
  device->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  device->SetTextStyle(views::style::STYLE_SECONDARY);
  device->SetElideBehavior(gfx::ELIDE_TAIL);
  AddChildView(std::move(device));

  // Open in New Tab button.
  auto button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&SendTabToSelfToolbarBubbleView::OpenInNewTab,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_TOOLBAR_BUTTON_SEND_TAB_TO_SELF_BUTTON_LABEL));
  button->SetProminent(true);
  button->SetProperty(views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kEnd);
  AddChildView(std::move(button));

  base::TimeDelta kTimeoutMs = base::Milliseconds(30000);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SendTabToSelfToolbarBubbleView::Timeout,
                     weak_ptr_factory_.GetWeakPtr()),
      kTimeoutMs);
}

void SendTabToSelfToolbarBubbleView::OpenInNewTab() {
  opened_ = true;
  NavigateParams params(profile_, url_, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = NavigateParams::SHOW_WINDOW;
  std::move(navigate_callback_).Run(&params);

  GetWidget()->Close();
  toolbar_button_->LogNotificationOpened();
}

void SendTabToSelfToolbarBubbleView::Timeout() {
  send_tab_to_self::RecordNotificationTimedOut();
  GetWidget()->Close();
}

void SendTabToSelfToolbarBubbleView::Hide() {
  if (!opened_) {
    toolbar_button_->LogNotificationDismissed();
  }
  toolbar_button_->DismissEntry(guid_);
}

}  // namespace send_tab_to_self
