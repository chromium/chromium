// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hats/hats_bubble_view.h"

#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/hats/hats_web_dialog.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

// static
HatsBubbleView* HatsBubbleView::instance_ = nullptr;

namespace {

HatsBubbleView::BubbleUsageCounts BubbleUsageCountsFromWidgetCloseReason(
    views::Widget::ClosedReason reason) {
  switch (reason) {
    case views::Widget::ClosedReason::kUnspecified:
    case views::Widget::ClosedReason::kLostFocus:
      return HatsBubbleView::BubbleUsageCounts::kIgnored;
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
      return HatsBubbleView::BubbleUsageCounts::kUIDismissed;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      return HatsBubbleView::BubbleUsageCounts::kDeclined;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      return HatsBubbleView::BubbleUsageCounts::kAccepted;
  }
}

}  // namespace

views::BubbleDialogDelegateView* HatsBubbleView::GetHatsBubble() {
  return instance_;
}

// static
void HatsBubbleView::ShowOnContentReady(Browser* browser,
                                        const std::string& site_id) {
  if (site_id == "test_site_id") {
    // Directly show the bubble during tests.
    HatsBubbleView::Show(browser, base::DoNothing());
    return;
  }

  // Try to create the web dialog and preload the HaTS survey content first.
  // The bubble will only show after the survey content is retrieved.
  // If it fails due to no internet connection or any other reason, the bubble
  // will not show.
  HatsWebDialog::Create(browser, site_id);
}

void HatsBubbleView::Show(Browser* browser,
                          HatsConsentCallback consent_callback) {
  AppMenuButton* anchor_button = BrowserView::GetBrowserViewForBrowser(browser)
                                     ->toolbar_button_provider()
                                     ->GetAppMenuButton();
  // Do not show HaTS bubble if there is no avatar menu button to anchor to.
  if (!anchor_button)
    return;

  DCHECK(anchor_button->GetWidget());
  gfx::NativeView parent_view = anchor_button->GetWidget()->GetNativeView();

  // Bubble delegate will be deleted when its window is destroyed.
  auto* bubble = new HatsBubbleView(browser, anchor_button, parent_view,
                                    std::move(consent_callback));
  bubble->SetHighlightedButton(anchor_button);
  bubble->GetWidget()->Show();
}

HatsBubbleView::HatsBubbleView(Browser* browser,
                               AppMenuButton* anchor_button,
                               gfx::NativeView parent_view,
                               HatsConsentCallback consent_callback)
    : BubbleDialogDelegateView(anchor_button, views::BubbleBorder::TOP_RIGHT),
      close_bubble_helper_(this, browser),
      consent_callback_(std::move(consent_callback)) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::HATS_BUBBLE);

  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_HATS_BUBBLE_OK_LABEL));
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   l10n_util::GetStringUTF16(IDS_NO_THANKS));
  set_close_on_deactivate(false);
  set_parent_window(parent_view);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  set_margins(
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto message = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_HATS_BUBBLE_TEXT));
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(message));

  views::BubbleDialogDelegateView::CreateBubble(this);

  instance_ = this;
}

HatsBubbleView::~HatsBubbleView() {
  // If the callback was not run before, we need to run it now.
  if (consent_callback_)
    std::move(consent_callback_).Run(false);
}

base::string16 HatsBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_HATS_BUBBLE_TITLE);
}

gfx::ImageSkia HatsBubbleView::GetWindowIcon() {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_PRODUCT_LOGO_32);
}

bool HatsBubbleView::ShouldShowWindowIcon() const {
  return true;
}

bool HatsBubbleView::Cancel() {
  if (consent_callback_)
    std::move(consent_callback_).Run(false);
  return true;
}

bool HatsBubbleView::Accept() {
  if (consent_callback_)
    std::move(consent_callback_).Run(true);
  return true;
}

bool HatsBubbleView::ShouldShowCloseButton() const {
  return true;
}

void HatsBubbleView::OnWidgetClosing(views::Widget* widget) {
  UMA_HISTOGRAM_ENUMERATION(
      "Feedback.HappinessTrackingSurvey.BubbleUsage",
      BubbleUsageCountsFromWidgetCloseReason(widget->closed_reason()));
}

void HatsBubbleView::OnWidgetDestroying(views::Widget* widget) {
  BubbleDialogDelegateView::OnWidgetDestroying(widget);
  instance_ = nullptr;
}

void HatsBubbleView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  auto* frame_view = GetBubbleFrameView();
  if (frame_view && frame_view->title()) {
    // Align bubble content to the beginning of the title text.
    gfx::Point point(frame_view->title()->x(), 0);
    views::View::ConvertPointToTarget(frame_view, GetWidget()->client_view(),
                                      &point);
    auto dialog_margins = margins();
    dialog_margins.set_left(point.x());
    set_margins(dialog_margins);
  }

  views::BubbleDialogDelegateView::OnBoundsChanged(previous_bounds);
}
