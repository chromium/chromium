// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_recommended_bubble_view.h"

#include <tuple>
#include <utility>

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

#if defined(OS_MACOSX)
#include "chrome/browser/platform_util.h"
#endif  // defined(OS_MACOSX)

// static
views::Widget* RelaunchRecommendedBubbleView::ShowBubble(
    Browser* browser,
    base::TimeTicks detection_time,
    base::RepeatingClosure on_accept) {
  DCHECK(browser);

  // Anchor the popup to the browser's app menu.
  auto* anchor_button = BrowserView::GetBrowserViewForBrowser(browser)
                            ->toolbar()
                            ->app_menu_button();
  auto* bubble_view = new RelaunchRecommendedBubbleView(
      anchor_button, gfx::Point(), detection_time, std::move(on_accept));
  bubble_view->SetArrow(views::BubbleBorder::TOP_RIGHT);

#if defined(OS_MACOSX)
  // Parent the bubble to the browser window when there is no anchor view.
  if (!anchor_button) {
    bubble_view->set_parent_window(
        platform_util::GetViewForWindow(browser->window()->GetNativeWindow()));
  }
#endif  // defined(OS_MACOSX)

  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble_view);
  bubble_view->ShowForReason(AUTOMATIC);

  return bubble_widget;
}

RelaunchRecommendedBubbleView::~RelaunchRecommendedBubbleView() = default;

bool RelaunchRecommendedBubbleView::Accept() {
  base::RecordAction(base::UserMetricsAction("RelaunchRecommended_Accept"));

  on_accept_.Run();

  // Keep the bubble open in case shutdown is prevented for some reason so that
  // the user can try again if needed.
  return false;
}

bool RelaunchRecommendedBubbleView::Close() {
  base::RecordAction(base::UserMetricsAction("RelaunchRecommended_Close"));

  return true;
}

int RelaunchRecommendedBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 RelaunchRecommendedBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK_EQ(button, ui::DIALOG_BUTTON_OK);
  return l10n_util::GetStringUTF16(IDS_RELAUNCH_ACCEPT_BUTTON);
}

base::string16 RelaunchRecommendedBubbleView::GetWindowTitle() const {
  return relaunch_recommended_timer_.GetWindowTitle();
}

bool RelaunchRecommendedBubbleView::ShouldShowCloseButton() const {
  return true;
}

gfx::ImageSkia RelaunchRecommendedBubbleView::GetWindowIcon() {
  return gfx::CreateVectorIcon(gfx::IconDescription(
      vector_icons::kBusinessIcon, kTitleIconSize, gfx::kChromeIconGrey,
      base::TimeDelta(), gfx::kNoneIcon));
}

bool RelaunchRecommendedBubbleView::ShouldShowWindowIcon() const {
  return true;
}

int RelaunchRecommendedBubbleView::GetHeightForWidth(int width) const {
  const gfx::Insets insets = GetInsets();
  return body_label_->GetHeightForWidth(width - insets.width()) +
         insets.height();
}

void RelaunchRecommendedBubbleView::Layout() {
  body_label_->SetBoundsRect(GetContentsBounds());
}

void RelaunchRecommendedBubbleView::Init() {
  body_label_ =
      new views::Label(l10n_util::GetStringUTF16(IDS_RELAUNCH_RECOMMENDED_BODY),
                       views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT);
  body_label_->SetMultiLine(true);
  body_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Align the body label with the left edge of the bubble's title.
  // TODO(bsep): Remove this when fixing https://crbug.com/810970.
  // Note: BubleFrameView applies INSETS_DIALOG_TITLE either side of the icon.
  int title_offset = 2 * views::LayoutProvider::Get()
                             ->GetInsetsMetric(views::INSETS_DIALOG_TITLE)
                             .left() +
                     kTitleIconSize;
  body_label_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, title_offset - margins().left(), 0, 0)));

  AddChildView(body_label_);

  base::RecordAction(base::UserMetricsAction("RelaunchRecommendedShown"));
}

gfx::Size RelaunchRecommendedBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void RelaunchRecommendedBubbleView::VisibilityChanged(
    views::View* starting_from,
    bool is_visible) {
  views::Button* anchor_button = views::Button::AsButton(GetAnchorView());
  if (anchor_button) {
    anchor_button->AnimateInkDrop(is_visible ? views::InkDropState::ACTIVATED
                                             : views::InkDropState::DEACTIVATED,
                                  nullptr);
  }
}

// |relaunch_recommended_timer_| automatically starts for the next time the
// title needs to be updated (e.g., from "2 days" to "3 days").
RelaunchRecommendedBubbleView::RelaunchRecommendedBubbleView(
    views::Button* anchor_button,
    const gfx::Point& anchor_point,
    base::TimeTicks detection_time,
    base::RepeatingClosure on_accept)
    : LocationBarBubbleDelegateView(anchor_button, anchor_point, nullptr),
      on_accept_(std::move(on_accept)),
      body_label_(nullptr),
      relaunch_recommended_timer_(
          detection_time,
          base::BindRepeating(&RelaunchRecommendedBubbleView::UpdateWindowTitle,
                              base::Unretained(this))) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::RELAUNCH_RECOMMENDED);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
}

void RelaunchRecommendedBubbleView::UpdateWindowTitle() {
  GetWidget()->UpdateWindowTitle();
  // This might update the length of the window title (for N days). Resize the
  // bubble to match the new preferred size.
  SizeToContents();
}
